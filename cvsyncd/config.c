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
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
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
#include "token.h"

#include "config_common.h"
#include "defs.h"

#ifndef CVSYNCD_DEFAULT_MAXCLIENTS
#define	CVSYNCD_DEFAULT_MAXCLIENTS	(16)
#endif /* CVSYNCD_DEFAULT_MAXCLIENTS */

#define	CVSYNCD_MIN_MAXCLIENTS		(1)
#define	CVSYNCD_MAX_MAXCLIENTS		(2048)

enum {
	TOK_ACL,
	TOK_BASE,
	TOK_BASE_PREFIX,
	TOK_COLLECTION,
	TOK_COMMENT,
	TOK_CONFIG,
	TOK_DISTFILE,
	TOK_ERRORMODE,
	TOK_HALTFILE,
	TOK_HASH,
	TOK_LBRACE,
	TOK_LISTEN,
	TOK_LOOSE,
	TOK_MAXCLIENTS,
	TOK_NAME,
	TOK_NOFOLLOW,
	TOK_PIDFILE,
	TOK_PORT,
	TOK_PREFIX,
	TOK_RBRACE,
	TOK_RELEASE,
	TOK_SCANFILE,
	TOK_SUPER,
	TOK_UMASK,

	TOK_UNKNOWN
};

static const struct token_keyword config_keywords[] = {
	{ "{",			1,	TOK_LBRACE },
	{ "}",			1,	TOK_RBRACE },
	{ "access",		6,	TOK_ACL },
	{ "base",		4,	TOK_BASE },
	{ "base-prefix",	11,	TOK_BASE_PREFIX },
	{ "collection",		10,	TOK_COLLECTION },
	{ "comment",		7,	TOK_COMMENT },
	{ "config",		6,	TOK_CONFIG },
	{ "distfile",		8,	TOK_DISTFILE },
	{ "errormode",		9,	TOK_ERRORMODE },
	{ "haltfile",		8,	TOK_HALTFILE },
	{ "hash",		4,	TOK_HASH },
	{ "listen",		6,	TOK_LISTEN },
	{ "loose",		5,	TOK_LOOSE },
	{ "maxclients",		10,	TOK_MAXCLIENTS },
	{ "name",		4,	TOK_NAME },
	{ "nofollow",		8,	TOK_NOFOLLOW },
	{ "pidfile",		7,	TOK_PIDFILE },
	{ "port",		4,	TOK_PORT },
	{ "prefix",		6,	TOK_PREFIX },
	{ "release",		7,	TOK_RELEASE },
	{ "scanfile",		8,	TOK_SCANFILE },
	{ "super",		5,	TOK_SUPER },
	{ "umask",		5,	TOK_UMASK },
	{ NULL,			0,	TOK_UNKNOWN },
};

struct config *config_parse(struct config_args *);
struct config *config_parse_config(struct config_args *);
struct collection *config_parse_collection(struct config_args *);

bool config_resolv_distfile(struct config *, struct collection *);
bool config_resolv_super(struct config *, struct collection *);
bool config_set_collection_default_rcs(struct collection *);
bool config_set_collection_super(struct collection *, struct collection *);

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

struct config *
config_load(const char *cfname)
{
	struct config *cf;
	struct config_args *ca;
	struct config_include *ci;
	int wn;

	if ((ca = config_open(cfname)) == NULL)
		return (NULL);

	if ((cf = config_parse(ca)) == NULL) {
		config_close(ca);
		return (NULL);
	}
	cf->cf_refcnt = 0;

	if ((ci = malloc(sizeof(*ci))) == NULL) {
		config_close(ca);
		config_destroy(cf);
		return (NULL);
	}
	ci->ci_next = cf->cf_includes;
	cf->cf_includes = ci;
	wn = snprintf(ci->ci_name, sizeof(ci->ci_name), "%s", cfname);
	if ((wn <= 0) || ((size_t)wn >= sizeof(ci->ci_name))) {
		logmsg_err("%s: %s", cfname, strerror(EINVAL));
		config_close(ca);
		config_destroy(cf);
		return (NULL);
	}
	ci->ci_mtime = ca->ca_mtime;

	if (!config_close(ca)) {
		config_destroy(cf);
		return (NULL);
	}

	return (cf);
}

