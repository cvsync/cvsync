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

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"

#include "logmsg.h"
#include "network.h"

int ipv4_listen_addr(in_addr_t, in_port_t);
int ipv4_connect_addr(in_addr_t, in_port_t);
in_port_t ipv4_port_pton(const char *);

int *
sock_listen(const char *host, const char *serv)
{
	struct hostent *h;
	struct in_addr in;
	in_addr_t addr;
	in_port_t port;
	int *socks, sock, nsocks, i;

	if (serv == NULL)
		return (NULL);
	if ((port = ipv4_port_pton(serv)) == 0)
		return (NULL);

	if (host == NULL) {
		if ((socks = malloc(2 * sizeof(*socks))) == NULL) {
			logmsg_err("%s", strerror(errno));
			return (NULL);
		}
		addr = htonl(INADDR_ANY);
		if ((socks[0] = ipv4_listen_addr(addr, port)) == -1) {
			free(socks);
			return (NULL);
		}

		in.s_addr = addr;
		logmsg_verbose("Listen on %s port %u", inet_ntoa(in), port);

		socks[1] = -1;
		return (socks);
	}
	if ((addr = inet_addr(host)) != (in_addr_t)-1) {
		if ((socks = malloc(2 * sizeof(*socks))) == NULL) {
			logmsg_err("%s", strerror(errno));
			return (NULL);
		}
		if ((socks[0] = ipv4_listen_addr(addr, port)) == -1) {
			free(socks);
			return (NULL);
		}
		socks[1] = -1;
		return (socks);
	}

	if ((h = gethostbyname(host)) == NULL)
		return (NULL);

	for (nsocks = 0 ; h->h_addr_list[nsocks] != NULL ; nsocks++)
		/* Nothing to do. */;
	if (nsocks == 0) {
		logmsg_err("no address available");
		return (NULL);
	}

	if ((socks = malloc((nsocks + 1) * sizeof(*socks))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	nsocks = 0;
	for (i = 0 ; h->h_addr_list[i] != NULL ; i++) {
		addr = *(in_addr_t *)(void *)h->h_addr_list[i];
		if ((sock = ipv4_listen_addr(addr, port)) == -1)
			continue;

		in.s_addr = addr;
		logmsg_verbose("Listen on %s port %u", inet_ntoa(in), port);

		socks[nsocks++] = sock;
	}

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
	struct hostent *h;
	struct in_addr in;
	in_addr_t addr;
	in_port_t port;
	int sock = -1, i;

	if ((af != AF_UNSPEC) && (af != AF_INET))
		return (-1);
	if ((host == NULL) || (serv == NULL))
		return (-1);

	logmsg("Connecting to %s port %s", host, serv);

	if ((port = ipv4_port_pton(serv)) == 0)
		return (-1);

	if ((addr = inet_addr(host)) != (in_addr_t)-1) {
		if ((sock = ipv4_connect_addr(addr, port)) == -1)
			return (-1);
		goto done;
	}

	if ((h = gethostbyname(host)) == NULL)
		return (-1);

	for (i = 0 ; h->h_addr_list[i] != NULL ; i++) {
		addr = *(in_addr_t *)(void *)h->h_addr_list[i];
		if ((sock = ipv4_connect_addr(addr, port)) == -1)
			continue;

		break;
	}

	if (sock == -1)
		return (-1);

done:
	in.s_addr = addr; /* already network byte order */
	logmsg("Connected to %s port %u", inet_ntoa(in), port);

	return (sock);
}

bool
sock_getpeeraddr(int sock, int *af, void *buffer, size_t bufsize)
{
	struct sockaddr_in sin4;
	char *cp;
	socklen_t sin4len = sizeof(sin4);
	size_t len;

	*af = AF_INET;

	if (getpeername(sock, (struct sockaddr *)(void *)&sin4,
			&sin4len) == -1) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	if ((cp = inet_ntoa(sin4.sin_addr)) == NULL)
		return (false);
	if ((len = strlen(cp)) >= bufsize)
		return (false);
	(void)memcpy(buffer, cp, len);
	((char *)buffer)[len] = '\0';

	return (true);
}

void
sock_resolv_addr(int af, const char *addr, char *buffer, size_t bufsize)
{
	struct hostent *h;
	size_t len;

	buffer[0] = '\0';

	if (af != AF_INET)
		return;

	if ((h = gethostbyname(addr)) == NULL)
		return;

	if ((h->h_name == NULL) || ((len = strlen(h->h_name)) >= bufsize))
		return;

	(void)memcpy(buffer, h->h_name, len);
	buffer[len] = '\0';
}

int
ipv4_listen_addr(in_addr_t addr, in_port_t port)
{
	static const int on = 1;
	struct sockaddr_in sin4;
	int sock, flags;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return (-1);

	if ((flags = fcntl(sock, F_GETFL)) < 0) {
		(void)close(sock);
		return (-1);
	}
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		(void)close(sock);
		return (-1);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on,
		       sizeof(on)) == -1) {
		(void)close(sock);
		return (-1);
	}

	sin4.sin_family = AF_INET;
	sin4.sin_port = htons(port);
	sin4.sin_addr.s_addr = addr; /* already network byte order */

	if (bind(sock, (struct sockaddr *)(void *)&sin4,
		 sizeof(sin4)) == -1) {
		(void)close(sock);
		return (-1);
	}

	if (listen(sock, SOMAXCONN) == -1) {
		(void)close(sock);
		return (-1);
	}

	return (sock);
}

int
ipv4_connect_addr(in_addr_t addr, in_port_t port)
{
	struct sockaddr_in sin4;
	int sock;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return (-1);

	sin4.sin_family = AF_INET;
	sin4.sin_port = htons(port);
	sin4.sin_addr.s_addr = addr; /* already network byte order */

	if (connect(sock, (struct sockaddr *)(void *)&sin4,
		    sizeof(sin4)) == -1) {
		(void)close(sock);
		return (-1);
	}

	return (sock);
}

in_port_t
ipv4_port_pton(const char *name)
{
	struct servent *serv;
	char *ep;
	unsigned long ul;

	if ((serv = getservbyname(name, "tcp")) != NULL)
		return (ntohs(serv->s_port));

	errno = 0;
	ul = strtoul(name, &ep, 10);
	if ((ep == NULL) || (*ep != '\0') ||
	    ((ul == 0) && (errno == EINVAL)) ||
	    ((ul == ULONG_MAX) && (errno == ERANGE))) {
		return (0);
	}
	if ((ul == 0) || (ul > 65535))
		return (0);

	return ((in_port_t)ul);
}
