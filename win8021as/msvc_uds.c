/**
 * @file uds.c
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>


#include <unistd.h>

#include "contain.h"
#include "print.h"
#include "transport_private.h"
#include "uds.h"

struct uds {
	struct transport t;
	//struct sockaddr_un sa;
	//socklen_t len;
};

static int uds_close(struct transport *t, struct fdarray *fda)
{
	return 0;
}

static int uds_open(struct transport *t, char *name, struct fdarray *fda,
		    enum timestamp_type tt)
{
	return 0;
}

static int uds_recv(struct transport *t, int fd, void *buf, int buflen,
		    struct hw_timestamp *hwts)
{
	return 0;
}

static int uds_send(struct transport *t, struct fdarray *fda, int event,
		    int peer, void *buf, int buflen, struct hw_timestamp *hwts)
{
	return buflen;
}

static void uds_release(struct transport *t)
{
}

struct transport *uds_transport_create(void)
{
	struct uds *uds;
	uds = (struct uds *)calloc(1, sizeof(*uds));
	if (!uds)
		return NULL;
	uds->t.close   = uds_close;
	uds->t.open    = uds_open;
	uds->t.recv    = uds_recv;
	uds->t.send    = uds_send;
	uds->t.release = uds_release;
	return &uds->t;
}

