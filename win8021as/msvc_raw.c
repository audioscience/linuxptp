/**
 * @file msvc_raw.c
 * @note Copyright (C) 2012 Richard Cochran <richardcochran@gmail.com>
 * @note Copyright (C) 2012 AudioScience, Inc <support@audioscience.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <winsock2.h>
#include <iphlpapi.h>

#include "contain.h"
#include "print.h"
#include "ether.h"
#include "transport_private.h"
#include "sk.h"

/* from linux/if_ether.h */
#define ETH_ALEN        6               /* Octets in one ethernet addr   */
#define ETH_P_ALL       0x0003          /* Every packet (be careful!!!) */
#define ETH_P_1588      0x88F7          /* IEEE 1588 Timesync */

/* from linux/socket.h*/
#define MSG_ERRQUEUE    0x2000		/* Fetch message from error queue */

int sk_send(int fd, void *buf, int buflen, int flags);

struct raw {
	struct transport t;
	struct eth_addr ptp_addr;
	struct eth_addr p2p_addr;
	int gptp_mode;
};

struct sockaddr_ll {
        unsigned short  sll_family;
        uint16_t        sll_protocol;
        int             sll_ifindex;
        unsigned short  sll_hatype;
        unsigned char   sll_pkttype;
        unsigned char   sll_halen;
        unsigned char   sll_addr[8];
 };

#if 0
#define OP_AND  (BPF_ALU | BPF_AND | BPF_K)
#define OP_JEQ  (BPF_JMP | BPF_JEQ | BPF_K)
#define OP_LDB  (BPF_LD  | BPF_B   | BPF_ABS)
#define OP_LDH  (BPF_LD  | BPF_H   | BPF_ABS)
#define OP_RETK (BPF_RET | BPF_K)

#define PTP_GEN_BIT 0x08 /* indicates general message, if set in message type */

#define N_RAW_FILTER    7
#define RAW_FILTER_TEST 4

static struct sock_filter raw_filter[N_RAW_FILTER] = {
	{OP_LDH,  0, 0, OFF_ETYPE   },
	{OP_JEQ,  0, 4, ETH_P_1588  }, /*f goto reject*/
	{OP_LDB,  0, 0, ETH_HLEN    },
	{OP_AND,  0, 0, PTP_GEN_BIT }, /*test general bit*/
	{OP_JEQ,  0, 1, 0           }, /*0,1=accept event; 1,0=accept general*/
	{OP_RETK, 0, 0, 1500        }, /*accept*/
	{OP_RETK, 0, 0, 0           }, /*reject*/
};
#endif

static int raw_configure(int fd, int event, int index,
			 unsigned char *addr1, unsigned char *addr2, int enable)
{
	return 1;
}

static int raw_close(struct transport *t, struct fdarray *fda)
{
	fda->fd[0] = -1;
	fda->fd[1] = -1;
	return 0;
}

unsigned char ptp_dst_mac[ETH_ALEN] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
unsigned char p2p_dst_mac[ETH_ALEN] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};

static int open_socket(char *name, int event)
{
	int fd, index;

	fd = 0x1000 + event;
	return fd;
}

static int raw_open(struct transport *t, char *name,
		    struct fdarray *fda, enum timestamp_type ts_type)
{
	struct raw *raw = container_of(t, struct raw, t);
	int efd, gfd;

	memcpy(raw->ptp_addr.dst, ptp_dst_mac, MAC_LEN);
	memcpy(raw->p2p_addr.dst, p2p_dst_mac, MAC_LEN);

	if (sk_net_interface_macaddr(name, raw->ptp_addr.src, MAC_LEN))
		goto no_mac;

	memcpy(raw->p2p_addr.src, raw->ptp_addr.src, MAC_LEN);

	efd = open_socket(name, 1);
	if (efd < 0)
		goto no_event;

	gfd = open_socket(name, 0);
	if (gfd < 0)
		goto no_general;

	if (sk_timestamping_init(efd, name, ts_type))
		goto no_timestamping;

	fda->fd[FD_EVENT] = efd;
	fda->fd[FD_GENERAL] = gfd;
	return 0;

no_timestamping:
	//close(gfd);
no_general:
	//close(efd);
no_event:
no_mac:
	return -1;
}

static int raw_recv(struct transport *t, int fd, void *buf, int buflen,
		    struct hw_timestamp *hwts)
{
	unsigned char *ptr = buf;
	ptr    -= sizeof(struct eth_hdr);
	buflen += sizeof(struct eth_hdr);
	return sk_receive(fd, ptr, buflen, hwts, 0);
}

static int raw_send(struct transport *t, struct fdarray *fda, int event, int peer,
		    void *buf, int len, struct hw_timestamp *hwts)
{
	struct raw *raw = container_of(t, struct raw, t);
	size_t cnt;
	int fd = event ? fda->fd[FD_EVENT] : fda->fd[FD_GENERAL];
	unsigned char pkt[1600], *ptr = buf;
	struct eth_hdr *hdr;

	ptr -= sizeof(*hdr);
	len += sizeof(*hdr);

	hdr = (struct eth_hdr *) ptr;
	if (peer)
		memcpy(&hdr->mac, &raw->p2p_addr, sizeof(hdr->mac));
	else
		memcpy(&hdr->mac, &raw->ptp_addr, sizeof(hdr->mac));

	hdr->type = htons(ETH_P_1588);

	cnt = sk_send(fd, ptr, len, 0);
	if (cnt < 1) {
		pr_err("send failed: %d %m", errno);
		return cnt;
	}
	/*
	 * Get the time stamp right away.
	 */
	return event ? sk_receive(fd, pkt, len, hwts, MSG_ERRQUEUE) : cnt;
}

static void raw_release(struct transport *t)
{
	struct raw *raw = container_of(t, struct raw, t);
	free(raw);
}

struct transport *raw_transport_create(int gptp_mode)
{
	struct raw *raw;
	raw = (struct raw *)calloc(1, sizeof(*raw));
	if (!raw)
		return NULL;
	raw->t.close   = raw_close;
	raw->t.open    = raw_open;
	raw->t.recv    = raw_recv;
	raw->t.send    = raw_send;
	raw->t.release = raw_release;
	raw->gptp_mode = gptp_mode;
	return &raw->t;
}
