/*-
 * Copyright (c) 2003-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "compat_stdbool.h"

#include "logmsg.h"
#include "network.h"

static fd_set __readfds;
static int *__socks, __nfds = 0, __fdmax = 0;

bool
sock_init(int *socks)
{
	__socks = socks;

	for (__nfds = 0 ; socks[__nfds] != -1 ; __nfds++) {
		if (socks[__nfds] >= (int)FD_SETSIZE)
			return (false);
		if (__fdmax < socks[__nfds])
			__fdmax = socks[__nfds];
	}
	__fdmax++;

	return (true);
}

void
sock_destroy(void)
{
	/* Nothing to do. */
}

bool
sock_select(void)
{
	struct timeval tv;
	int i;

	FD_ZERO(&__readfds);

	for (i = 0 ; i < __nfds ; i++)
		FD_SET(__socks[i], &__readfds);

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	if (select(__fdmax, &__readfds, NULL, NULL, &tv) == -1)
		return (false);

	return (true);
}

int
sock_accept(void)
{
	int sock = -1, i;

	for (i = 0 ; i < __nfds ; i++) {
		if (!FD_ISSET(__socks[i], &__readfds))
			continue;

		if ((sock = accept(__socks[i], NULL, NULL)) == -1) {
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
	fd_set *readfds, *writefds, fds;
	struct timeval tv;
	int tmout, rv;

	if (dir == CVSYNC_SOCKDIR_OUT) {
		readfds = NULL;
		writefds = &fds;
	} else { /* dir == CVSYNC_SOCKDIR_IN */
		readfds = &fds;
		writefds = NULL;
	}
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	for (tmout = 0 ; tmout < CVSYNC_TIMEOUT ; tmout += CVSYNC_TICKS) {
		tv.tv_sec = CVSYNC_TICKS / 1000;
		tv.tv_usec = (CVSYNC_TICKS % 1000) * 1000;
		if ((rv = select(sock + 1, readfds, writefds, NULL,
				 &tv)) == -1) {
			if (errno != EINTR) {
				logmsg_err("Socket Error: select: %s",
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
		if (!FD_ISSET(sock, &fds)) {
			logmsg_err("Socket Error: select");
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
