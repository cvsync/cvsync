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

#include <stdio.h>

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "attribute.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "logmsg.h"
#include "scanfile.h"

#include "defs.h"

#define	CVSUP_MAXATTRSIZE	(256)

size_t cvsup_escape_name(struct scanfile_attr *, void *, size_t);

static char cvsup_userinfo[CVSUP_MAXATTRSIZE]; /* user/group name cache */
static int cvsup_userinfo_length;

bool
cvsup_init(const char *user, const char *group)
{
	struct group *gr;
	struct passwd *pw;
	size_t ulen, glen;
	int wn;

	if (user == NULL) {
		if ((pw = getpwuid(getuid())) == NULL) {
			logmsg_err("Who am I?");
			return (false);
		}
		user = pw->pw_name;
	}
	ulen = strlen(user);

	if (group == NULL) {
		if ((gr = getgrgid(getgid())) == NULL) {
			logmsg_err("Who am I?");
			return (false);
		}
		group = gr->gr_name;
	}
	glen = strlen(group);

	wn = snprintf(cvsup_userinfo, sizeof(cvsup_userinfo),
		      "%lu#%.*s%lu#%.*s", (unsigned long)ulen, (int)ulen,
		      user, (unsigned long)glen, (int)glen, group);
	if ((wn <= 0) || ((size_t)wn >= sizeof(cvsup_userinfo))) {
		logmsg_err("Too long user/group name");
		return (false);
	}

	cvsup_userinfo_length = wn;

	return (true);
}

bool
cvsup_write_header(int fd, time_t mtime)
{
	char head[CVSUP_MAXATTRSIZE];
	size_t len;
	ssize_t wrn;
	int wn;

	wn = snprintf(head, sizeof(head), "F 5 %ld\n", (long)mtime);
	if ((wn <= 0) || ((size_t)wn >= sizeof(head))) {
		logmsg_err("Header: %s", strerror(EINVAL));
		return (false);
	}

	len = (size_t)wn;
	if ((wrn = write(fd, head, len)) == -1) {
		logmsg_err("Header: %s", strerror(errno));
		return (false);
	}
	if ((size_t)wrn != len) {
		logmsg_err("Header: write error");
		return (false);
	}

	return (true);
}

bool
cvsup_write_dirdown(int fd, struct scanfile_attr *attr)
{
	char attrstr[CVSUP_MAXATTRSIZE], name[CVSUP_MAXATTRSIZE];
	size_t namelen, len;
	ssize_t wrn;
	int wn;

	if ((namelen = cvsup_escape_name(attr, name,
					 sizeof(name))) == (size_t)-1) {
		return (false);
	}

	wn = snprintf(attrstr, sizeof(attrstr), "D %.*s\n",
		      (int)namelen, name);
	if ((wn <= 0) || ((size_t)wn >= sizeof(attrstr))) {
		logmsg_err("Too long DirDown attribute");
		return (false);
	}

	len = (size_t)wn;
	if ((wrn = write(fd, attrstr, len)) == -1) {
		logmsg_err("DirDown: %s", strerror(errno));
		return (false);
	}
	if ((size_t)wrn != len) {
		logmsg_err("DirDown: write error");
		return (false);
	}

	return (true);
}

bool
cvsup_write_dirup(int fd, struct scanfile_attr *attr)
{
	struct cvsync_attr cattr;
	char attrstr[CVSUP_MAXATTRSIZE], name[CVSUP_MAXATTRSIZE];
	size_t namelen, len;
	ssize_t wrn;
	int mlen, wn;

	if (!attr_rcs_decode_dir(attr->a_aux, attr->a_auxlen, &cattr))
		return (false);

	if ((namelen = cvsup_escape_name(attr, name,
					 sizeof(name))) == (size_t)-1) {
		return (false);
	}

	mlen = snprintf(attrstr, sizeof(attrstr), "%o", cattr.ca_mode);

	wn = snprintf(attrstr, sizeof(attrstr),
		      "U %.*s 3#1e11#2%.*s%d#%o1#0\n",
		      (int)namelen, name, cvsup_userinfo_length,
		      cvsup_userinfo, mlen, cattr.ca_mode);
	if ((wn <= 0) || ((size_t)wn >= sizeof(attrstr))) {
		logmsg_err("Too long DirDown attribute");
		return (false);
	}

	len = (size_t)wn;
	if ((wrn = write(fd, attrstr, len)) == -1) {
		logmsg_err("DirDown: %s", strerror(errno));
		return (false);
	}
	if ((size_t)wrn != len) {
		logmsg_err("DirDown: write error");
		return (false);
	}

	return (true);
}

