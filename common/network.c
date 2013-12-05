/*-
 * Copyright (c) 2000-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"

#include "logmsg.h"
#include "network.h"

#ifndef ENABLE_SOCK_WAIT
#define	ENABLE_SOCK_WAIT	(0)
#endif /* ENABLE_SOCK_WAIT */

void
sock_listen_stop(int *socks)
{
	int i;

	for (i = 0 ; socks[i] != -1 ; i++)
		(void)close(socks[i]);
	free(socks);
}

void
sock_close(int sock)
{
	(void)close(sock);
}

bool
sock_send(int sock, const void *buffer, size_t bufsize)
{
	const uint8_t *sp = buffer, *bp = sp + bufsize;
	ssize_t xn;
	bool interrupted = false;

	while (sp < bp) {
#if ENABLE_SOCK_WAIT
		if (!sock_wait(sock, CVSYNC_SOCKDIR_OUT))
			return (false);
#endif /* ENABLE_SOCK_WAIT */
		if ((xn = send(sock, sp, (size_t)(bp - sp), 0)) == -1) {
			if (errno != EINTR) {
				logmsg_err("Socket Error: send: %s",
					   strerror(errno));
				return (false);
			}
			xn = 0;
		}
		if (xn != 0) {
			sp += xn;
			interrupted = false;
			continue;
		}
		if (interrupted)
			break;

		interrupted = true;
	}
	if (sp != bp) {
		logmsg_err("Socket Error: send: %u residue %d", bufsize,
			   bp - sp);
		return (false);
	}

	return (true);
}

bool
sock_recv(int sock, void *buffer, size_t bufsize)
{
	uint8_t *sp = buffer, *bp = sp + bufsize;
	ssize_t rn;
	bool interrupted = false;

	while (sp < bp) {
#if ENABLE_SOCK_WAIT
		if (!sock_wait(sock, CVSYNC_SOCKDIR_IN))
			return (false);
#endif /* ENABLE_SOCK_WAIT */
		if ((rn = recv(sock, sp, (size_t)(bp - sp), 0)) == -1) {
			if (errno != EINTR) {
				logmsg_err("Socket Error: recv: %s",
					   strerror(errno));
				return (false);
			}
			rn = 0;
		}
		if (rn != 0) {
			sp += rn;
			interrupted = false;
			continue;
		}
		if (interrupted)
			break;

		interrupted = true;
	}
	if (sp != bp) {
		logmsg_err("Socket Error: recv: %u residue %d", bufsize,
			   bp - sp);
		return (false);
	}

	return (true);
}
