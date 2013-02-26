#ifndef _MSVC_TIME_H_
#define _MSVC_TIME_H_

#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC

#ifndef _WINSOCKAPI_
struct timeval {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
};
#endif

struct timespec {
	long	tv_sec;                 /* seconds */
	long	tv_nsec;                /* nanoseconds */
};

struct itimerspec {
	struct timespec it_interval;  /* Timer interval */
	struct timespec it_value;     /* Initial expiration */
};

#endif

/*
 * The IDs of the various system clocks (for POSIX.1b interval timers):
 */
#define CLOCK_REALTIME                  0
#define CLOCK_MONOTONIC                 1
#define CLOCK_PROCESS_CPUTIME_ID        2
#define CLOCK_THREAD_CPUTIME_ID         3
#define CLOCK_MONOTONIC_RAW             4

typedef unsigned int clockid_t;

int msvc_time_gettime(clockid_t clk_id, struct timespec *tp);
int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif