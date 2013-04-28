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
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_stdlib.h"
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

/* errormode */
static const struct token_keyword errormode_keywords[] = {
	{ "abort",	5,	CVSYNC_ERRORMODE_ABORT },
	{ "fixup",	5,	CVSYNC_ERRORMODE_FIXUP },
	{ "ignore",	6,	CVSYNC_ERRORMODE_IGNORE },
	{ NULL,		0,	CVSYNC_ERRORMODE_UNSPEC },
};

struct config_args *
config_open(const char *cfname)
{
	struct config_args *ca;
	struct stat st;
	int fd;

	if ((ca = malloc(sizeof(*ca))) == NULL) {
		logmsg_err("%s: %s", cfname, strerror(errno));
		return (NULL);
	}

	if ((fd = open(cfname, O_RDONLY, 0)) == -1) {
		logmsg_err("%s: %s", cfname, strerror(errno));
		free(ca);
		return (NULL);
	}
	if (fstat(fd, &st) == -1) {
		logmsg_err("%s: %s", cfname, strerror(errno));
		(void)close(fd);
		free(ca);
		return (NULL);
	}
	ca->ca_mtime = st.st_mtime;
	if ((ca->ca_fp = fdopen(fd, "r")) == NULL) {
		logmsg_err("%s: %s", cfname, strerror(errno));
		(void)close(fd);
		free(ca);
		return (NULL);
	}

	return (ca);
}

bool
config_close(struct config_args *ca)
{
	FILE *fp = ca->ca_fp;

	free(ca);

	if (fclose(fp) == EOF) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	return (true);
}

bool
config_insert_collection(struct config *cf, struct collection *cl)
{
	struct collection *prev = cf->cf_collections;

	if (prev == NULL) {
		cf->cf_collections = cl;
		return (true);
	}

	for (;;) {
		if ((strcasecmp(prev->cl_name, cl->cl_name) == 0) &&
		    (strcasecmp(prev->cl_release, cl->cl_release) == 0)) {
			logmsg_err("collection %s/%s: duplicate", cl->cl_name,
				   cl->cl_release);
			return (false);
		}
		if (prev->cl_next == NULL) {
			prev->cl_next = cl;
			break;
		}
		prev = prev->cl_next;
	}

	return (true);
}

bool
config_parse_base(struct config_args *ca, struct config *cf)
{
	struct stat st;

	ca->ca_buffer = cf->cf_base;
	ca->ca_bufsize = sizeof(cf->cf_base);

	if (!config_set_string(ca))
		return (false);

	if (cf->cf_base[0] != '/') {
		logmsg_err("line %u: base %s: must be the absolute path",
			   lineno, cf->cf_base);
		return (false);
	}

	if (stat(cf->cf_base, &st) == -1) {
		logmsg_err("line %u: base %s: %s", lineno, cf->cf_base,
			   strerror(errno));
		return (false);
	}
	if (!S_ISDIR(st.st_mode)) {
		logmsg_err("line %u: base %s: %s", lineno, cf->cf_base,
			   strerror(ENOTDIR));
		return (false);
	}

	return (true);
}

bool
config_parse_base_prefix(struct config_args *ca, struct config *cf)
{
	struct stat st;

	ca->ca_buffer = cf->cf_base_prefix;
	ca->ca_bufsize = sizeof(cf->cf_base_prefix);

	if (!config_set_string(ca))
		return (false);

	if (cf->cf_base_prefix[0] != '/') {
		logmsg_err("line %u: base-prefix %s: must be the absolute "
			   "path", lineno, cf->cf_base_prefix);
		return (false);
	}

	if (stat(cf->cf_base_prefix, &st) == -1) {
		logmsg_err("line %u: base-prefix %s: %s", lineno,
			   cf->cf_base_prefix, strerror(errno));
		return (false);
	}
	if (!S_ISDIR(st.st_mode)) {
		logmsg_err("line %u: base-prefix %s: %s", lineno,
			   cf->cf_base_prefix, strerror(ENOTDIR));
		return (false);
	}

	return (true);
}

