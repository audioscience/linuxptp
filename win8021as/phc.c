/**
 * @file clock.c
 * @note Copyright (C) 2011 Richard Cochran <richardcochran@gmail.com>
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
#include "msvc_poll.h"
#include <stdlib.h>
#include <string.h>
#include "msvc_time.h"

#include "bmc.h"
#include "clock.h"
#include "foreign.h"
#include "mave.h"
#include "missing.h"
#include "msg.h"
#include "phc.h"

clockid_t phc_open(char *phc)
{
	return 1;
}

void phc_close(clockid_t clkid)
{
}

int phc_max_adj(clockid_t clkid)
{
	return 2;
}