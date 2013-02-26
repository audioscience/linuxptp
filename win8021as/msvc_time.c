/**
 * @file msvc_time.c
 * @note Copyright (C) 2012 AudioScience, Inc
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
#include <winsock2.h>
#include <malloc.h>
#include <string.h>
#include "msvc_time.h"
#include "msvc_syscall.h"

extern LARGE_INTEGER clock_monotonic_freq;

int msvc_time_gettime(clockid_t clk_id, struct timespec *tp)
{
	LARGE_INTEGER count;

	QueryPerformanceCounter(&count);

	if (clock_monotonic_freq.QuadPart == 0) {
		QueryPerformanceFrequency(&clock_monotonic_freq);
	}

	tp->tv_sec = (long)
		(count.QuadPart / clock_monotonic_freq.QuadPart);
	tp->tv_nsec = (long)
		((count.QuadPart - tp->tv_sec * clock_monotonic_freq.QuadPart) * 
			1000000000/clock_monotonic_freq.QuadPart);
	return 0;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	return msvc_time_gettime(clk_id, tp);
}
