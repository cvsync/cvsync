/*-
 * Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "compat_arpa_inet.h"
#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "cvsync.h"
#include "list.h"
#include "logmsg.h"
#include "network.h"
#include "token.h"

#include "defs.h"

static const struct token_keyword access_keywords[] = {
	{ "allow",	5,	ACL_ALLOW },
	{ "always",	6,	ACL_ALWAYS },
	{ "deny",	4,	ACL_DENY },
	{ "permit",	6,	ACL_ALLOW },
	{ "reject",	6,	ACL_DENY },
	{ NULL,		0,	ACL_NOMATCH },
};

void access_open(const char *);
void access_close(struct access_control_args *);

struct aclent *access_match(struct access_control_args *, int, const char *);
struct access_control_args *access_parse(FILE *);
bool access_parse_allow(struct token *, struct aclent *);
bool access_parse_always(struct token *, struct aclent *);
bool access_parse_deny(struct token *, struct aclent *);
bool access_parse_address(char *, const char *, size_t, struct aclent *);
bool access_parse_hostname(char *, const char *, struct aclent *);
bool access_parse_number(char *, const char *, size_t *);
bool access_match_address(struct aclent *, int, const char *);
bool access_match_hostname(struct aclent *, const char *);
bool access_match_ipv4addr(struct aclent *, const void *);
bool access_match_ipv6addr(struct aclent *, const void *);
bool access_set_ipv4addr(struct aclent *, const void *, size_t);
bool access_set_ipv6addr(struct aclent *, const void *, size_t);

static struct server_args **acl;
static struct access_control_args *acl_lists;
static struct list *acl_high;
static char acl_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
static size_t acl_size, acl_actives = 0;
static time_t acl_mtime = 0;

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

bool
access_init(size_t sz)
{
#if !defined(NO_INITSTATE)
	static char acl_random_state[256];
#endif /* !defined(NO_INITSTATE) */
	struct timeval tv;

	if ((acl = malloc(sz * sizeof(*acl))) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		return (false);
	}
	(void)memset(acl, 0, sz * sizeof(*acl));

	if ((acl_high = list_init()) == NULL) {
		free(acl);
		return (false);
	}

	(void)gettimeofday(&tv, NULL);

#if !defined(NO_INITSTATE)
	initstate((unsigned long)tv.tv_usec, acl_random_state,
		  sizeof(acl_random_state));
#else /* !defined(NO_INITSTATE) */
	srandom((unsigned long)tv.tv_usec);
#endif /* !defined(NO_INITSTATE) */

	acl_name[0] = '\0';
	acl_size = sz;

	return (true);
}

void
access_destroy(void)
{
	struct listent *lep;
	struct server_args *sa;
	size_t i;

	pthread_mutex_lock(&mtx);

	if (cvsync_isterminated()) {
		for (i = 0 ; i < acl_size ; i++) {
			if ((sa = acl[i]) == NULL)
				continue;
			sock_close(sa->sa_socket);
		}

		for (lep = acl_high->l_head ;
		     lep != NULL ;
		     lep = lep->le_next) {
			sa = lep->le_elm;
			sock_close(sa->sa_socket);
		}
	}

	acl_size = 0;

	while (!list_isempty(acl_high) || (acl_actives != 0))
		pthread_cond_wait(&cond, &mtx);

	pthread_mutex_unlock(&mtx);

	list_destroy(acl_high);
	free(acl);

	access_close(acl_lists);
}

