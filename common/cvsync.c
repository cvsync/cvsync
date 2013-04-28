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
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_strings.h"

#include "cvsync.h"
#include "logmsg.h"

struct cvsync_token_type {
	const char	*name;
	int		type;
};

static const struct cvsync_token_type cvsync_compress_types[] = {
	{ "none",	CVSYNC_COMPRESS_NO },
	{ "zlib",	CVSYNC_COMPRESS_ZLIB },
	{ NULL,		CVSYNC_COMPRESS_UNSPEC },
};

static const struct cvsync_token_type cvsync_list_types[] = {
	{ "all",	CVSYNC_LIST_ALL },
	{ "rcs",	CVSYNC_LIST_RCS },
	{ NULL,		CVSYNC_LIST_UNKNOWN },
};

static const struct cvsync_token_type cvsync_release_types[] = {
	{ "list",	CVSYNC_RELEASE_LIST },
	{ "rcs",	CVSYNC_RELEASE_RCS },
	{ NULL,		CVSYNC_RELEASE_UNKNOWN },
};

bool __cvsync_isinterrupted(void);
bool __cvsync_isterminated(void);

static struct sigaction cvsync_sig_ign;
static bool cvsync_process_interrupted = false;
static bool cvsync_process_terminated = false;

bool
cvsync_init(void)
{
	struct sigaction act;

	cvsync_sig_ign.sa_handler = cvsync_signal;
	if (sigemptyset(&cvsync_sig_ign.sa_mask) == -1) {
		logmsg_err("sigemptyset: %s", strerror(errno));
		return (false);
	}
	cvsync_sig_ign.sa_flags = 0;

	if (sigaction(SIGHUP, &cvsync_sig_ign, NULL) == -1) {
		logmsg_err("sigaction: %s", strerror(errno));
		return (false);
	}
	if (sigaction(SIGPIPE, &cvsync_sig_ign, NULL) == -1) {
		logmsg_err("sigaction: %s", strerror(errno));
		return (false);
	}

	if (sigaction(SIGINT, NULL, &act) == -1) {
		logmsg_err("sigaction: %s", strerror(errno));
		return (false);
	}
	act.sa_handler = cvsync_signal;
	act.sa_flags &= ~SA_RESTART;
	if (sigaction(SIGINT, &act, NULL) == -1) {
		logmsg_err("sigaction: %s", strerror(errno));
		return (false);
	}

	if (sigaction(SIGTERM, NULL, &act) == -1) {
		logmsg_err("sigaction: %s", strerror(errno));
		return (false);
	}
	act.sa_handler = cvsync_signal;
	act.sa_flags &= ~SA_RESTART;
	if (sigaction(SIGTERM, &act, NULL) == -1) {
		logmsg_err("sigaction: %s", strerror(errno));
		return (false);
	}

	return (true);
}

int
cvsync_compress_pton(const char *name)
{
	const struct cvsync_token_type *t;

	for (t = cvsync_compress_types ; t->name != NULL ; t++) {
		if (strcasecmp(t->name, name) == 0)
			return (t->type);
	}

	return (CVSYNC_COMPRESS_UNSPEC);
}

const char *
cvsync_compress_ntop(int type)
{
	const struct cvsync_token_type *t;

	for (t = cvsync_compress_types ; t->name != NULL ; t++) {
		if (t->type == type)
			return (t->name);
	}

	return ("unspecified");
}

int
cvsync_list_pton(const char *name)
{
	const struct cvsync_token_type *t;

	for (t = cvsync_list_types ; t->name != NULL ; t++) {
		if (strcasecmp(t->name, name) == 0)
			return (t->type);
	}

	return (CVSYNC_LIST_UNKNOWN);
}

const char *
cvsync_list_ntop(int type)
{
	const struct cvsync_token_type *t;

	for (t = cvsync_list_types ; t->name != NULL ; t++) {
		if (t->type == type)
			return (t->name);
	}

	return ("unknown");
}

int
cvsync_release_pton(const char *name)
{
	const struct cvsync_token_type *t;

	for (t = cvsync_release_types ; t->name != NULL ; t++) {
		if (strcasecmp(t->name, name) == 0)
			return (t->type);
	}

	return (CVSYNC_RELEASE_UNKNOWN);
}

