/*-
 * Copyright (c) 2000-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <netdb.h>

#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_netdb.h"

#include "logmsg.h"
#include "network.h"

int *
sock_listen(const char *host, const char *serv)
{
	static const int on = 1;
	struct addrinfo hints, *ai, *res;
	char nhost[CVSYNC_MAXHOST], nserv[CVSYNC_MAXSERV];
	int *socks, sock, flags, err;
	size_t nsocks = 0;

	(void)memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (host == NULL)
		hints.ai_flags = AI_PASSIVE;

	if ((err = getaddrinfo(host, serv, &hints, &res)) != 0) {
		logmsg_err("host %s: serv: %s: %s", host, serv,
			   gai_strerror(err));
		return (NULL);
	}

	for (ai = res ; ai != NULL ; ai = ai->ai_next)
		nsocks++;
	if (nsocks == 0) {
		logmsg_err("no address available");
		freeaddrinfo(res);
		return (NULL);
	}

	if ((socks = malloc((nsocks + 1) * sizeof(*socks))) == NULL) {
		logmsg_err("%s", strerror(errno));
		freeaddrinfo(res);
		return (NULL);
	}

	nsocks = 0;

	for (ai = res ; ai != NULL ; ai = ai->ai_next) {
#if defined(AF_INET6)
		if ((ai->ai_family != AF_INET) && (ai->ai_family != AF_INET6))
#else /* defined(AF_INET6) */
		if (ai->ai_family != AF_INET)
#endif /* defined(AF_INET6) */
			continue;

		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, nhost,
				sizeof(nhost), nserv, sizeof(nserv),
				NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			continue;
		}

		if ((sock = socket(ai->ai_family, ai->ai_socktype,
				   ai->ai_protocol)) == -1) {
			continue;
		}

		if ((flags = fcntl(sock, F_GETFL)) < 0) {
			(void)close(sock);
			continue;
		}
		if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
			(void)close(sock);
			continue;
		}

#if defined(IPV6_V6ONLY)
		if (ai->ai_family == AF_INET6) {
			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on,
				   sizeof(on));
		}
#endif /* defined(IPV6_V6ONLY) */

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			       &on, sizeof(on)) == -1) {
			(void)close(sock);
			continue;
		}

		if (bind(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
			(void)close(sock);
			continue;
		}

		if (listen(sock, SOMAXCONN) == -1) {
			(void)close(sock);
			continue;
		}

		socks[nsocks++] = sock;

		logmsg_verbose("Listen on %s port %s", nhost, nserv);
	}

	freeaddrinfo(res);

	if (nsocks == 0) {
		logmsg_err("no listening socket");
		free(socks);
		return (NULL);
	}

	socks[nsocks] = -1;

	return (socks);
}

int
sock_connect(int af, const char *host, const char *serv)
{
	struct addrinfo hints, *ai, *res;
	char nhost[CVSYNC_MAXHOST], nserv[CVSYNC_MAXSERV];
	int sock = -1, err;

	(void)memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;

	logmsg("Connecting to %s port %s", host, serv);

	if ((err = getaddrinfo(host, serv, &hints, &res)) != 0) {
		logmsg_err("host: %s: serv: %s: %s", host, serv,
			   gai_strerror(err));
		return (-1);
	}

	for (ai = res ; ai != NULL ; ai = ai->ai_next) {
#if defined(AF_INET6)
		if ((ai->ai_family != AF_INET) && (ai->ai_family != AF_INET6))
#else /* defined(AF_INET6) */
		if (ai->ai_family != AF_INET)
#endif /* defined(AF_INET6) */
			continue;

		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, nhost,
				sizeof(nhost), nserv, sizeof(nserv),
				NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			continue;
		}

		if ((sock = socket(ai->ai_family, ai->ai_socktype,
				   ai->ai_protocol)) == -1) {
			continue;
		}

		if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
			logmsg_err("host %s port %s: %s", nhost, nserv,
				   strerror(errno));
			(void)close(sock);
			sock = -1;
			continue;
		}

		break;
	}

	freeaddrinfo(res);

	if (sock == -1)
		return (-1);

	logmsg("Connected to %s port %s", nhost, nserv);

	return (sock);
}

bool
sock_getpeeraddr(int sock, int *af, void *buffer, size_t bufsize)
{
	struct sockaddr_storage ss;
	socklen_t sslen = sizeof(ss);
	int err;

	if (getpeername(sock, (struct sockaddr *)(void *)&ss, &sslen) == -1) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	if ((err = getnameinfo((struct sockaddr *)(void *)&ss, sslen, buffer,
			       bufsize, NULL, 0, NI_NUMERICHOST)) != 0) {
		logmsg_err("%s", strerror(err));
		return (false);
	}

	*af = ss.ss_family;

	return (true);
}

void
sock_resolv_addr(int af, const char *addr, char *buffer, size_t bufsize)
{
	struct addrinfo hints, *ai, *res;

	buffer[0] = '\0';

	(void)memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(addr, NULL, &hints, &res) != 0)
		return;

	for (ai = res ; ai != NULL ; ai = ai->ai_next) {
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, buffer,
				bufsize, NULL, 0, NI_NAMEREQD) == 0) {
			break;
		}
	}

	freeaddrinfo(res);
}
