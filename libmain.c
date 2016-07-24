/**
 * @file libmain.c
 * @brief PTP Boundary Clock library main file
 * @note Copyright (C) 2011 Richard Cochran <richardcochran@gmail.com>
 * @note Modified by Delio Brignoli <brignoli.delio@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/net_tstamp.h>

#include "clock.h"
#include "config.h"
#include "pi.h"
#include "print.h"
#include "raw.h"
#include "sk.h"
#include "transport.h"
#include "udp6.h"
#include "uds.h"
#include "util.h"
#include "version.h"

int assume_two_step = 0;

static struct config cfg_settings = {
	.dds = {
		.dds = {
			.flags = DDS_TWO_STEP_FLAG,
			.priority1 = 128,
			.clockQuality.clockClass = 248,
			.clockQuality.clockAccuracy = 0xfe,
			.clockQuality.offsetScaledLogVariance = 0xffff,
			.priority2 = 128,
			.domainNumber = 0,
		},
		.free_running = 0,
		.freq_est_interval = 1,
		.grand_master_capable = 1,
		.stats_interval = 0,
		.kernel_leap = 1,
		.sanity_freq_limit = 200000000,
		.time_source = INTERNAL_OSCILLATOR,
		.clock_desc = {
			.productDescription = {
				.max_symbols = 64,
				.text = ";;",
				.length = 2,
			},
			.revisionData = {
				.max_symbols = 32,
				.text = ";;",
				.length = 2,
			},
			.userDescription      = { .max_symbols = 128 },
			.manufacturerIdentity = { 0, 0, 0 },
		},
		.delay_filter = FILTER_MOVING_MEDIAN,
		.delay_filter_length = 10,
	},

	.pod = {
		.logAnnounceInterval = 1,
		.logSyncInterval = 0,
		.logMinDelayReqInterval = 0,
		.logMinPdelayReqInterval = 0,
		.announceReceiptTimeout = 3,
		.syncReceiptTimeout = 0,
		.transportSpecific = 0,
		.announce_span = 1,
		.path_trace_enabled = 0,
		.follow_up_info = 0,
		.freq_est_interval = 1,
		/* Default to very a large neighborPropDelay threshold */
		.neighborPropDelayThresh = 20000000,
		.min_neighbor_prop_delay = -20000000,
		.foreign_master_threshold = 2,
	},

	.timestamping = TS_HARDWARE,
	.dm = DM_E2E,
	.transport = TRANS_UDP_IPV4,

	.assume_two_step = &assume_two_step,
	.tx_timestamp_timeout = &sk_tx_timeout,
	.check_fup_sync = &sk_check_fupsync,

	.clock_servo = CLOCK_SERVO_PI,

	.step_threshold = &servo_step_threshold,
	.first_step_threshold = &servo_first_step_threshold,
	.max_frequency = &servo_max_frequency,

	.pi_proportional_const = &configured_pi_kp,
	.pi_integral_const = &configured_pi_ki,
	.pi_proportional_scale = &configured_pi_kp_scale,
	.pi_proportional_exponent = &configured_pi_kp_exponent,
	.pi_proportional_norm_max = &configured_pi_kp_norm_max,
	.pi_integral_scale = &configured_pi_ki_scale,
	.pi_integral_exponent = &configured_pi_ki_exponent,
	.pi_integral_norm_max = &configured_pi_ki_norm_max,

	.ptp_dst_mac = ptp_dst_mac,
	.p2p_dst_mac = p2p_dst_mac,
	.udp6_scope = &udp6_scope,
	.uds_address = uds_path,

	.print_level = LOG_INFO,
	.use_syslog = 1,
	.verbose = 0,

	.cfg_ignore = 0,
};

struct clock *ptp4l_setup(char *config)
{
	char *req_phc = NULL;
	int c, i;
	struct interface *iface = cfg_settings.iface;
	char *ports[MAX_PORTS];
	int nports = 0;
	enum timestamp_type *timestamping = &cfg_settings.timestamping;
	struct clock *clock;
	struct defaultDS *ds = &cfg_settings.dds.dds;
	int phc_index = -1, required_modes = 0;

	/* Set fault timeouts to a default value */
	for (i = 0; i < FT_CNT; i++) {
		cfg_settings.pod.flt_interval_pertype[i].type = FTMO_LOG2_SECONDS;
		cfg_settings.pod.flt_interval_pertype[i].val = 4;
	}

