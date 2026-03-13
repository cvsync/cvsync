/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include "compat_stdbool.h"

#include "logmsg.h"
#include "network.h"

static struct kevent *__changelist;
static size_t __nchanges = 0;
static int __kq, __nevents;

bool
sock_init(int *socks)
{
	size_t i;

	for (i = 0 ; socks[i] != -1 ; i++)
		__nchanges++;
	if (__nchanges == 0)
		return (false);
	__changelist = malloc(__nchanges * sizeof(*__changelist));
	if (__changelist == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	for (i = 0 ; i < __nchanges ; i++) {
		EV_SET(&__changelist[i], socks[i], EVFILT_READ, EV_ADD,
		       0, 0, NULL);
	}
	if ((__kq = kqueue()) == -1) {
		logmsg_err("Socket Error: kqueue");
		free(__changelist);
		return (false);
	}
	if (kevent(__kq, __changelist, __nchanges, NULL, 0, NULL) == -1) {
		logmsg_err("Socket Error: kqueue: %s", strerror(errno));
		(void)close(__kq);
		free(__changelist);
		return (false);
	}

	return (true);
}

void
sock_destroy(void)
{
	free(__changelist);
	if (close(__kq) == -1)
		logmsg_err("Socket Error: kqueue: %s", strerror(errno));
}

bool
sock_select(void)
{
	struct timespec ts;

	ts.tv_sec = 1;
	ts.tv_nsec = 0;

	if ((__nevents = kevent(__kq, NULL, 0, __changelist, __nchanges,
				&ts)) == -1) {
		logmsg_err("Socket Error: kqueue: %s", strerror(errno));
		return (false);
	}

	return (true);
}

int
sock_accept(void)
{
	int sock = -1, i;

	for (i = 0 ; i < __nevents ; i++) {
		if ((sock = accept((int)__changelist[i].ident, NULL,
				   NULL)) == -1) {
			if (errno == EINTR) {
				logmsg_intr();
				return (-1);
			}
			if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
				logmsg_err("%s", strerror(errno));
				return (-1);
			}
			continue;
		}

		break;
	}

	return (sock);
}
