#ifndef _MSVC_SYSCALL_H_
#define _MSVC_SYSCALL_H_

#define __NR_clock_adjtime (1)
#define __NR_timerfd_create (2)
#define __NR_timerfd_settime (3)
#define __NR_timerfd_close (4)

int syscall(int number, ...);

int syscall_timer_timeout(int h);
int syscall_timer_restart(int h);

#endif