struct server_args *
access_authorize(int sock, struct config *cf)
{
	union {
		uint32_t	v32;
		uint8_t		v8[4];
	} _v;
	struct access_control_args aca;
	struct aclent *aclp;
	struct server_args *sa, *sa_active;
	size_t n, i;
	int wn;

	if (cvsync_isterminated())
		return (NULL);

	if ((sa = malloc(sizeof(*sa))) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		return (NULL);
	}
	sa->sa_error = CVSYNC_NO_ERROR;

	if (cvsync_isinterrupted()) {
		sa->sa_status = ACL_DENY;
		sa->sa_error = CVSYNC_ERROR_UNAVAIL;
		return (sa);
	}

	if (!sock_getpeeraddr(sock, &sa->sa_family, sa->sa_addr,
			      sizeof(sa->sa_addr))) {
		sa->sa_status = ACL_DENY;
		sa->sa_error = CVSYNC_ERROR_UNAVAIL;
		return (sa);
	}
	_v.v32 = (uint32_t)random();
	wn = snprintf(sa->sa_hostinfo, sizeof(sa->sa_hostinfo),
		      "[%s] (%02x%02x%02x%02x)", sa->sa_addr,
		      _v.v8[0], _v.v8[1], _v.v8[2], _v.v8[3]);
	if ((wn <= 0) || ((size_t)wn >= sizeof(sa->sa_hostinfo))) {
		logmsg_err("ACL: %s", strerror(EINVAL));
		sa->sa_status = ACL_DENY;
		sa->sa_error = CVSYNC_ERROR_UNAVAIL;
		return (sa);
	}

	access_open(cf->cf_access_name);

	if ((aclp = access_match(acl_lists, sa->sa_family,
				 sa->sa_addr)) != NULL) {
		sa->sa_status = aclp->acl_status;
	} else {
		sa->sa_status = ACL_ALLOW;
	}

	switch (sa->sa_status) {
	case ACL_ALLOW:
		if (pthread_mutex_lock(&mtx) != 0) {
			sa->sa_status = ACL_DENY;
			sa->sa_error = CVSYNC_ERROR_UNAVAIL;
			return (sa);
		}

		if (acl_actives >= acl_size) {
			pthread_mutex_unlock(&mtx);
			sa->sa_status = ACL_DENY;
			sa->sa_error = CVSYNC_ERROR_LIMITED;
			return (sa);
		}

		if ((aclp != NULL) && (aclp->acl_max > 0)) {
			aca.aca_patterns = aclp;
			aca.aca_size = 1;

			n = 0;
			for (i = 0 ; i < acl_size ; i++) {
				if ((sa_active = acl[i]) == NULL)
					continue;
				if (access_match(&aca, sa_active->sa_family,
						 sa_active->sa_addr) != NULL) {
					if (++n <= aclp->acl_max)
						continue;

					pthread_mutex_unlock(&mtx);
					sa->sa_status = ACL_DENY;
					sa->sa_error = CVSYNC_ERROR_LIMITED;
					return (sa);
				}
			}
		}
		for (i = 0 ; i < acl_size ; i++) {
			if (acl[i] == NULL)
				break;
		}
		sa->sa_id = i;

		acl[i] = sa;
		acl_actives++;

		if (pthread_mutex_unlock(&mtx) != 0) {
			acl[i] = NULL;
			acl_actives--;
			sa->sa_status = ACL_DENY;
			sa->sa_error = CVSYNC_ERROR_UNAVAIL;
			return (sa);
		}
		break;
	case ACL_ALWAYS:
		if (pthread_mutex_lock(&mtx) != 0) {
			sa->sa_status = ACL_DENY;
			sa->sa_error = CVSYNC_ERROR_UNAVAIL;
			return (sa);
		}

		if (!list_insert_tail(acl_high, sa)) {
			pthread_mutex_unlock(&mtx);
			sa->sa_status = ACL_DENY;
			sa->sa_error = CVSYNC_ERROR_UNAVAIL;
			return (sa);
		}

		if (pthread_mutex_unlock(&mtx) != 0) {
			sa->sa_status = ACL_DENY;
			sa->sa_error = CVSYNC_ERROR_UNAVAIL;
			return (sa);
		}
		break;
	case ACL_DENY:
		sa->sa_error = CVSYNC_ERROR_DENIED;
		break;
	default:
		sa->sa_status = ACL_DENY;
		sa->sa_error = CVSYNC_ERROR_UNAVAIL;
		return (sa);
	}

	sa->sa_socket = sock;
	sa->sa_config = cf;

	config_acquire(cf);

	logmsg("%s Connected (status=%d)", sa->sa_hostinfo, sa->sa_status);
	time(&sa->sa_tick);

	return (sa);
}