	if (config && (c = config_read(config, &cfg_settings))) {
		goto exit_err;
	}
	if (!cfg_settings.dds.grand_master_capable &&
	    ds->flags & DDS_SLAVE_ONLY) {
		fprintf(stderr,
			"Cannot mix 1588 slaveOnly with 802.1AS !gmCapable.\n");
		goto exit_err;
	}
	if (!cfg_settings.dds.grand_master_capable ||
	    ds->flags & DDS_SLAVE_ONLY) {
		ds->clockQuality.clockClass = 255;
	}

	print_set_verbose(cfg_settings.verbose);
	print_set_syslog(cfg_settings.use_syslog);
	print_set_level(cfg_settings.print_level);

	for (i = 0; i < nports; i++) {
		if (config_create_interface(ports[i], &cfg_settings) < 0) {
			fprintf(stderr, "too many interfaces\n");
			goto exit_err;
		}
	}

	if (!cfg_settings.nports) {
		fprintf(stderr, "no interface specified\n");
		goto exit_err;
	}

	if (!(ds->flags & DDS_TWO_STEP_FLAG)) {
		switch (*timestamping) {
		case TS_SOFTWARE:
		case TS_LEGACY_HW:
			fprintf(stderr, "one step is only possible "
				"with hardware time stamping\n");
			goto exit_err;
		case TS_HARDWARE:
			*timestamping = TS_ONESTEP;
			break;
		case TS_ONESTEP:
			break;
		}
	}

	switch (*timestamping) {
	case TS_SOFTWARE:
		required_modes |= SOF_TIMESTAMPING_TX_SOFTWARE |
			SOF_TIMESTAMPING_RX_SOFTWARE |
			SOF_TIMESTAMPING_SOFTWARE;
		break;
	case TS_LEGACY_HW:
		required_modes |= SOF_TIMESTAMPING_TX_HARDWARE |
			SOF_TIMESTAMPING_RX_HARDWARE |
			SOF_TIMESTAMPING_SYS_HARDWARE;
		break;
	case TS_HARDWARE:
	case TS_ONESTEP:
		required_modes |= SOF_TIMESTAMPING_TX_HARDWARE |
			SOF_TIMESTAMPING_RX_HARDWARE |
			SOF_TIMESTAMPING_RAW_HARDWARE;
		break;
	}

	/* check whether timestamping mode is supported. */
	for (i = 0; i < cfg_settings.nports; i++) {
		if (iface[i].ts_info.valid &&
		    ((iface[i].ts_info.so_timestamping & required_modes) != required_modes)) {
			fprintf(stderr, "interface '%s' does not support "
				        "requested timestamping mode.\n",
				iface[i].name);
			goto exit_err;
		}
	}

	/* determine PHC Clock index */
	if (cfg_settings.dds.free_running) {
		phc_index = -1;
	} else if (*timestamping == TS_SOFTWARE || *timestamping == TS_LEGACY_HW) {
		phc_index = -1;
	} else if (req_phc) {
		if (1 != sscanf(req_phc, "/dev/ptp%d", &phc_index)) {
			fprintf(stderr, "bad ptp device string\n");
			goto exit_err;
		}
	} else if (iface[0].ts_info.valid) {
		phc_index = iface[0].ts_info.phc_index;
	} else {
		fprintf(stderr, "ptp device not specified and\n"
			        "automatic determination is not\n"
			        "supported. please specify ptp device\n");
		goto exit_err;
	}

	if (phc_index >= 0) {
		pr_info("selected /dev/ptp%d as PTP clock", phc_index);
	}

	if (generate_clock_identity(&ds->clockIdentity, iface[0].name)) {
		fprintf(stderr, "failed to generate a clock identity\n");
		goto exit_err;
	}

	clock = clock_create(phc_index, iface, cfg_settings.nports,
			     *timestamping, &cfg_settings.dds,
			     cfg_settings.clock_servo);
	if (!clock) {
		fprintf(stderr, "failed to create a clock\n");
		goto exit_err;
	}

	return clock;

exit_err:
	return NULL;
}

void ptp4l_destroy(struct clock *clock)
{
	clock_destroy(clock);
}
