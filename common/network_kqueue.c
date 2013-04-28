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
