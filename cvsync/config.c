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
#include <sys/socket.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_strings.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "hash.h"
#include "logmsg.h"
#include "network.h"
#include "refuse.h"
#include "scanfile.h"
#include "token.h"

#include "config_common.h"
#include "defs.h"

enum {
	TOK_BASE,
	TOK_BASE_PREFIX,
	TOK_COLLECTION,
	TOK_COMPRESS,
	TOK_CONFIG,
	TOK_ERRORMODE,
	TOK_HASH,
	TOK_HOSTNAME,
	TOK_LBRACE,
	TOK_LOOSE,
	TOK_NAME,
	TOK_PORT,
	TOK_PREFIX,
	TOK_PROTOCOL,
	TOK_RBRACE,
	TOK_REFUSE,
	TOK_RELEASE,
	TOK_SCANFILE,
	TOK_UMASK,

	TOK_UNKNOWN
};

static const struct token_keyword config_keywords[] = {
	{ "{",			1,	TOK_LBRACE },
	{ "}",			1,	TOK_RBRACE },
	{ "base",		4,	TOK_BASE },
	{ "base-prefix",	11,	TOK_BASE_PREFIX },
	{ "collection",		10,	TOK_COLLECTION },
	{ "compress",		8,	TOK_COMPRESS },
	{ "config",		6,	TOK_CONFIG },
	{ "errormode",		9,	TOK_ERRORMODE },
	{ "hash",		4,	TOK_HASH },
	{ "host",		4,	TOK_HOSTNAME },
	{ "hostname",		8,	TOK_HOSTNAME },
	{ "loose",		5,	TOK_LOOSE },
	{ "name",		4,	TOK_NAME },
	{ "port",		4,	TOK_PORT },
	{ "prefix",		6,	TOK_PREFIX },
	{ "proto",		5,	TOK_PROTOCOL },
	{ "protocol",		8,	TOK_PROTOCOL },
	{ "refuse",		6,	TOK_REFUSE },
	{ "release",		7,	TOK_RELEASE },
	{ "scanfile",		8,	TOK_SCANFILE },
	{ "umask",		5,	TOK_UMASK },
	{ NULL,			0,	TOK_UNKNOWN },
};

/* protocol */
static const struct token_keyword protocol_keywords[] = {
	/* IPv4 */
	{ "inet",	4,	AF_INET },
	{ "inet4",	5,	AF_INET },
	{ "ipv4",	4,	AF_INET },

#if defined(AF_INET6)
	/* IPv6 */
	{ "inet6",	5,	AF_INET6 },
	{ "ipv6",	4,	AF_INET6 },
#endif /* defined(AF_INET6) */

	{ NULL,		0,	AF_UNSPEC },
};

struct config *config_parse_file(struct config_args *);
struct config *config_parse_file_config(struct config_args *);
struct collection *config_parse_file_collection(struct config_args *);
struct config *config_parse_uri(const char *, const char *);
bool config_parse_uri_host(const char *, const char *, struct config *);
bool config_parse_uri_collection(const char *, const char *,
				 struct collection *);
bool config_parse_uri_aux(const char *, const char *, struct collection *);

struct config *config_init_config(void);
struct collection *config_init_collection(void);

bool config_resolv_refuse(struct config *, struct collection *);
bool config_set_config_default(struct config *);
bool config_set_collection_default(struct config *, struct collection *);

struct config *
config_load_file(const char *cfname)
{
	struct config *cf;
	struct config_args *ca;

	logmsg_verbose("Parsing a file %s...", cfname);

	if ((ca = config_open(cfname)) == NULL)
		return (NULL);

	if ((cf = config_parse_file(ca)) == NULL) {
		config_close(ca);
		return (NULL);
	}

	if (!config_close(ca)) {
		config_destroy(cf);
		return (NULL);
	}

	return (cf);
}

