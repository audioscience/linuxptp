/**
 * @file msvc_syscall.h
 * @note Copyright (C) 2012 Andrew Elder <aelder@audiocience.com>
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <Windows.h>

#include "msvc_syscall.h"
#include "msvc_time.h"

#define MAX_TIMERS 16

struct avb_timer {
	int index;
	int running;
	int elapsed;
	unsigned int count;
	unsigned int interval;
	unsigned int start_time;
};

static struct avb_timer timers[MAX_TIMERS];
static int timer_count = 0;
LARGE_INTEGER clock_monotonic_freq = {0, 0};

static unsigned int clock_monotonic_in_ms(void)
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return (unsigned int)((count.QuadPart * 1000 / clock_monotonic_freq.QuadPart) & 0xffffffff);
}

int syscall(int number, ...)
{
	va_list argp;
	int index, flags;
	const struct itimerspec *new_value;
	struct itimerspec *old_value;
	struct avb_timer *t;

	va_start(argp, number);

	if (clock_monotonic_freq.QuadPart == 0) {
		QueryPerformanceFrequency(&clock_monotonic_freq);
	}

	switch(number) {
	case __NR_timerfd_create:
		if ((timer_count + 1) == MAX_TIMERS)
			return -1;
		index = va_arg(argp, int);
		flags = va_arg(argp, int);
		memset( &timers[timer_count], 0, sizeof(struct avb_timer));
		timers[timer_count].index = index;
		timer_count++;
		return timer_count - 1;
	case __NR_timerfd_settime:
		index = va_arg(argp, int);
		flags = va_arg(argp, int);
		new_value =  va_arg(argp, const struct itimerspec *);
		old_value =  va_arg(argp, struct itimerspec *);
		if (old_value != NULL) {
			assert(0);
		}
		t = &timers[index];
		t->count = new_value->it_value.tv_sec * 1000 + new_value->it_value.tv_nsec / 1000000;
		t->interval = new_value->it_interval.tv_sec * 1000 + new_value->it_interval.tv_nsec / 1000000;
		if ((t->count == 0) && (t->interval != 0))
			t->count = t->interval;
		t->start_time = clock_monotonic_in_ms();
		if (t->count)
			t->running = TRUE;
		t->elapsed = FALSE;
		return 0;
		break;
	default:
		return -1;
	}
	
	return 0;
}

int syscall_timer_timeout(int h)
{
	struct avb_timer *t;

	t = &timers[h];
	if (t->running && !t->elapsed) {
		unsigned int elapsed_ms;
		unsigned int current_time = clock_monotonic_in_ms();
		elapsed_ms = current_time - t->start_time;
		if( elapsed_ms > t->count) {
			//printf("ELAPESD Timer %d, index %d, count %d, current %d\n", h, t->index, t->count, current_time);
			t->elapsed = TRUE;
		}
	}
	return t->elapsed;
}

int syscall_timer_restart(int h)
{
	struct avb_timer *t;

	t = &timers[h];
	if (t->interval) {
		t->elapsed = FALSE;
		t->start_time += t->interval;
	}
	return 0;
}