const char *
cvsync_release_ntop(int type)
{
	const struct cvsync_token_type *t;

	for (t = cvsync_release_types ; t->name != NULL ; t++) {
		if (t->type == type)
			return (t->name);
	}

	return ("unknown");
}

void *
cvsync_memdup(void *ptr, size_t size)
{
	void *newptr;

	if ((newptr = malloc(size)) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	(void)memcpy(newptr, ptr, size);

	return (newptr);
}

int
cvsync_cmp_pathname(const char *n1, size_t nlen1, const char *n2, size_t nlen2)
{
	const char *bp1 = n1 + nlen1, *bp2 = n2 + nlen2;

	while ((n1 < bp1) && (n2 < bp2)) {
		if (*n1 == *n2) {
			n1++;
			n2++;
			continue;
		}

		if (*n1 == '/')
			return (-1);
		if (*n2 == '/')
			return (1);

		if (*n1 < *n2)
			return (-1);
		else /* *n1 > *n2 */
			return (1);
	}

	return ((int)(nlen1 - nlen2));
}

struct cvsync_file *
cvsync_fopen(const char *path)
{
	struct cvsync_file *cfp;
	struct stat st;

	if ((cfp = malloc(sizeof(*cfp))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	if ((cfp->cf_fileno = open(path, O_RDONLY, 0)) == -1) {
		logmsg_err("%s: %s", path, strerror(errno));
		free(cfp);
		return (NULL);
	}
	if (fstat(cfp->cf_fileno, &st) == -1) {
		logmsg_err("%s: %s", path, strerror(errno));
		(void)close(cfp->cf_fileno);
		free(cfp);
		return (NULL);
	}
	if (!S_ISREG(st.st_mode)) {
		logmsg_err("%s: Not a regular file", path);
		(void)close(cfp->cf_fileno);
		free(cfp);
		return (NULL);
	}

	cfp->cf_size = st.st_size;
	cfp->cf_mtime = st.st_mtime;
	cfp->cf_mode = st.st_mode;

	cfp->cf_addr = NULL;
	cfp->cf_msize = 0;

	return (cfp);
}

bool
cvsync_fclose(struct cvsync_file *cfp)
{
	if (cfp->cf_addr != NULL) {
		if (munmap(cfp->cf_addr, cfp->cf_msize) == -1) {
			logmsg_err("%s", strerror(errno));
			(void)close(cfp->cf_fileno);
			free(cfp);
			return (false);
		}
	}
	if (close(cfp->cf_fileno) == -1) {
		logmsg_err("%s", strerror(errno));
		free(cfp);
		return (false);
	}
	free(cfp);

	return (true);
}

bool
cvsync_mmap(struct cvsync_file *cfp, off_t offset, off_t size)
{
	uint64_t size64 = (uint64_t)size;

	if (size64 > SIZE_MAX) {
		logmsg_err("%" PRIu64 ": %s", size64, strerror(ERANGE));
		return (false);
	}

	if ((cfp->cf_msize = (size_t)size) == 0) {
		cfp->cf_addr = NULL;
		return (true);
	}
	if ((cfp->cf_addr = mmap(NULL, cfp->cf_msize, PROT_READ, MAP_PRIVATE,
				 cfp->cf_fileno, offset)) == MAP_FAILED) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	return (true);
}

bool
cvsync_munmap(struct cvsync_file *cfp)
{
	if (cfp->cf_addr != NULL) {
		if (munmap(cfp->cf_addr, cfp->cf_msize) == -1) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
		cfp->cf_addr = NULL;
		cfp->cf_msize = 0;
	}

	return (true);
}

void
cvsync_signal(int sig)
{
	sigaction(SIGINT, &cvsync_sig_ign, NULL);
	sigaction(SIGTERM, &cvsync_sig_ign, NULL);

	switch (sig) {
	case SIGHUP:
	case SIGPIPE:
		/* Just ignore */
		break;
	case SIGINT:
		cvsync_process_terminated = true;
		/* FALLTHROUGH */
	case SIGTERM:
		cvsync_process_interrupted = true;
		break;
	default:
		break;
	}
}

bool
__cvsync_isinterrupted(void)
{
	return (cvsync_process_interrupted);
}

bool
__cvsync_isterminated(void)
{
	return (cvsync_process_terminated);
}
