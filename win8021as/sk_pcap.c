/**
 * @file sk.c
 * @brief Implements protocol independent socket methods.
 * @note Copyright (C) 2012 Richard Cochran <richardcochran@gmail.com>
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

#define USE_RTX 1

/* pcap.h needs to come before winsock2.h */
#include <pcap.h>

#include <winsock2.h>
#include <windows.h>
#include <WinIoCtl.h>
#include <assert.h>

#include <stdlib.h>
#include <stdint.h>
#if USE_RTX
#include <rtapi.h>
#endif
#include <stdio.h>
#include <string.h>

#include <iphlpapi.h>
//#include "msvc_time.h"
#include "msvc_syscall.h"
#include "transport.h"

#if USE_RTX
#include "RtDriver.h"
#include "Drvutl.h"
#include "RtI210_ptp.h"
#endif


/* from linux/socket.h*/
#define MSG_ERRQUEUE    0x2000		/* Fetch message from error queue */

/* globals */

struct netif {
	pcap_t *pcap_interface;
	char mac[6];
	char name[128];
	const u_char *ethernet_frame;
};


static pcap_if_t *alldevs;
static char errbuf[PCAP_ERRBUF_SIZE];
struct netif local_if = {0};

int sk_tx_retries = 2, sk_prefer_layer2 = 0;
static struct netif *net_if;
static LARGE_INTEGER timer_frequency;
static uint16_t rx_length = 0;
static uint8_t *rx_frame;
static struct hw_timestamp rx_hwts;
extern int running;

/* private methods */

static int hwts_init(int fd, char *device, int gptp_mode)
{
	return 0;
}

/* public methods */

int sk_interface_index(int fd, char *name)
{
	return 1;
}

/* programmable hardware clock i/f */
int sk_interface_phc(char *name, int *index)
{
	return 1;
}

/* open */
int sk_pcap_open(char *name)
{
	IP_ADAPTER_INFO *AdapterInfo; 
	IP_ADAPTER_INFO *Current;
	ULONG AIS;
	DWORD status;

	pcap_if_t *d;
	int i;
	int total_interfaces = 0;

	if (pcap_findalldevs(&alldevs, errbuf) == -1) {
		fprintf(stderr, "Error finding interfaces\n");
		goto error_return;
	}

	/* count the interfaces */
	for(d = alldevs; d; d = d->next)
		total_interfaces++;

	i = 0;
	for(d = alldevs; d; d = d->next) {
		if (strstr(d->name, name)) {
			break;
		}
	}

	if (!d)
		goto error_return;


	if ( (local_if.pcap_interface = pcap_open_live(d->name,          // name of the device
                              65536,            // portion of the packet to capture
                                                // 65536 guarantees that the whole packet will be captured on all the link layers
                              0,			// NOT promiscuous mode 
                              100,             // read timeout in ms
                              errbuf            // error buffer
                              ) ) == NULL) {
		fprintf(stderr,"\nUnable to open the adapter. %s is not supported by WinPcap\n", d->name);
		/* Free the device list */
		pcap_freealldevs(alldevs);
		goto error_return;
	}

	/* lookup ip address */
	AdapterInfo = (IP_ADAPTER_INFO *)calloc(total_interfaces, sizeof(IP_ADAPTER_INFO));
	AIS = sizeof(IP_ADAPTER_INFO) * total_interfaces;
	status = GetAdaptersInfo(AdapterInfo, &AIS);
	if (status != ERROR_SUCCESS) {
	        fprintf(stderr,"\nError, GetAdaptersInfo() call in netif_win32_pcap.c failed\n");
		free(AdapterInfo);
		goto error_return;
	}

	for(Current = AdapterInfo; Current != NULL; Current = Current->Next) {
	        if(strstr(d->name, Current->AdapterName) != 0) {
			uint32_t my_ip;
			ULONG len;
			uint8_t tmp[16];

			my_ip = inet_addr(Current->IpAddressList.IpAddress.String);
			len = sizeof(tmp);
			SendARP(my_ip ,INADDR_ANY, tmp, &len);
			for (i = 0; i < 6; i++)
				local_if.mac[i] = tmp[i];
		}
	}
	free(AdapterInfo);

	return 0;

error_return:
	return -1;
}

/* given name, lookup MAC address */
int sk_interface_macaddr(char *name, unsigned char *mac, int len)
{
	int i;

	if (!local_if.pcap_interface)
		sk_pcap_open(name);
	for (i = 0; i < 6; i++)
		mac[i] = local_if.mac[i];

	return 0;
}

int sk_net_interface_macaddr(char *name, unsigned char *mac, int len)
{
	return sk_interface_macaddr(name, mac, len);
}

int sk_net_interface_phc(char *name, int *index)
{
	*index = 1;
	return 0;
}


int sk_send(int fd, void *buf, int buflen, int flags)
{
	if (pcap_sendpacket(local_if.pcap_interface, (uint8_t *)buf, buflen ) != 0){
		fprintf(stderr,"\nError sending the packet: \n", pcap_geterr(net_if->pcap_interface));
		return 0;
	}
	msvc_time_gettime(0, &rx_hwts.ts);
	return buflen;
}

int sk_receive(int fd, void *buf, int buflen,
	       struct hw_timestamp *hwts, int flags)
{
	struct timespec *ts = NULL;

	if (flags == MSG_ERRQUEUE) {
		hwts->ts = rx_hwts.ts;
		return buflen;
	}

	assert(buflen > rx_length);

	memcpy(buf, rx_frame, rx_length);
	hwts->ts = rx_hwts.ts;

	return rx_length;
}

int sk_timestamping_init(int fd, char *device, enum timestamp_type type,
				int gptp_mode)
{
	struct bpf_program fcode;

	if (!local_if.pcap_interface)
		sk_pcap_open(device);

	// compile a filter
	if (pcap_compile(local_if.pcap_interface, &fcode, "ether proto 0x88f7", 1, 0) < 0) {
		fprintf(stderr,"\nUnable to compile the packet filter. Check the syntax.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
    
	//set the filter
	if (pcap_setfilter(local_if.pcap_interface, &fcode) < 0) {
		fprintf(stderr,"\nError setting the filter.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
	return 0;

	QueryPerformanceFrequency(&timer_frequency);
	return 0;
}

int poll(struct pollfd *fds, size_t nfds, int timeout)
{
	size_t i;
	int status;
	int event_found = 0;

	/*
	 * Reset all events.
	 */
	for (i = 0; i < nfds; i++){
		fds[i].revents = 0;
	}

	/*
	 * Poll wpcap and check timers.
	 */

	while (!event_found && running) {
		struct pcap_pkthdr *header;

		status = pcap_next_ex( local_if.pcap_interface, &header, &rx_frame);
		if(status > 0 ) {
			local_if.ethernet_frame = rx_frame;
			rx_length = (uint16_t)header->len;
		}
		
		if (status > 0) {
			msvc_time_gettime(0, &rx_hwts.ts);
			fds[0].revents = POLLIN;
			event_found++;
		}
		for(i = 0; i < nfds; i++) {
			if (fds[i].fd == -1)
				continue;
			if (fds[i].fd >= 0x1000)
				continue;
			if (syscall_timer_timeout(fds[i].fd)) {
				fds[i].revents = POLLIN;
				event_found++;
				syscall_timer_restart(fds[i].fd);
			}
		}
	}
	return event_found;
}
