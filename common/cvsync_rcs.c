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

#include <limits.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "cvsync.h"
#include "filetypes.h"

static const char *_attic = "Attic/";
static const size_t _atticlen = 6;

bool
cvsync_rcs_append_attic(char *path, size_t pathlen, size_t pathmax)
{
	size_t len;

	if ((len = pathlen + _atticlen + 1) >= pathmax)
		return (false);

	path[pathlen] = '/';
	(void)memcpy(&path[pathlen + 1], _attic, _atticlen);
	path[len] = '\0';

	return (true);
}

bool
cvsync_rcs_insert_attic(char *path, size_t pathlen, size_t pathmax)
{
	char *rpath;
	size_t len;

	if ((len = pathlen + _atticlen) >= pathmax)
		return (false);

	rpath = &path[pathlen - 1];
	if (*rpath == '/')
		return (false);

	while (rpath > path) {
		if (*rpath == '/') {
			rpath++;
			break;
		}
		rpath--;
	}
	len = pathlen - (size_t)(rpath - path);
	(void)memmove(&rpath[_atticlen], rpath, len + 1); /* incl. NULL */
	(void)memcpy(rpath, _attic, _atticlen);

	return (true);
}

bool
cvsync_rcs_remove_attic(char *path, size_t pathlen)
{
	char *rpath, *sp = NULL;
	size_t len;

	rpath = &path[pathlen - 1];
	if (*rpath == '/')
		return (false);

	while (rpath > path) {
		if (*rpath == '/') {
			sp = rpath + 1;
			break;
		}
		rpath--;
	}
	if (sp == NULL)
		return (false);
	len = pathlen - (size_t)(sp - path);
	for (rpath-- ; rpath > path ; rpath--) {
		if (*rpath == '/') {
			rpath++;
			break;
		}
	}
	if (!IS_DIR_ATTIC(rpath, sp - rpath - 1))
		return (false);

	(void)memmove(rpath, sp, len + 1); /* incl. NULL */

	return (true);
}

bool
cvsync_rcs_filename(const char *name, size_t namelen)
{
	size_t i;

	for (i = 0 ; i < namelen ; i++) {
		if (name[i] == '/')
			return (false);
	}
	if (IS_DIR_CURRENT(name, namelen) || IS_DIR_PARENT(name, namelen) ||
	    IS_FILE_TMPFILE(name, namelen)) {
		return (false);
	}

	return (true);
}

bool
cvsync_rcs_pathname(const char *path, size_t pathlen)
{
	size_t len;

	if ((pathlen == 0) || (path[0] == '/') || (path[pathlen - 1] == '/'))
		return (false);

	for (;;) {
		for (len = 0 ; len < pathlen ; len++) {
			if (path[len] == '/')
				break;
		}
		if (len == 0)
			return (false);

		if (IS_DIR_CURRENT(path, len) || IS_DIR_PARENT(path, len) ||
		    IS_FILE_TMPFILE(path, len)) {
			return (false);
		}

		if (len == pathlen)
			break;

		len++;
		path += len;
		pathlen -= len;
	}

	return (true);
}