bool
config_parse_errormode(struct config_args *ca, struct collection *cl)
{
	const struct token_keyword *key;

	if (cl->cl_errormode != CVSYNC_ERRORMODE_UNSPEC) {
		logmsg_err("line %u: found duplication of the 'errormode'",
			   lineno);
		return (false);
	}

	if ((key = token_get_keyword(ca->ca_fp, errormode_keywords)) == NULL)
		return (false);

	cl->cl_errormode = key->type;

	return (true);
}

bool
config_parse_hash(struct config_args *ca, struct config *cf)
{
	struct token *tk = &ca->ca_token;

	if (cf->cf_hash != HASH_UNSPEC) {
		logmsg_err("line %u: found duplication of the 'hash'", lineno);
		return (false);
	}

	if (!token_get_string(ca->ca_fp, tk))
		return (false);

	if ((cf->cf_hash = hash_pton(tk->token, tk->length)) == HASH_UNSPEC) {
		logmsg_err("line %u: %s: unsupported hash type", lineno,
			   tk->token);
		return (false);
	}

	return (true);
}

bool
config_parse_release(struct config_args *ca, struct collection *cl)
{
	ca->ca_buffer = cl->cl_release;
	ca->ca_bufsize = sizeof(cl->cl_release);

	if (!config_set_string(ca))
		return (false);

	if (cvsync_release_pton(cl->cl_release) == CVSYNC_RELEASE_UNKNOWN) {
		logmsg_err("line %u: %s: unsupported release type", lineno,
			   cl->cl_release);
		return (false);
	}

	return (true);
}

bool
config_parse_umask(struct config_args *ca, struct collection *cl)
{
	unsigned long ul;

	if (cl->cl_umask != CVSYNC_UMASK_UNSPEC) {
		logmsg_err("line %u: found duplication of the 'umask'",
			   lineno);
		return (false);
	}

	if (!token_get_number(ca->ca_fp, &ul))
		return (false);

	if (ul & ~CVSYNC_ALLPERMS) {
		logmsg_err("line %u: umask %lu: %s", lineno, ul,
			   strerror(ERANGE));
		return (false);
	}

	cl->cl_umask = (uint16_t)ul;

	return (true);
}