void
access_done(struct server_args *sa)
{
	struct config *cf = sa->sa_config;
	struct listent *lep;

	pthread_mutex_lock(&mtx);

	switch (sa->sa_status) {
	case ACL_ALLOW:
		acl[sa->sa_id] = NULL;
		acl_actives--;
		break;
	case ACL_ALWAYS:
		for (lep = acl_high->l_head ;
		     lep != NULL ;
		     lep = lep->le_next) {
			if (lep->le_elm == sa)
				break;
		}
		if (lep == NULL)
			logmsg_err("ACL: not found: %s", sa->sa_hostinfo);
		if (!list_remove(acl_high, lep))
			logmsg_err("ACL: fail to remove: %s", sa->sa_hostinfo);
		break;
	case ACL_DENY:
		/* Nothing to do. */
		break;
	default:
		break;
	}

	pthread_cond_signal(&cond);

	pthread_mutex_unlock(&mtx);

	if (sa->sa_status == ACL_DENY)
		logmsg("%s Connection denied", sa->sa_hostinfo);
	else
		logmsg("%s Connection closed", sa->sa_hostinfo);

	sock_close(sa->sa_socket);
	free(sa);

	config_revoke(cf);
}

void
access_open(const char *fname)
{
	struct access_control_args *aca;
	struct stat st;
	FILE *fp;
	int fd;
	bool force_load = false;

	if (strlen(fname) == 0) {
		access_close(acl_lists);
		acl_name[0] = '\0';
		acl_lists = NULL;
		return;
	}

	if ((strlen(acl_name) == 0) || (strcmp(acl_name, fname) != 0))
		force_load = true;

	if ((fd = open(fname, O_RDONLY, 0)) == -1) {
		if (errno != ENOENT) {
			logmsg_err("ACL: %s: %s", fname, strerror(errno));
		}
		return;
	}
	if (fstat(fd, &st) == -1) {
		logmsg_err("ACL: %s: %s", fname, strerror(errno));
		(void)close(fd);
		return;
	}

	if (!force_load && (st.st_mtime == acl_mtime)) {
		(void)close(fd);
		return;
	}
	acl_mtime = st.st_mtime;

	if (st.st_size == 0) {
		(void)close(fd);
		aca = NULL;
		goto done;
	}

	if ((fp = fdopen(fd, "r")) == NULL) {
		logmsg_err("ACL: %s: %s", fname, strerror(errno));
		(void)close(fd);
		return;
	}

	if ((aca = access_parse(fp)) == NULL) {
		logmsg_err("ACL: %s: failed to load", fname);
		(void)fclose(fp);
		return;
	}

	if (fclose(fp) == EOF) {
		logmsg_err("ACL: %s: %s", fname, strerror(errno));
		access_close(aca);
		return;
	}

done:
	snprintf(acl_name, sizeof(acl_name), "%s", fname);
	access_close(acl_lists);
	acl_lists = aca;

	logmsg_verbose("ACL: reloaded");
}

void
access_close(struct access_control_args *aca)
{
	size_t i;

	if (aca == NULL)
		return;

	if (aca->aca_patterns != NULL) {
		for (i = 0 ; i < aca->aca_size ; i++) {
			if (aca->aca_patterns[i].acl_addr != NULL)
				free(aca->aca_patterns[i].acl_addr);
			if (aca->aca_patterns[i].acl_mask != NULL)
				free(aca->aca_patterns[i].acl_mask);
		}
		free(aca->aca_patterns);
	}

	free(aca);
}

