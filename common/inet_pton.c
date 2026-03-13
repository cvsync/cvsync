/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "compat_arpa_inet.h"
#include "compat_stdbool.h"

#include "network.h"

int
inet_pton(int af, const char *src, void *dst)
{
	const char *p;
	in_addr_t addr;
	int n;

	if (af != AF_INET) {
		errno = EAFNOSUPPORT;
		return (-1);
	}

	if ((addr = inet_addr(src)) == (in_addr_t)-1) {
		errno = EAFNOSUPPORT;
		return (-1);
	}

	n = 0;
	for (p = src ; *p != '\0' ; p++) {
		if (*p == '.')
			n++;
	}
	if (n != 3)
		return (0);

	(void)memcpy(dst, &addr, sizeof(addr));

	return (1);
}