bool
cvsup_write_file(int fd, struct scanfile_attr *attr)
{
	struct cvsync_attr cattr;
	char attrstr[CVSUP_MAXATTRSIZE], name[CVSUP_MAXATTRSIZE];
	size_t namelen, len;
	ssize_t wrn;
	int tlen, slen, mlen, wn;

	if (!attr_rcs_decode_file(attr->a_aux, attr->a_auxlen, &cattr))
		return (false);

	if ((namelen = cvsup_escape_name(attr, name,
					 sizeof(name))) == (size_t)-1) {
		return (false);
	}

	tlen = snprintf(attrstr, sizeof(attrstr), "%lld",
			(long long)cattr.ca_mtime);
	slen = snprintf(attrstr, sizeof(attrstr), "%llu",
			(unsigned long long)cattr.ca_size);
	mlen = snprintf(attrstr, sizeof(attrstr), "%o", cattr.ca_mode);

	wn = snprintf(attrstr, sizeof(attrstr),
		      "V %.*s 3#1e71#1%d#%lld%d#%llu%.*s%d#%o1#0\n",
		      (int)namelen, name, tlen, (long long)cattr.ca_mtime,
		      slen, (unsigned long long)cattr.ca_size,
		      cvsup_userinfo_length, cvsup_userinfo,
		      mlen, cattr.ca_mode);
	if ((wn <= 0) || ((size_t)wn >= sizeof(attrstr))) {
		logmsg_err("Too long File attribute");
		return (false);
	}

	len = (size_t)wn;
	if ((wrn = write(fd, attrstr, len)) == -1) {
		logmsg_err("File: %s", strerror(errno));
		return (false);
	}
	if ((size_t)wrn != len) {
		logmsg_err("File: write error");
		return (false);
	}

	return (true);
}

bool
cvsup_write_rcs(int fd, struct scanfile_attr *attr)
{
	struct cvsync_attr cattr;
	char attrstr[CVSUP_MAXATTRSIZE], name[CVSUP_MAXATTRSIZE], c;
	size_t namelen, len;
	ssize_t wrn;
	int tlen, mlen, wn;

	if (!attr_rcs_decode_rcs(attr->a_aux, attr->a_auxlen, &cattr))
		return (false);

	if (attr->a_type == FILETYPE_RCS)
		c = 'V';
	else /* FILETYPE_RCS_ATTIC */
		c = 'v';

	if ((namelen = cvsup_escape_name(attr, name,
					 sizeof(name))) == (size_t)-1) {
		return (false);
	}

	tlen = snprintf(attrstr, sizeof(attrstr), "%lld",
			(long long)cattr.ca_mtime);
	mlen = snprintf(attrstr, sizeof(attrstr), "%o", cattr.ca_mode);

	wn = snprintf(attrstr, sizeof(attrstr),
		      "%c %.*s 3#1e31#1%d#%lld%.*s%d#%o1#0\n",
		      c, (int)namelen, name, tlen, (long long)cattr.ca_mtime,
		      cvsup_userinfo_length, cvsup_userinfo,
		      mlen, cattr.ca_mode);
	if ((wn <= 0) || ((size_t)wn >= sizeof(attrstr))) {
		logmsg_err("Too long RCS attribute");
		return (false);
	}

	len = (size_t)wn;
	if ((wrn = write(fd, attrstr, len)) == -1) {
		logmsg_err("RCS: %s", strerror(errno));
		return (false);
	}
	if ((size_t)wrn != len) {
		logmsg_err("RCS: write error");
		return (false);
	}

	return (true);
}

bool
cvsup_write_symlink(int fd, struct scanfile_attr *attr)
{
	char attrstr[CVSUP_MAXATTRSIZE], name[CVSUP_MAXATTRSIZE];
	size_t namelen, len;
	ssize_t wrn;
	int wn;

	if ((namelen = cvsup_escape_name(attr, name,
					 sizeof(name))) == (size_t)-1) {
		return (false);
	}

	wn = snprintf(attrstr, sizeof(attrstr), "V %.*s 1#91#5%lu#%.*s\n",
		      (int)namelen, name, (unsigned long)attr->a_auxlen,
		      (int)attr->a_auxlen, (char *)attr->a_aux);
	if ((wn <= 0) || ((size_t)wn >= sizeof(attrstr))) {
		logmsg_err("Too long Symlink attribute");
		return (false);
	}

	len = (size_t)wn;
	if ((wrn = write(fd, attrstr, len)) == -1) {
		logmsg_err("Symlink: %s", strerror(errno));
		return (false);
	}
	if ((size_t)wrn != len) {
		logmsg_err("Symlink: write error");
		return (false);
	}

	return (true);
}

size_t
cvsup_escape_name(struct scanfile_attr *attr, void *buffer, size_t length)
{
	char *sp = attr->a_name, *bp = sp + attr->a_namelen, *p = buffer;
	size_t namelen = 0;

	while (sp < bp) {
		switch (*sp) {
		case '\\':
			if (namelen + 2 > length) {
				logmsg_err("No space to escape '\\'");
				return ((size_t)-1);
			}
			*p++ = '\\';
			*p++ = '\\';
			namelen += 2;
			break;
		case ' ':
			if (namelen + 2 > length) {
				logmsg_err("No space to escape ' '");
				return ((size_t)-1);
			}
			*p++ = '\\';
			*p++ = '_';
			namelen += 2;
			break;
		default:
			if (namelen >= length) {
				logmsg_err("No space to put any characters");
				return ((size_t)-1);
			}
			*p++ = *sp;
			namelen++;
			break;
		}
		sp++;
	}

	return (namelen);
}