struct aclent *
access_match(struct access_control_args *aca, int af, const char *addr)
{
	struct aclent *aclp = NULL;
	char host[CVSYNC_MAXHOST];
	size_t i;
	bool resolved = false;

	if ((aca == NULL) || (aca->aca_size == 0))
		return (NULL);

	for (i = 0 ; i < aca->aca_size ; i++) {
		aclp = &aca->aca_patterns[i];

		if (aclp->acl_family == AF_UNSPEC) {
			if (!resolved) {
				sock_resolv_addr(af, addr, host, sizeof(host));
				resolved = true;
			}
			if (strlen(host) == 0)
				continue;
			if (access_match_hostname(aclp, host))
				break;
		} else {
			if (access_match_address(aclp, af, addr))
				break;
		}
	}
	if (i == aca->aca_size)
		aclp = NULL;

	return (aclp);
}

struct access_control_args *
access_parse(FILE *fp)
{
	const struct token_keyword *key;
	struct access_control_args *aca;
	struct aclent *aclp;
	struct token tk;
	size_t max = 0;

	lineno = 1;

	if ((aca = malloc(sizeof(*aca))) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		return (NULL);
	}
	aca->aca_patterns = NULL;
	aca->aca_size = 0;

	for (;;) {
		if (!token_skip_whitespace(fp)) {
			if (feof(fp) == 0) {
				access_close(aca);
				return (NULL);
			}
			break;
		}

		if (aca->aca_size == max) {
			struct aclent *newptr;
			size_t old = max, new = old + 4;

			if ((newptr = malloc(new * sizeof(*newptr))) == NULL) {
				logmsg_err("ACL: %s", strerror(errno));
				access_close(aca);
				return (NULL);
			}

			if (aca->aca_patterns != NULL) {
				(void)memcpy(newptr, aca->aca_patterns,
					     old * sizeof(*newptr));
				free(aca->aca_patterns);
			}
			aca->aca_patterns = newptr;
			max = new;
		}
		aclp = &aca->aca_patterns[aca->aca_size++];

		if ((key = token_get_keyword(fp, access_keywords)) == NULL) {
			access_close(aca);
			return (NULL);
		}
		aclp->acl_status = key->type;

		if (!token_get_string(fp, &tk)) {
			access_close(aca);
			return (NULL);
		}

		switch (aclp->acl_status) {
		case ACL_ALLOW:
			if (!access_parse_allow(&tk, aclp)) {
				logmsg_err("ACL: line %u: %s: invalid "
					   "address/hostname", lineno,
					   tk.token);
				access_close(aca);
				return (NULL);
			}
			break;
		case ACL_ALWAYS:
			if (!access_parse_always(&tk, aclp)) {
				logmsg_err("ACL: line %u: %s: invalid "
					   "address/hostname", lineno,
					   tk.token);
				access_close(aca);
				return (NULL);
			}
			break;
		case ACL_DENY:
			if (!access_parse_deny(&tk, aclp)) {
				logmsg_err("ACL: line %u: %s: invalid "
					   "address/hostname", lineno,
					   tk.token);
				access_close(aca);
				return (NULL);
			}
			break;
		default:
			access_close(aca);
			return (NULL);
		}
	}

	if (aca->aca_size == 0) {
		free(aca->aca_patterns);
		aca->aca_patterns = NULL;
	}

	return (aca);
}

bool
access_parse_allow(struct token *tk, struct aclent *aclp)
{
	char *sp = tk->token, *bp = sp + tk->length, *sep;
	size_t n;

	if ((sep = memchr(sp, ',', (size_t)(bp - sp))) != NULL) {
		if (sep + 1 >= bp)
			return (false);
		if (!access_parse_number(sep + 1, bp, &aclp->acl_max))
			return (false);
		bp = sep;
	}

	if ((sep = memchr(sp, '/', (size_t)(bp - sp))) != NULL) {
		if (sep + 1 >= bp)
			return (false);
		if (!access_parse_number(sep + 1, bp, &n))
			return (false);
		return (access_parse_address(sp, sep, n, aclp));
	}

	if (!access_parse_address(sp, bp, (size_t)-1, aclp))
		return (access_parse_hostname(sp, bp, aclp));

	return (true);
}

