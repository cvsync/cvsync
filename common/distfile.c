/*-
 * Copyright (c) 2003-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"

#include "distfile.h"
#include "logmsg.h"
#include "token.h"

static const struct token_keyword distfile_keywords[] = {
	{ "allow",	5,	DISTFILE_ALLOW },
	{ "deny",	4,	DISTFILE_DENY },
	{ "nordiff",	7,	DISTFILE_NORDIFF },
	{ "omitany",	7,	DISTFILE_DENY },
	{ "upgrade",	7,	DISTFILE_ALLOW },
	{ NULL,		0,	DISTFILE_DENY },
};

struct distfile_args *distfile_parse(FILE *);
bool distfile_parse_pattern(FILE *, struct distent *);

struct distfile_args *
distfile_open(const char *fname)
{
	struct distfile_args *da;
	struct stat st;
	FILE *fp;
	int fd;

	if ((fd = open(fname, O_RDONLY, 0)) == -1) {
		logmsg_err("%s: %s", fname, strerror(errno));
		return (NULL);
	}
	if (fstat(fd, &st) == -1) {
		logmsg_err("%s: %s", fname, strerror(errno));
		(void)close(fd);
		return (NULL);
	}
	if (st.st_size > 0) {
		if ((fp = fdopen(fd, "r")) == NULL) {
			logmsg_err("%s: %s", fname, strerror(errno));
			(void)close(fd);
			return (NULL);
		}
		if ((da = distfile_parse(fp)) == NULL) {
			(void)fclose(fp);
			return (NULL);
		}
		if (fclose(fp) == EOF) {
			logmsg_err("%s: %s", fname, strerror(errno));
			distfile_close(da);
			return (NULL);
		}
	} else {
		if (close(fd) == -1) {
			logmsg_err("%s: %s", fname, strerror(errno));
			return (NULL);
		}
		if ((da = malloc(sizeof(*da))) == NULL) {
			logmsg_err("%s", strerror(errno));
			return (NULL);
		}
		(void)memset(da, 0, sizeof(*da));
	}

	return (da);
}

void
distfile_close(struct distfile_args *da)
{
	size_t i;

	if (da == NULL)
		return;

	if (da->da_patterns != NULL) {
		for (i = 0 ; i < da->da_size ; i++)
			free(da->da_patterns[i].de_pattern);
		free(da->da_patterns);
	}

	free(da);
}

struct distfile_args *
distfile_parse(FILE *fp)
{
	struct distent *de;
	struct distfile_args *da;
	size_t max = 0;

	lineno = 1;

	if ((da = malloc(sizeof(*da))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	da->da_patterns = NULL;
	da->da_size = 0;

	for (;;) {
		if (!token_skip_whitespace(fp)) {
			if (feof(fp) == 0) {
				logmsg_err("Distfile Error: premature EOF");
				distfile_close(da);
				return (NULL);
			}
			break;
		}

		if (da->da_size == max) {
			struct distent *newptr;
			size_t old = max, new = old + 4;

			if ((newptr = malloc(new * sizeof(*newptr))) == NULL) {
				logmsg_err("%s", strerror(errno));
				distfile_close(da);
				return (NULL);
			}

			if (da->da_patterns != NULL) {
				(void)memcpy(newptr, da->da_patterns,
					     old * sizeof(*newptr));
				free(da->da_patterns);
			}
			da->da_patterns = newptr;
			max = new;
		}

		de = &da->da_patterns[da->da_size];

		if (!distfile_parse_pattern(fp, de)) {
			distfile_close(da);
			return (NULL);
		}

		da->da_size++;
	}

	if (da->da_size == 0) {
		if (da->da_patterns != NULL)
			free(da->da_patterns);
		da->da_patterns = NULL;
	}

	return (da);
}

bool
distfile_parse_pattern(FILE *fp, struct distent *de)
{
	const struct token_keyword *key;
	struct token tk;

	if ((key = token_get_keyword(fp, distfile_keywords)) == NULL)
		return (false);

	if (!token_get_string(fp, &tk))
		return (false);

	if ((de->de_pattern = malloc(tk.length + 1)) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	(void)memcpy(de->de_pattern, tk.token, tk.length);
	de->de_pattern[tk.length] = '\0';
	de->de_status = key->type;

	return (true);
}

int
distfile_access(struct distfile_args *da, const char *filename)
{
	struct distent *de;
	size_t i;

	if (da->da_size == 0)
		return (DISTFILE_ALLOW);

	for (i = 0 ; i < da->da_size ; i++) {
		de = &da->da_patterns[i];
		if (fnmatch(de->de_pattern, filename, 0) != FNM_NOMATCH)
			return (de->de_status);
	}

	return (DISTFILE_ALLOW);
}
