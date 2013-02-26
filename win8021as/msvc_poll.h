#ifndef _MSVC_POLL_H_
#define _MSVC_POLL_H_

struct pollfd {
                int fd;           /* file descriptor */
                short events;     /* requested events */
                short revents;    /* returned events */
};

/* Linux/include/asm-generic/poll.h */

/* These are specified by Winsock2.h */
#define POLLRDNORM  0x0100
#define POLLRDBAND  0x0200
#define POLLIN      (POLLRDNORM | POLLRDBAND)
#define POLLPRI     0x0400

#define POLLWRNORM  0x0010
#define POLLOUT     (POLLWRNORM)
#define POLLWRBAND  0x0020

#define POLLERR     0x0001
#define POLLHUP     0x0002
#define POLLNVAL    0x0004

int poll(struct pollfd *fds, size_t nfds, int timeout);

#endif