bool
config_resolv_prefix(struct config *cf, struct collection *cl, bool rdonly)
{
	struct stat st;
	char path[PATH_MAX];
	int fd, wn;

	if (cl->cl_prefix[0] != '/') {
		if (strlen(cf->cf_base_prefix) == 0) {
			if (strlen(cl->cl_prefix) == 0) {
				logmsg_err("collection %s/%s: no prefix",
					   cl->cl_name, cl->cl_release);
			} else {
				logmsg_err("collection %s/%s: prefix %s: "
					   "must be the absolute path",
					   cl->cl_name, cl->cl_release,
					   cl->cl_prefix);
			}
			return (false);
		}
		wn = snprintf(path, sizeof(path), "%s/%s", cf->cf_base_prefix,
			      cl->cl_prefix);
	} else {
		wn = snprintf(path, sizeof(path), "%s", cl->cl_prefix);
	}
	if ((wn <= 0) || ((size_t)wn >= sizeof(path))) {
		logmsg_err("collection %s/%s: prefix %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_prefix, strerror(EINVAL));
		return (false);
	}

	if (stat(path, &st) == -1) {
		logmsg_err("collection %s/%s: prefix %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_prefix, strerror(errno));
		return (false);
	}
	if (!S_ISDIR(st.st_mode)) {
		logmsg_err("collection %s/%s: prefix %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_prefix, strerror(ENOTDIR));
		return (false);
	}
	if (realpath(path, cl->cl_prefix) == NULL) {
		logmsg_err("collection %s/%s: prefix %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_prefix, strerror(errno));
		return (false);
	}
	cl->cl_prefixlen = strlen(cl->cl_prefix);
	if (cl->cl_prefixlen + 1 >= sizeof(cl->cl_prefix)) {
		logmsg_err("collection %s/%s: prefix %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_prefix,
			   strerror(ENAMETOOLONG));
		return (false);
	}
	cl->cl_prefix[cl->cl_prefixlen++] = '/';
	cl->cl_prefix[cl->cl_prefixlen] = '\0';

	if (!rdonly) {
		(void)memcpy(&cl->cl_prefix[cl->cl_prefixlen], CVSYNC_TMPFILE,
			     CVSYNC_TMPFILE_LEN);
		cl->cl_prefix[cl->cl_prefixlen + CVSYNC_TMPFILE_LEN] = '\0';

		if ((fd = mkstemp(cl->cl_prefix)) == -1) {
			cl->cl_prefix[cl->cl_prefixlen] = '\0';
			logmsg_err("collection %s/%s: prefix %s: %s",
				   cl->cl_name, cl->cl_release, cl->cl_prefix,
				   strerror(errno));
			return (false);
		}
		if (unlink(cl->cl_prefix) == -1) {
			cl->cl_prefix[cl->cl_prefixlen] = '\0';
			logmsg_err("collection %s/%s: prefix %s: %s",
				   cl->cl_name, cl->cl_release, cl->cl_prefix,
				   strerror(errno));
			(void)close(fd);
			return (false);
		}
		if (close(fd) == -1) {
			cl->cl_prefix[cl->cl_prefixlen] = '\0';
			logmsg_err("collection %s/%s: prefix %s: %s",
				   cl->cl_name, cl->cl_release, cl->cl_prefix,
				   strerror(errno));
			return (false);
		}

		cl->cl_prefix[cl->cl_prefixlen] = '\0';
	}

	return (true);
}

bool
config_resolv_scanfile(struct config *cf, struct collection *cl)
{
	char path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	int wn;

	if (strlen(cl->cl_scan_name) == 0)
		return (true);

	if (cl->cl_scan_name[0] != '/') {
		if (strlen(cf->cf_base) == 0) {
			logmsg_err("collection %s/%s: scanfile %s: must be an "
				   "absolute path", cl->cl_name,
				   cl->cl_release, cl->cl_scan_name);
			return (false);
		}
		wn = snprintf(path, sizeof(path), "%s/%s", cf->cf_base,
			      cl->cl_scan_name);
	} else {
		wn = snprintf(path, sizeof(path), "%s", cl->cl_scan_name);
	}
	if ((wn <= 0) || ((size_t)wn >= sizeof(path))) {
		logmsg_err("collection %s/%s: scanfile %s: %s", cl->cl_name,
			   cl->cl_release, cl->cl_scan_name, strerror(EINVAL));
		return (false);
	}
	wn = snprintf(cl->cl_scan_name, sizeof(cl->cl_scan_name), "%s", path);
	if ((wn <= 0) || ((size_t)wn >= sizeof(cl->cl_scan_name))) {
		logmsg_err("collection %s/%s: scanfile %s: %s", cl->cl_name,
			   cl->cl_release, path, strerror(EINVAL));
		return (false);
	}

	return (true);
}

bool
config_set_string(struct config_args *ca)
{
	struct token *tk = &ca->ca_token;

	if (!token_get_string(ca->ca_fp, tk))
		return (false);

	if (strlen(ca->ca_buffer) != 0) {
		logmsg_err("line %u: found duplication of the '%s'",
			   lineno, ca->ca_key->name);
		return (false);
	}
	if (tk->length >= ca->ca_bufsize) {
		logmsg_err("line %u: %s: %s", lineno, ca->ca_key->name,
			   strerror(ENAMETOOLONG));
		return (false);
	}
	(void)memcpy(ca->ca_buffer, tk->token, tk->length);
	ca->ca_buffer[tk->length] = '\0';

	return (true);
}
