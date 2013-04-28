/*-
 * Copyright (c) 2003-2005 MAEKAWA Masahide <maekawa@cvsync.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

static struct pollfd *__fds = NULL;
static nfds_t __nfds = 0;

bool
sock_init(int *socks)
{
	nfds_t i;

	for (i = 0 ; socks[i] != -1 ; i++)
		__nfds++;
	if (__nfds == 0)
		return (false);
	if ((__fds = malloc(__nfds * sizeof(*__fds))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	for (i = 0 ; i < __nfds ; i++) {
		__fds[i].fd = socks[i];
		__fds[i].events = POLLIN;
	}

	return (true);
}

void
sock_destroy(void)
{
	free(__fds);
}

bool
sock_select(void)
{
	nfds_t i;

	for (i = 0 ; i < __nfds ; i++)
		__fds[i].revents = 0;

	if (poll(__fds, __nfds, 1000 /* 1sec */) == -1)
		return (false);

	return (true);
}

int
sock_accept(void)
{
	nfds_t i;
	int sock = -1;

	for (i = 0 ; i < __nfds ; i++) {
		if (!(__fds[i].revents & POLLIN))
			continue;

		if ((sock = accept(__fds[i].fd, NULL, NULL)) == -1) {
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
	struct pollfd fds[1];
	short events;
	int rv;

	if (dir == CVSYNC_SOCKDIR_OUT)
		events = POLLOUT;
	else /* dir == CVSYNC_SOCKDIR_IN */
		events = POLLIN;

	fds[0].fd = sock;
	fds[0].events = events;
	fds[0].revents = 0;

	for (;;) {
		if ((rv = poll(fds, 1, CVSYNC_TICKS)) == -1) {
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
		if (!(fds[0].revents & events)) {
			logmsg_err("Socket Error: poll");
			return (false);
		}
		break;
	}

	return (true);
}
