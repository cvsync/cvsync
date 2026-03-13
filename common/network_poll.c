/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>

#include "compat_stdbool.h"

#include "logmsg.h"
#include "network.h"

static struct pollfd *fds = NULL;
static nfds_t nfds = 0;

bool
sock_init(int *socks)
{
	nfds_t i;

	for (i = 0 ; socks[i] != -1 ; i++)
		nfds++;
	if (nfds == 0)
		return (false);
	if ((fds = malloc(nfds * sizeof(*fds))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	for (i = 0 ; i < nfds ; i++) {
		fds[i].fd = socks[i];
		fds[i].events = POLLIN;
	}

	return (true);
}

void
sock_destroy(void)
{
	free(fds);
}

bool
sock_select(void)
{
	nfds_t i;

	for (i = 0 ; i < nfds ; i++)
		fds[i].revents = 0;

	if (poll(fds, nfds, 1000 /* 1sec */) == -1)
		return (false);

	return (true);
}

int
sock_accept(void)
{
	nfds_t i;
	int sock = -1;

	for (i = 0 ; i < nfds ; i++) {
		if (!(fds[i].revents & POLLIN))
			continue;

		if ((sock = accept(fds[i].fd, NULL, NULL)) == -1) {
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

bool
sock_wait(int sock, int dir)
{
	struct pollfd fds0[1];
	short events;
	int tmout, rv;

	if (dir == CVSYNC_SOCKDIR_OUT)
		events = POLLOUT;
	else /* dir == CVSYNC_SOCKDIR_IN */
		events = POLLIN;

	fds0[0].fd = sock;
	fds0[0].events = events;
	fds0[0].revents = 0;

	for (tmout = 0 ; tmout < CVSYNC_TIMEOUT ; tmout += CVSYNC_TICKS) {
		if ((rv = poll(fds0, 1, CVSYNC_TICKS)) == -1) {
			if (errno != EINTR) {
				logmsg_err("Socket Error: poll: %s",
					   strerror(errno));
				return (false);
			}
			rv = 0;
		}
		if (rv == 0) {
			if (sched_yield() == -1) {
				logmsg_err("Socket Error: yield: %s",
					   strerror(errno));
			}
			continue;
		}
		if (!(fds0[0].revents & events)) {
			logmsg_err("Socket Error: poll");
			return (false);
		}
		break;
	}
	if (tmout == CVSYNC_TIMEOUT) {
		logmsg_err("Socket Error: timeout");
		return (false);
	}

	return (true);
}