struct config *
config_load_uri(const char *uri)
{
	static const char *URI_scheme_name = "cvsync://";
	static const size_t URI_scheme_length = 9;
	const char *sp = uri, *bp = sp + strlen(sp);
	struct config *cf;

	logmsg_verbose("Parsing a URI %s...", uri);

	while (sp < bp) {
		if (!isspace((int)(bp[-1])))
			break;
		bp--;
	}

	if ((size_t)(bp - sp) <= URI_scheme_length) {
		logmsg_err("%s: invalid URI scheme", uri);
		return (NULL);
	}
	if (strncasecmp(sp, URI_scheme_name, URI_scheme_length) != 0) {
		logmsg_err("%s: invalid URI scheme", uri);
		return (NULL);
	}
	sp += URI_scheme_length;

	if ((cf = config_parse_uri(sp, bp)) == NULL)
		return (NULL);

	return (cf);
}

void
config_destroy(struct config *cf)
{
	struct config *cf_next;

	while (cf != NULL) {
		cf_next = cf->cf_next;
		collection_destroy_all(cf->cf_collections);
		free(cf);
		cf = cf_next;
	}
}

struct config *
config_parse_file(struct config_args *ca)
{
	struct config *cfs = NULL, *cf, *prev;
	const struct token_keyword *key;
	FILE *fp = ca->ca_fp;

	lineno = 1;

	for (;;) {
		if (!token_skip_whitespace(fp)) {
			if (feof(fp) == 0)
				return (NULL);
			break;
		}

		if ((key = token_get_keyword(fp, config_keywords)) == NULL)
			return (NULL);

		switch (key->type) {
		case TOK_CONFIG:
			if ((cf = config_parse_file_config(ca)) == NULL) {
				config_destroy(cfs);
				return (NULL);
			}
			if (cfs == NULL) {
				cfs = cf;
			} else {
				prev = cfs;
				while (prev->cf_next != NULL)
					prev = prev->cf_next;
				prev->cf_next = cf;
			}
			break;
		default:
			logmsg_err("line %u: %s: invalid keyword", lineno,
				   key->name);
			config_destroy(cfs);
			return (NULL);
		}
	}

	return (cfs);
}

