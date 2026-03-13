/*-
 * This software is released under the BSD License, see LICENSE.
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
