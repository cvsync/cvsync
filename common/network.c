/*-
 * This software is released under the BSD License, see LICENSE.
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