bool
access_parse_always(struct token *tk, struct aclent *aclp)
{
	char *sp = tk->token, *bp = sp + tk->length, *sep;
	size_t n;

	if ((sep = memchr(sp, '/', (size_t)(bp - sp))) != NULL) {
		if (sep + 1 >= bp)
			return (false);
		if (!access_parse_number(sep + 1, bp, &n))
			return (false);
		return (access_parse_address(sp, sep, n, aclp));
	}

	if (!access_parse_address(sp, bp, (size_t)-1, aclp))
		return (access_parse_hostname(sp, bp, aclp));

	return (true);
}

bool
access_parse_deny(struct token *tk, struct aclent *aclp)
{
	char *sp = tk->token, *bp = sp + tk->length, *sep;
	size_t n;

	if ((sep = memchr(sp, '/', (size_t)(bp - sp))) != NULL) {
		if (sep + 1 >= bp)
			return (false);
		if (!access_parse_number(sep + 1, bp, &n))
			return (false);
		return (access_parse_address(sp, sep, n, aclp));
	}

	if (!access_parse_address(sp, bp, (size_t)-1, aclp))
		return (access_parse_hostname(sp, bp, aclp));

	return (true);
}

bool
access_parse_address(char *sp, const char *bp, size_t n, struct aclent *aclp)
{
	char addr[CVSYNC_MAXHOST], binaddr[CVSYNC_MAXADDRLEN];
	size_t addrlen;

	if ((addrlen = (size_t)(bp - sp)) >= sizeof(addr))
		return (false);
	(void)memcpy(addr, sp, addrlen);
	addr[addrlen] = '\0';

	if (inet_pton(AF_INET, addr, binaddr) == 1) {
		aclp->acl_family = AF_INET;
		return (access_set_ipv4addr(aclp, binaddr, n));
	}
#if defined(AF_INET6)
	if (inet_pton(AF_INET6, addr, binaddr) == 1) {
		aclp->acl_family = AF_INET6;
		return (access_set_ipv6addr(aclp, binaddr, n));
	}
#endif /* defined(AF_INET6) */

	return (false);
}

bool
access_parse_hostname(char *sp, const char *bp, struct aclent *aclp)
{
	if (isdigit((int)(sp[bp - sp - 1])) ||
	    (memchr(sp, ':', (size_t)(bp - sp)) != NULL)) {
		return (false);
	}

	aclp->acl_family = AF_UNSPEC;
	aclp->acl_addrlen = (size_t)(bp - sp);
	if ((aclp->acl_addr = malloc(aclp->acl_addrlen + 1)) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		return (false);
	}
	(void)memcpy(aclp->acl_addr, sp, aclp->acl_addrlen);
	((char *)aclp->acl_addr)[aclp->acl_addrlen] = '\0';
	aclp->acl_mask = NULL;

	return (true);
}

bool
access_parse_number(char *sp, const char *bp, size_t *n)
{
	char s[5], *ep;
	size_t len;
	unsigned long v;

	if ((len = (size_t)(bp - sp)) >= sizeof(s)) {
		logmsg_err("ACL: %.*s: restricted to 1000", len, sp);
		return (false);
	}
	(void)memcpy(s, sp, len);
	s[len] = '\0';

	errno = 0;
	v = strtoul(s, &ep, 0);
	if ((ep == NULL) || (*ep != '\0') || ((v == 0) && (errno == EINVAL)) ||
	    ((v == ULONG_MAX) && (errno == ERANGE))) {
		logmsg_err("ACL: %s: %s", s, strerror(EINVAL));
		return (false);
	}
	if (v > 1000) {
		logmsg_err("ACL: %lu: restricted to 1000", v);
		return (false);
	}

	*n = (size_t)v;

	return (true);
}