void
config_destroy(struct config *cf)
{
	struct config_include *ci, *next;

	if (cf != NULL) {
		ci = cf->cf_includes;
		while (ci != NULL) {
			next = ci->ci_next;
			free(ci);
			ci = next;
		}
		collection_destroy_all(cf->cf_collections);
		free(cf);
	}
}

void
config_acquire(struct config *cf)
{
	pthread_mutex_lock(&mtx);
	cf->cf_refcnt++;
	pthread_mutex_unlock(&mtx);
}

void
config_revoke(struct config *cf)
{
	pthread_mutex_lock(&mtx);
	if (--cf->cf_refcnt < 0)
		config_destroy(cf);
	pthread_mutex_unlock(&mtx);
}

bool
config_ischanged(struct config *cf)
{
	struct config_include *ci;
	struct stat st;

	for (ci = cf->cf_includes ; ci != NULL ; ci = ci->ci_next) {
		if (stat(ci->ci_name, &st) == -1) {
			logmsg_err("%s: %s", ci->ci_name, strerror(errno));
			continue;
		}
		if (st.st_mtime != ci->ci_mtime) {
			ci->ci_mtime = st.st_mtime;
			return (true);
		}
	}

	return (false);
}

struct config *
config_parse(struct config_args *ca)
{
	const struct token_keyword *key;
	struct config *cf = NULL;
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
			if (cf != NULL) {
				logmsg_err("line %u: duplication", lineno);
				config_destroy(cf);
				return (NULL);
			}
			if ((cf = config_parse_config(ca)) == NULL)
				return (NULL);
			break;
		default:
			logmsg_err("line %u: %s: invalid keyword", lineno,
				   key->name);
			if (cf != NULL)
				config_destroy(cf);
			return (NULL);
		}
	}

	return (cf);
}