struct config *
config_parse_file_config(struct config_args *ca)
{
	const struct token_keyword *key;
	struct collection *cl;
	struct config *cf;
	FILE *fp = ca->ca_fp;

	if ((key = token_get_keyword(fp, config_keywords)) == NULL)
		return (NULL);
	if (key->type != TOK_LBRACE) {
		logmsg_err("line %u: missing '{' for the 'config'", lineno);
		return (NULL);
	}

	if ((cf = config_init_config()) == NULL)
		return (NULL);

	for (;;) {
		if ((key = token_get_keyword(fp, config_keywords)) == NULL) {
			config_destroy(cf);
			return (NULL);
		}

		if (key->type == TOK_RBRACE)
			break;
		ca->ca_key = key;

		switch (key->type) {
		case TOK_BASE:
			if (!config_parse_base(ca, cf)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_BASE_PREFIX:
			if (!config_parse_base_prefix(ca, cf)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_COLLECTION:
			if ((cl = config_parse_file_collection(ca)) == NULL) {
				config_destroy(cf);
				return (NULL);
			}
			if (!config_set_collection_default(cf, cl)) {
				config_destroy(cf);
				collection_destroy(cl);
				return (NULL);
			}
			if (!config_insert_collection(cf, cl)) {
				config_destroy(cf);
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_COMPRESS:
			if (cf->cf_compress != CVSYNC_COMPRESS_UNSPEC) {
				logmsg_err("line %u: found duplication of the "
					   "'%s'", lineno, key->name);
				config_destroy(cf);
				return (NULL);
			}
			cf->cf_compress = CVSYNC_COMPRESS_ZLIB;
			break;
		case TOK_HASH:
			if (!config_parse_hash(ca, cf)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_HOSTNAME:
			ca->ca_buffer = cf->cf_host;
			ca->ca_bufsize = sizeof(cf->cf_host);
			if (!config_set_string(ca)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_PORT:
			ca->ca_buffer = cf->cf_serv;
			ca->ca_bufsize = sizeof(cf->cf_serv);
			if (!config_set_string(ca)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_PROTOCOL:
			if (cf->cf_family != AF_UNSPEC) {
				logmsg_err("line %u: found duplication of the "
					   "'%s'", lineno, key->name);
				config_destroy(cf);
				return (NULL);
			}
			key = token_get_keyword(fp, protocol_keywords);
			if (key == NULL) {
				config_destroy(cf);
				return (NULL);
			}
			cf->cf_family = key->type;
			break;
		default:
			logmsg_err("line %u: %s: invalid keyword", lineno,
				   key->name);
			config_destroy(cf);
			return (NULL);
		}
	}

	if (!config_set_config_default(cf)) {
		config_destroy(cf);
		return (NULL);
	}

	for (cl = cf->cf_collections ; cl != NULL ; cl = cl->cl_next) {
		if (cvsync_release_pton(cl->cl_release) == CVSYNC_RELEASE_LIST)
			continue;

		if (!config_resolv_prefix(cf, cl, false /* rdwr */)) {
			config_destroy(cf);
			return (NULL);
		}
		if (!config_resolv_refuse(cf, cl)) {
			config_destroy(cf);
			return (NULL);
		}
		if (!config_resolv_scanfile(cf, cl)) {
			config_destroy(cf);
			return (NULL);
		}
	}

	return (cf);
}

struct collection *
config_parse_file_collection(struct config_args *ca)
{
	const struct token_keyword *key;
	struct collection *cl;
	FILE *fp = ca->ca_fp;

	if ((key = token_get_keyword(fp, config_keywords)) == NULL)
		return (NULL);
	if (key->type != TOK_LBRACE) {
		logmsg_err("line %u: missing '{' for the 'collection'",
			   lineno);
		return (NULL);
	}

	if ((cl = config_init_collection()) == NULL)
		return (NULL);

	for (;;) {
		if ((key = token_get_keyword(fp, config_keywords)) == NULL) {
			collection_destroy(cl);
			return (NULL);
		}

		if (key->type == TOK_RBRACE)
			break;
		ca->ca_key = key;

		switch (key->type) {
		case TOK_ERRORMODE:
			if (!config_parse_errormode(ca, cl)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_LOOSE:
			logmsg_err("line %u: '%s' is obsoleted", lineno,
				   key->name);
			if (cl->cl_errormode != CVSYNC_ERRORMODE_UNSPEC) {
				logmsg_err("line %u: conflicts '%s' with "
					   "'errormode'", lineno, key->name);
				collection_destroy(cl);
				return (NULL);
			}
			cl->cl_errormode = CVSYNC_ERRORMODE_FIXUP;
			break;
		case TOK_NAME:
			ca->ca_buffer = cl->cl_name;
			ca->ca_bufsize = sizeof(cl->cl_name);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_RELEASE:
			if (!config_parse_release(ca, cl)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_PREFIX:
			ca->ca_buffer = cl->cl_prefix;
			ca->ca_bufsize = sizeof(cl->cl_prefix);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_REFUSE:
			ca->ca_buffer = cl->cl_refuse_name;
			ca->ca_bufsize = sizeof(cl->cl_refuse_name);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_SCANFILE:
			ca->ca_buffer = cl->cl_scan_name;
			ca->ca_bufsize = sizeof(cl->cl_scan_name);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_UMASK:
			if (!config_parse_umask(ca, cl)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		default:
			logmsg_err("line %u: %s: invalid keyword", lineno,
				   key->name);
			collection_destroy(cl);
			return (NULL);
		}
	}

	if (strlen(cl->cl_name) == 0) {
		logmsg_err("no collection name");
		collection_destroy(cl);
		return (NULL);
	}
	if (strlen(cl->cl_release) == 0) {
		logmsg_err("collection %s: no release", cl->cl_name);
		collection_destroy(cl);
		return (NULL);
	}

	return (cl);
}

struct config *
config_parse_uri(const char *sp, const char *bp)
{
	const char *ep;
	struct collection *cl;
	struct config *cf;

	if ((cf = config_init_config()) == NULL)
		return (NULL);
	if ((cl = config_init_collection()) == NULL) {
		config_destroy(cf);
		return (NULL);
	}

	if ((ep = memchr(sp, '/', (size_t)(bp - sp))) == NULL)
		ep = bp;
	if (!config_parse_uri_host(sp, ep, cf)) {
		config_destroy(cf);
		collection_destroy(cl);
		return (NULL);
	}
	if ((sp = ep + 1) >= bp) {
		snprintf(cl->cl_name, sizeof(cl->cl_name), "%s", "all");
		snprintf(cl->cl_release, sizeof(cl->cl_release), "%s", "list");

		goto done;
	}

	if ((ep = memchr(sp, '?', (size_t)(bp - sp))) == NULL)
		ep = bp;
	if (!config_parse_uri_collection(sp, ep, cl)) {
		config_destroy(cf);
		collection_destroy(cl);
		return (NULL);
	}
	if ((sp = ep + 1) >= bp)
		goto done;

	if (!config_parse_uri_aux(sp, bp, cl)) {
		config_destroy(cf);
		collection_destroy(cl);
		return (NULL);
	}

done:
	if (!config_set_collection_default(cf, cl)) {
		config_destroy(cf);
		collection_destroy(cl);
		return (NULL);
	}

	switch (cvsync_release_pton(cl->cl_release)) {
	case CVSYNC_RELEASE_RCS:
		if (!config_resolv_prefix(cf, cl, true /* rdwr */)) {
			config_destroy(cf);
			collection_destroy(cl);
			return (NULL);
		}
		if (!config_resolv_scanfile(cf, cl)) {
			config_destroy(cf);
			collection_destroy(cl);
			return (NULL);
		}
		break;
	default:
		/* Nothing to do. */
		break;
	}

	if (!config_insert_collection(cf, cl)) {
		config_destroy(cf);
		collection_destroy(cl);
		return (NULL);
	}
	if (!config_set_config_default(cf)) {
		config_destroy(cf);
		return (NULL);
	}

	return (cf);
}

bool
config_parse_uri_host(const char *sp, const char *bp, struct config *cf)
{
	const char *sv_sp = sp, *ep;
	size_t sv_len = (size_t)(bp - sp), len;

	if (*sp == '[') {
		if (++sp >= bp) {
			logmsg_err("host: premature EOL");
			return (false);
		}
		if ((ep = memchr(sp, ']', (size_t)(bp - sp))) == NULL) {
			logmsg_err("host: missing ']'");
			return (false);
		}
		if ((len = (size_t)(ep - sp)) >= sizeof(cf->cf_host)) {
			logmsg_err("%.*s: host %.*s: %s", sv_len, sv_sp, len,
				   sp, strerror(ENAMETOOLONG));
			return (false);
		}
		if (len == 0) {
			logmsg_err("host: empty string");
			return (false);
		}
		(void)memcpy(cf->cf_host, sp, len);
		cf->cf_host[len] = '\0';

		sp = ep + 1;
	} else {
		if ((ep = memchr(sp, ':', (size_t)(bp - sp))) == NULL)
			ep = bp;
		if ((len = (size_t)(ep - sp)) >= sizeof(cf->cf_host)) {
			logmsg_err("%.*s: host %.*s: %s", sv_len, sv_sp, len,
				   sp, strerror(ENAMETOOLONG));
			return (false);
		}
		if (len == 0) {
			logmsg_err("host: empty string");
			return (false);
		}
		(void)memcpy(cf->cf_host, sp, len);
		cf->cf_host[len] = '\0';

		sp = ep;
	}
	if (sp >= bp)
		return (true);

	if (*sp++ != ':') {
		logmsg_err("%.*s: port: %s", sv_len, sv_sp, strerror(EINVAL));
		return (false);
	}
	if (sp >= bp)
		return (true);

	if ((len = (size_t)(bp - sp)) >= sizeof(cf->cf_serv)) {
		logmsg_err("%.*s: port %.*s: %s", sv_len, sv_sp, len, sp,
			   strerror(ENAMETOOLONG));
		return (false);
	}
	(void)memcpy(cf->cf_serv, sp, len);
	cf->cf_serv[len] = '\0';

	return (true);
}

bool
config_parse_uri_collection(const char *sp, const char *bp,
			    struct collection *cl)
{
	const char *ep;
	size_t len;

	if ((ep = memchr(sp, '/', (size_t)(bp - sp))) == NULL) {
		logmsg_err("name: premature EOL");
		return (false);
	}
	if ((len = (size_t)(ep - sp)) >= sizeof(cl->cl_name)) {
		logmsg_err("name %.*s: %s", len, sp, strerror(ENAMETOOLONG));
		return (false);
	}
	if (len == 0) {
		logmsg_err("name: empty string");
		return (false);
	}
	(void)memcpy(cl->cl_name, sp, len);
	cl->cl_name[len] = '\0';

	if ((sp = ep + 1) >= bp) {
		logmsg_err("release: premature EOL");
		return (false);
	}
	if ((ep = memchr(sp, '/', (size_t)(bp - sp))) == NULL) {
		logmsg_err("release: premature EOL");
		return (false);
	}
	if ((len = (size_t)(ep - sp)) >= sizeof(cl->cl_release)) {
		logmsg_err("release %.*s: %s", len, sp,
			   strerror(ENAMETOOLONG));
		return (false);
	}
	if (len == 0) {
		logmsg_err("release: empty string");
		return (false);
	}
	(void)memcpy(cl->cl_release, sp, len);
	cl->cl_release[len] = '\0';

	switch (cvsync_release_pton(cl->cl_release)) {
	case CVSYNC_RELEASE_LIST:
	case CVSYNC_RELEASE_RCS:
		if (++ep != bp) {
			logmsg_err("%.*s: unknown extra data", bp - ep, ep);
			return (false);
		}
		break;
	default:
		logmsg_err("release %s: unknown release type", cl->cl_release);
		return (false);
	}

	return (true);
}

bool
config_parse_uri_aux(const char *sp, const char *bp, struct collection *cl)
{
	const struct token_keyword *key;
	const char *ep, *sep;
	size_t len;

	while (sp < bp) {
		if ((ep = memchr(sp, '&', (size_t)(bp - sp))) == NULL)
			ep = bp;
		if ((sep = memchr(sp, '=', (size_t)(ep - sp))) == NULL)
			len = (size_t)(ep - sp);
		else
			len = (size_t)(sep - sp);
		if (len == 0) {
			logmsg_err("collection %s/%s: empty keyword",
				   cl->cl_name, cl->cl_release);
			return (false);
		}

		for (key = config_keywords ; key->name != NULL ; key++) {
			if ((len == key->namelen) &&
			    (memcmp(sp, key->name, len) == 0)) {
				break;
			}
		}
		if (key->name == NULL) {
			logmsg_err("collection %s/%s: %.*s: unknown keyword",
				   cl->cl_name, cl->cl_release, len, sp);
			return (false);
		}

		switch (key->type) {
		case TOK_PREFIX:
			if (sep == NULL) {
				logmsg_err("collection %s/%s: %s: missing a "
					   "separator '='", cl->cl_name,
					   cl->cl_release, key->name);
				return (false);
			}
			if ((sp = sep + 1) >= ep) {
				logmsg_err("collection %s/%s: %s: empty",
					   cl->cl_name, cl->cl_release,
					   key->name);
				return (false);
			}
			cl->cl_prefixlen = (size_t)(ep - sp);
			if (cl->cl_prefixlen >= sizeof(cl->cl_prefix)) {
				logmsg_err("collection %s/%s: %s: %s",
					   cl->cl_name, cl->cl_release,
					   key->name, strerror(ENAMETOOLONG));
				return (false);
			}
			if (cl->cl_prefixlen == 0) {
				logmsg_err("collection %s/%s: %s: empty "
					   "string", cl->cl_name,
					   cl->cl_release, key->name);
				return (false);
			}
			(void)memcpy(cl->cl_prefix, sp, cl->cl_prefixlen);
			cl->cl_prefix[cl->cl_prefixlen] = '\0';

			break;
		case TOK_SCANFILE:
			if (sep == NULL) {
				logmsg_err("collection %s/%s: %s: missing a "
					   "separator '='", cl->cl_name,
					   cl->cl_release, key->name);
				return (false);
			}
			if ((sp = sep + 1) >= ep) {
				logmsg_err("collection %s/%s: %s: empty",
					   cl->cl_name, cl->cl_release,
					   key->name);
				return (false);
			}
			len = (size_t)(ep - sp);
			if (len >= sizeof(cl->cl_scan_name)) {
				logmsg_err("collection %s/%s: %s: %s",
					   cl->cl_name, cl->cl_release,
					   key->name, strerror(ENAMETOOLONG));
				return (false);
			}
			if (len == 0) {
				logmsg_err("collection %s/%s: %s: empty "
					   "string", cl->cl_name,
					   cl->cl_release, key->name);
				return (false);
			}
			(void)memcpy(cl->cl_scan_name, sp, len);
			cl->cl_scan_name[len] = '\0';

			break;
		default:
			logmsg_err("collection %s/%s: %.*s: invalid keyword",
				   cl->cl_name, cl->cl_release, len, sp);
			return (false);
		}

		sp = ep + 1;
	}

	return (true);
}

struct config *
config_init_config(void)
{
	struct config *cf;

	if ((cf = malloc(sizeof(*cf))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	(void)memset(cf, 0, sizeof(*cf));

	cf->cf_family = AF_UNSPEC;
	cf->cf_compress = CVSYNC_COMPRESS_UNSPEC;
	cf->cf_hash = HASH_UNSPEC;

	return (cf);
}

struct collection *
config_init_collection(void)
{
	struct collection *cl;

	if ((cl = malloc(sizeof(*cl))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	(void)memset(cl, 0, sizeof(*cl));

	cl->cl_errormode = CVSYNC_ERRORMODE_UNSPEC;
	cl->cl_umask = CVSYNC_UMASK_UNSPEC;

	return (cl);
}

bool
config_resolv_refuse(struct config *cf, struct collection *cl)
{
	char path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	int wn;

	if (strlen(cl->cl_refuse_name) == 0)
		return (true);

	if (cl->cl_refuse_name[0] != '/') {
		if (strlen(cf->cf_base) == 0) {
			logmsg_err("collection %s/%s: refuse %s: must be an "
				   "absolute path", cl->cl_name,
				   cl->cl_release, cl->cl_refuse_name);
			return (false);
		}
		wn = snprintf(path, sizeof(path), "%s/%s", cf->cf_base,
			      cl->cl_refuse_name);
	} else {
		wn = snprintf(path, sizeof(path), "%s", cl->cl_refuse_name);
	}
	if ((wn <= 0) || ((size_t)wn >= sizeof(path))) {
		logmsg_err("collection %s/%s: refuse %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_refuse_name,
			   strerror(EINVAL));
		return (false);
	}

	if ((cl->cl_refuse = refuse_open(path)) == NULL)
		return (false);

	return (true);
}

bool
config_set_config_default(struct config *cf)
{
	if (strlen(cf->cf_serv) == 0) {
		snprintf(cf->cf_serv, sizeof(cf->cf_serv), "%s",
			 CVSYNC_DEFAULT_PORT);
	}
	if (cf->cf_compress == CVSYNC_COMPRESS_UNSPEC)
		cf->cf_compress = CVSYNC_COMPRESS_NO;
	if (cf->cf_hash == HASH_UNSPEC)
		cf->cf_hash = HASH_DEFAULT_TYPE;

	if (strlen(cf->cf_host) == 0) {
		logmsg_err("A remote host name is not specified");
		return (false);
	}
	if (cf->cf_collections == NULL) {
		logmsg_err("host %s port %s: no collections",
			   cf->cf_host, cf->cf_serv);
		return (false);
	}

	return (true);
}

bool
config_set_collection_default(struct config *cf, struct collection *cl)
{
	struct collection *prev;
	int type;

	switch (cvsync_release_pton(cl->cl_release)) {
	case CVSYNC_RELEASE_LIST:
		if (cf->cf_collections != NULL) {
			logmsg_err("release 'list' is one only");
			return (false);
		}
		if (cvsync_list_pton(cl->cl_name) == CVSYNC_LIST_UNKNOWN) {
			logmsg_err("release 'list': %s: unknown name",
				   cl->cl_name);
			return (false);
		}
		break;
	case CVSYNC_RELEASE_RCS:
		if (cl->cl_errormode == CVSYNC_ERRORMODE_UNSPEC)
			cl->cl_errormode = CVSYNC_ERRORMODE_ABORT;
		if (cl->cl_umask == CVSYNC_UMASK_UNSPEC)
			cl->cl_umask = CVSYNC_UMASK_RCS;

		if ((prev = cf->cf_collections) != NULL) {
			type = cvsync_release_pton(prev->cl_release);
			if (type == CVSYNC_RELEASE_LIST) {
				logmsg_err("release 'list' is one only");
				return (false);
			}
		}
		break;
	default:
		/* NOTREACHED */
		return (false);
	}

	return (true);
}