bool
access_match_address(struct aclent *aclp, int af, const char *addr)
{
	uint8_t binaddr[CVSYNC_MAXADDRLEN];

	if (af != aclp->acl_family)
		return (false);

	if (inet_pton(af, addr, binaddr) != 1)
		return (false);

	switch (af) {
	case AF_INET:
		return (access_match_ipv4addr(aclp, binaddr));
#if defined(AF_INET6)
	case AF_INET6:
		return (access_match_ipv6addr(aclp, binaddr));
#endif /* defined(AF_INET6) */
	default:
		break;
	}

	return (false);
}

bool
access_match_hostname(struct aclent *aclp, const char *host)
{
	if (fnmatch(aclp->acl_addr, host, 0) == FNM_NOMATCH)
		return (false);

	return (true);
}

bool
access_match_ipv4addr(struct aclent *aclp, const void *addr)
{
	uint32_t v4pat = *(uint32_t *)aclp->acl_addr;
	uint32_t v4mask = *(uint32_t *)aclp->acl_mask;
	uint32_t v4addr = *(const uint32_t *)addr;

	if (v4pat != (v4addr & v4mask))
		return (false);

	return (true);
}

bool
access_match_ipv6addr(struct aclent *aclp, const void *addr)
{
	const uint8_t *v6addr = addr;
	uint8_t *v6pat = aclp->acl_addr, *v6mask = aclp->acl_mask;
	size_t i;

	for (i = 0 ; i < aclp->acl_addrlen ; i++) {
		if (v6pat[i] != (v6addr[i] & v6mask[i]))
			return (false);
	}

	return (true);
}

bool
access_set_ipv4addr(struct aclent *aclp, const void *addr, size_t netmask)
{
	uint32_t v4addr = *(const uint32_t *)addr, v4mask;
	size_t addrbits, n;

	aclp->acl_addrlen = sizeof(uint32_t);
	addrbits = aclp->acl_addrlen * 8;

	if (netmask == (size_t)-1)
		netmask = addrbits;
	if (netmask > addrbits)
		return (false);
	v4mask = ntohl(INADDR_BROADCAST);
	if ((n = addrbits - netmask) > 0) {
		v4mask >>= n;
		v4mask <<= n;
	}
	v4mask = htonl(v4mask);

	if ((aclp->acl_addr = malloc(aclp->acl_addrlen)) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		return (false);
	}
	if ((aclp->acl_mask = malloc(aclp->acl_addrlen)) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		free(aclp->acl_addr);
		return (false);
	}
	*(uint32_t *)aclp->acl_addr = v4addr & v4mask;
	*(uint32_t *)aclp->acl_mask = v4mask;

	return (true);
}

bool
access_set_ipv6addr(struct aclent *aclp, const void *addr, size_t prefixlen)
{
	const uint8_t *v6addr = addr;
	uint8_t v6mask[16];
	size_t addrbits, n, i;

	aclp->acl_addrlen = 16;
	addrbits = aclp->acl_addrlen * 8;

	if (prefixlen == (size_t)-1)
		prefixlen = addrbits;
	if (prefixlen > addrbits)
		return (false);

	n = prefixlen / 8;
	for (i = 0 ; i < n ; i++)
		v6mask[i] = 0xff;
	if ((prefixlen % 8) != 0) {
		if ((i = 8 - (prefixlen - n * 8)) > 0) {
			v6mask[n] = (uint8_t)(0xff >> i);
			v6mask[n] = (uint8_t)(v6mask[n] << i);
			n++;
		}
	}
	while (n < aclp->acl_addrlen)
		v6mask[n++] = 0;

	if ((aclp->acl_addr = malloc(aclp->acl_addrlen)) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		return (false);
	}
	if ((aclp->acl_mask = malloc(aclp->acl_addrlen)) == NULL) {
		logmsg_err("ACL: %s", strerror(errno));
		free(aclp->acl_addr);
		return (false);
	}

	for (i = 0 ; i < aclp->acl_addrlen ; i++)
		((uint8_t *)aclp->acl_addr)[i] = v6addr[i] & v6mask[i];
	(void)memcpy(aclp->acl_mask, v6mask, aclp->acl_addrlen);

	return (true);
}