struct config *
config_parse_config(struct config_args *ca)
{
	const struct token_keyword *key;
	struct collection *cl;
	struct config *cf;
	FILE *fp = ca->ca_fp;
	unsigned long ul;

	if ((key = token_get_keyword(fp, config_keywords)) == NULL)
		return (NULL);
	if (key->type != TOK_LBRACE) {
		logmsg_err("line %u: missing '{' for the 'config'", lineno);
		return (NULL);
	}

	if ((cf = malloc(sizeof(*cf))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	(void)memset(cf, 0, sizeof(*cf));
	cf->cf_maxclients = (size_t)-1;
	cf->cf_hash = HASH_UNSPEC;

	for (;;) {
		if ((key = token_get_keyword(fp, config_keywords)) == NULL) {
			config_destroy(cf);
			return (NULL);
		}

		if (key->type == TOK_RBRACE)
			break;
		ca->ca_key = key;

		switch (key->type) {
		case TOK_ACL:
			ca->ca_buffer = cf->cf_access_name;
			ca->ca_bufsize = sizeof(cf->cf_access_name);
			if (!config_set_string(ca)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
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
			if ((cl = config_parse_collection(ca)) == NULL) {
				config_destroy(cf);
				return (NULL);
			}
			if (!config_insert_collection(cf, cl)) {
				collection_destroy(cl);
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_HALTFILE:
			ca->ca_buffer = cf->cf_halt_name;
			ca->ca_bufsize = sizeof(cf->cf_halt_name);
			if (!config_set_string(ca)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_HASH:
			if (!config_parse_hash(ca, cf)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_LISTEN:
			ca->ca_buffer = cf->cf_addr;
			ca->ca_bufsize = sizeof(cf->cf_addr);
			if (!config_set_string(ca)) {
				config_destroy(cf);
				return (NULL);
			}
			break;
		case TOK_MAXCLIENTS:
			if (cf->cf_maxclients != (size_t)-1) {
				logmsg_err("line %u: found duplication of the "
					   "'%s'", lineno, key->name);
				config_destroy(cf);
				return (NULL);
			}
			if (!token_get_number(fp, &ul)) {
				config_destroy(cf);
				return (NULL);
			}
			if ((ul < CVSYNCD_MIN_MAXCLIENTS) ||
			    (ul > CVSYNCD_MAX_MAXCLIENTS)) {
				logmsg_err("line %u: %s %lu: %s", lineno,
					   key->name, ul, strerror(ERANGE));
				config_destroy(cf);
				return (NULL);
			}
			cf->cf_maxclients = (size_t)ul;
			break;
		case TOK_PIDFILE:
			ca->ca_buffer = cf->cf_pid_name;
			ca->ca_bufsize = sizeof(cf->cf_pid_name);
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
		default:
			logmsg_err("line %u: %s: invalid keyword", lineno,
				   key->name);
			config_destroy(cf);
			return (NULL);
		}
	}

	if (strlen(cf->cf_serv) == 0) {
		snprintf(cf->cf_serv, sizeof(cf->cf_serv), "%s",
			 CVSYNC_DEFAULT_PORT);
	}
	if (cf->cf_maxclients == (size_t)-1)
		cf->cf_maxclients = CVSYNCD_DEFAULT_MAXCLIENTS;
	if (cf->cf_hash == HASH_UNSPEC)
		cf->cf_hash = HASH_DEFAULT_TYPE;

	if ((strlen(cf->cf_access_name) != 0) &&
	    (cf->cf_access_name[0] != '/')) {
		logmsg_err("access %s: must be the absolute path",
			   cf->cf_access_name);
		config_destroy(cf);
		return (NULL);
	}
	if ((strlen(cf->cf_halt_name) != 0) && (cf->cf_halt_name[0] != '/')) {
		logmsg_err("haltfile %s: must be the absolute path",
			   cf->cf_halt_name);
		config_destroy(cf);
		return (NULL);
	}
	if ((strlen(cf->cf_pid_name) != 0) && (cf->cf_pid_name[0] != '/')) {
		logmsg_err("pidfile %s: must be the absolute path",
			   cf->cf_pid_name);
		config_destroy(cf);
		return (NULL);
	}

	if (cf->cf_collections == NULL) {
		logmsg_err("no collections");
		config_destroy(cf);
		return (NULL);
	}

	for (cl = cf->cf_collections ; cl != NULL ; cl = cl->cl_next) {
		if (!config_resolv_distfile(cf, cl)) {
			config_destroy(cf);
			return (NULL);
		}
		if (!config_resolv_prefix(cf, cl, true /* rdonly */)) {
			config_destroy(cf);
			return (NULL);
		}
		if (!config_resolv_scanfile(cf, cl)) {
			config_destroy(cf);
			return (NULL);
		}
		if ((strlen(cl->cl_super_name) != 0) &&
		    !config_resolv_super(cf, cl)) {
			config_destroy(cf);
			return (NULL);
		}
	}
	for (cl = cf->cf_collections ; cl != NULL ; cl = cl->cl_next) {
		if (cl->cl_super == NULL)
			continue;
		if (strcmp(cl->cl_release, cl->cl_super->cl_release) != 0)
			continue;

		if (!config_set_collection_super(cl, cl->cl_super)) {
			config_destroy(cf);
			return (NULL);
		}
	}

	return (cf);
}

struct collection *
config_parse_collection(struct config_args *ca)
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

	if ((cl = malloc(sizeof(*cl))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	(void)memset(cl, 0, sizeof(*cl));
	cl->cl_errormode = CVSYNC_ERRORMODE_UNSPEC;
	cl->cl_symfollow = true;
	cl->cl_umask = CVSYNC_UMASK_UNSPEC;

	for (;;) {
		if ((key = token_get_keyword(fp, config_keywords)) == NULL) {
			collection_destroy(cl);
			return (NULL);
		}

		if (key->type == TOK_RBRACE)
			break;
		ca->ca_key = key;

		switch (key->type) {
		case TOK_COMMENT:
			ca->ca_buffer = cl->cl_comment;
			ca->ca_bufsize = sizeof(cl->cl_comment);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_DISTFILE:
			ca->ca_buffer = cl->cl_dist_name;
			ca->ca_bufsize = sizeof(cl->cl_dist_name);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
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
			ca->ca_buffer =  cl->cl_name;
			ca->ca_bufsize = sizeof(cl->cl_name);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_NOFOLLOW:
			if (!cl->cl_symfollow) {
				logmsg_err("line %u: found duplication of the "
					   "'%s'", lineno, key->name);
				collection_destroy(cl);
				return (NULL);
			}
			cl->cl_symfollow = false;
			break;
		case TOK_PREFIX:
			ca->ca_buffer = cl->cl_prefix;
			ca->ca_bufsize = sizeof(cl->cl_prefix);
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
		case TOK_SCANFILE:
			ca->ca_buffer = cl->cl_scan_name;
			ca->ca_bufsize = sizeof(cl->cl_scan_name);
			if (!config_set_string(ca)) {
				collection_destroy(cl);
				return (NULL);
			}
			break;
		case TOK_SUPER:
			ca->ca_buffer = cl->cl_super_name;
			ca->ca_bufsize = sizeof(cl->cl_super_name);
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

	switch (cvsync_release_pton(cl->cl_release)) {
	case CVSYNC_RELEASE_RCS:
		if (!config_set_collection_default_rcs(cl)) {
			collection_destroy(cl);
			return (NULL);
		}
		break;
	default:
		/* NOTREACHED */
		return (NULL);
	}

	if (cl->cl_errormode == CVSYNC_ERRORMODE_UNSPEC)
		cl->cl_errormode = CVSYNC_ERRORMODE_ABORT;

	return (cl);
}

bool
config_resolv_distfile(struct config *cf, struct collection *cl)
{
	char path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	int wn;

	if (strlen(cl->cl_dist_name) == 0)
		return (true);

	if (cl->cl_dist_name[0] != '/') {
		if (strlen(cf->cf_base) == 0) {
			logmsg_err("collection %s/%s: distfile %s: must be an "
				   "absolute path", cl->cl_name,
				   cl->cl_release, cl->cl_dist_name);
			return (false);
		}
		wn = snprintf(path, sizeof(path), "%s/%s", cf->cf_base,
			      cl->cl_dist_name);
	} else {
		wn = snprintf(path, sizeof(path), "%s", cl->cl_dist_name);
	}
	if ((wn <= 0) || ((size_t)wn >= sizeof(path))) {
		logmsg_err("collection %s/%s: distfile %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_dist_name, strerror(EINVAL));
		return (false);
	}
	wn = snprintf(cl->cl_dist_name, sizeof(cl->cl_dist_name), "%s", path);
	if ((wn <= 0) || ((size_t)wn >= sizeof(cl->cl_dist_name))) {
		logmsg_err("collection %s/%s: distfile %s: %s", cl->cl_name,
			   cl->cl_release, path, strerror(EINVAL));
		return (false);
	}

	return (true);
}

bool
config_resolv_super(struct config *cf, struct collection *cl)
{
	struct collection *super;
	char *name = cl->cl_super_name;
	int type = cvsync_release_pton(cl->cl_release);
	bool found = false;

	for (super = cf->cf_collections ;
	     super != NULL ;
	     super = super->cl_next) {
		if (super == cl)
			continue;
		if (strcasecmp(super->cl_name, name) != 0)
			continue;
		switch (cvsync_release_pton(super->cl_release)) {
		case CVSYNC_RELEASE_RCS:
			if (type != CVSYNC_RELEASE_RCS)
				break;
			found = true;
			break;
		default:
			/* NOTREACHED */
			break;
		}
		if (found)
			break;
	}
	if (!found) {
		logmsg_err("Not found such a super collection: %s/%s", name,
			   cl->cl_release);
		return (false);
	}

	cl->cl_super = super;

	return (true);
}

bool
config_set_collection_default_rcs(struct collection *cl)
{
	bool found_errors = false;

	if (strlen(cl->cl_super_name) != 0) {
		if (cl->cl_umask != CVSYNC_UMASK_UNSPEC) {
			logmsg_err("collection %s/%s: invalid 'umask'",
				   cl->cl_name, cl->cl_release);
			found_errors = true;
		}
		if (!cl->cl_symfollow) {
			logmsg_err("collection %s/%s: invalid 'nofollow'",
				   cl->cl_name, cl->cl_release);
			found_errors = true;
		}
		if (found_errors)
			return (false);
	}

	if (cl->cl_umask == CVSYNC_UMASK_UNSPEC)
		cl->cl_umask = CVSYNC_UMASK_RCS;

	return (true);
}

bool
config_set_collection_super(struct collection *cl, struct collection *super)
{
	while (super->cl_super != NULL) {
		if (cl == super) {
			logmsg_err("Detect cyclic dependency of 'super' from "
				   "%s/%s", cl->cl_name, cl->cl_release);
			return (false);
		}
		super = super->cl_super;
	}

	if ((super->cl_prefixlen >= cl->cl_prefixlen) ||
	    (cl->cl_prefix[super->cl_prefixlen - 1] != '/') ||
	    (memcmp(super->cl_prefix, cl->cl_prefix,
		    super->cl_prefixlen) != 0)) {
		logmsg_err("prefix %s of %s/%s must be sub-directory of "
			   "prefix %s of %s/%s", cl->cl_prefix, cl->cl_name,
			   cl->cl_release, super->cl_prefix, super->cl_name,
			   super->cl_release);
		return (false);
	}

	if ((strlen(cl->cl_dist_name) == 0) &&
	    (strlen(super->cl_dist_name) != 0)) {
		snprintf(cl->cl_dist_name, sizeof(cl->cl_dist_name), "%s",
			 super->cl_dist_name);
	}
	if ((strlen(cl->cl_scan_name) == 0) &&
	    (strlen(super->cl_scan_name) != 0)) {
		snprintf(cl->cl_scan_name, sizeof(cl->cl_scan_name), "%s",
			 super->cl_scan_name);
	}

	cl->cl_rprefixlen = cl->cl_prefixlen - super->cl_prefixlen - 1;
	if (cl->cl_rprefixlen > sizeof(cl->cl_rprefix))
		return (false);
	if (cl->cl_rprefixlen > 0) {
		(void)memcpy(cl->cl_rprefix,
			     &cl->cl_prefix[super->cl_prefixlen],
			     cl->cl_rprefixlen);
		cl->cl_rprefix[cl->cl_rprefixlen] = '/';
	}
	cl->cl_prefixlen = super->cl_prefixlen;
	(void)memcpy(cl->cl_prefix, super->cl_prefix, cl->cl_prefixlen);
	cl->cl_prefix[cl->cl_prefixlen] = '\0';

	cl->cl_super = super;
	cl->cl_errormode = super->cl_errormode;

	switch (cvsync_release_pton(cl->cl_release)) {
	case CVSYNC_RELEASE_RCS:
		cl->cl_symfollow = super->cl_symfollow;
		cl->cl_umask = super->cl_umask;
		break;
	default:
		/* NOTREACHED */
		break;
	}

	return (true);
}
