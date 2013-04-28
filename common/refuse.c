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
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "logmsg.h"
#include "refuse.h"
#include "token.h"

struct refuse_args *refuse_parse(FILE *);
char *refuse_parse_pattern(FILE *);

struct refuse_args *
refuse_open(const char *fname)
{
	struct refuse_args *ra;
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
		if ((ra = refuse_parse(fp)) == NULL) {
			(void)fclose(fp);
			return (NULL);
		}
		if (fclose(fp) == EOF) {
			logmsg_err("%s: %s", fname, strerror(errno));
			refuse_close(ra);
			return (NULL);
		}
	} else {
		if (close(fd) == -1) {
			logmsg_err("%s: %s", fname, strerror(errno));
			return (NULL);
		}
		if ((ra = malloc(sizeof(*ra))) == NULL) {
			logmsg_err("%s", strerror(errno));
			return (NULL);
		}
		(void)memset(ra, 0, sizeof(*ra));
	}

	return (ra);
}

void
refuse_close(struct refuse_args *ra)
{
	size_t i;

	if (ra == NULL)
		return;

	if (ra->ra_patterns != NULL) {
		for (i = 0 ; i < ra->ra_size ; i++)
			free(ra->ra_patterns[i]);
		free(ra->ra_patterns);
	}

	free(ra);
}

struct refuse_args *
refuse_parse(FILE *fp)
{
	struct refuse_args *ra;
	char *pat;
	size_t max = 0;

	lineno = 1;

	if ((ra = malloc(sizeof(*ra))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	ra->ra_patterns = NULL;
	ra->ra_size = 0;

	for (;;) {
		if (!token_skip_whitespace(fp)) {
			if (feof(fp) == 0) {
				logmsg_err("Distfile Error: premature EOF");
				refuse_close(ra);
				return (NULL);
			}
			break;
		}

		if (ra->ra_size == max) {
			char **newptr;
			size_t old = max, new = old + 4;

			if ((newptr = malloc(new * sizeof(*newptr))) == NULL) {
				logmsg_err("%s", strerror(errno));
				refuse_close(ra);
				return (NULL);
			}

			if (ra->ra_patterns != NULL) {
				(void)memcpy(newptr, ra->ra_patterns,
					     old * sizeof(*newptr));
				free(ra->ra_patterns);
			}
			ra->ra_patterns = newptr;
			max = new;
		}

		if ((pat = refuse_parse_pattern(fp)) == NULL) {
			refuse_close(ra);
			return (NULL);
		}

		ra->ra_patterns[ra->ra_size++] = pat;
	}

	if (ra->ra_size == 0) {
		if (ra->ra_patterns != NULL)
			free(ra->ra_patterns);
		ra->ra_patterns = NULL;
	}

	return (ra);
}

char *
refuse_parse_pattern(FILE *fp)
{
	char *tok;
	size_t max = 32, n = 0;
	int c;

	if ((tok = malloc(max + 1)) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n')
			break;
		if (n == max) {
			char *newptr;
			size_t old = max, new = old + 32;

			if ((newptr = malloc(new + 1)) == NULL) {
				logmsg_err("%s", strerror(errno));
				free(tok);
				return (NULL);
			}

			(void)memcpy(newptr, tok, old);
			free(tok);

			tok = newptr;
			max = new;
		}

		tok[n++] = (char)c;
	}
	if (c != '\n') {
		free(tok);
		return (NULL);
	}
	tok[n] = '\0';

	return (tok);
}

bool
refuse_access(struct refuse_args *ra, struct cvsync_attr *ca)
{
	char *pat, *ep;
	size_t namelen, i;
	int rv;

	if ((ra == NULL) || (ra->ra_size == 0))
		return (true);

	if (ca->ca_namelen >= sizeof(ca->ca_name))
		return (true);
	ca->ca_name[ca->ca_namelen] = '\0';

	for (i = 0 ; i < ra->ra_size ; i++) {
		pat = ra->ra_patterns[i];
		if (fnmatch(pat, (char *)ca->ca_name, 0) != FNM_NOMATCH)
			return (false);
		if (ca->ca_type != FILETYPE_DIR)
			continue;
		namelen = strlen(pat);
		ep = pat + namelen;
		while (ep >= pat) {
			if (*--ep != '/')
				continue;

			*ep = '\0';
			rv = fnmatch(pat, (char *)ca->ca_name, 0);
			*ep = '/';

			if (rv != FNM_NOMATCH)
				return (false);

			namelen = ep - pat;
		}
	}

	return (true);
}
