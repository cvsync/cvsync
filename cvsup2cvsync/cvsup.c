/*-
 * Copyright (c) 2003-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "attribute.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "logmsg.h"

#include "defs.h"

#define	CVSUP_ATTR_FILETYPE	(0x01)
#define	CVSUP_ATTR_MTIME	(0x02)
#define	CVSUP_ATTR_SIZE		(0x04)
#define	CVSUP_ATTR_LINKTARGET	(0x08)
#define	CVSUP_ATTR_MODE		(0x80)

#define	CVSUP_ATTRS_DIR		(CVSUP_ATTR_MODE)
#define	CVSUP_ATTRS_FILE	(CVSUP_ATTR_MTIME|CVSUP_ATTR_SIZE|CVSUP_ATTR_MODE)
#define	CVSUP_ATTRS_RCS		(CVSUP_ATTR_MTIME|CVSUP_ATTR_MODE)
#define	CVSUP_ATTRS_SYMLINK	(CVSUP_ATTR_LINKTARGET)

bool cvsup_search_dir(struct cvsync_attr *, uint8_t *, uint8_t *);
bool cvsup_decode_dirattr(struct cvsync_attr *, uint8_t *, uint8_t *);

bool cvsup_decode_rcs(struct cvsync_attr *, uint8_t *, uint8_t *);
bool cvsup_decode_symlink(struct cvsync_attr *, uint8_t *, uint8_t *);

uint32_t cvsup_decode_attrs(uint8_t *, uint8_t *, uint8_t **);
uint8_t cvsup_decode_filetype(uint8_t *, uint8_t *, uint8_t **);
int64_t cvsup_decode_mtime(uint8_t *, uint8_t *, uint8_t **);
uint64_t cvsup_decode_size(uint8_t *, uint8_t *, uint8_t **);
size_t cvsup_decode_linktarget(uint8_t *, uint8_t *, uint8_t **, void *, size_t);
uint16_t cvsup_decode_mode(uint8_t *, uint8_t *, uint8_t **);
size_t cvsup_decode_length(uint8_t *, uint8_t *);
uint8_t *cvsup_ignore_bit(uint8_t *, uint8_t *);

uint8_t *
cvsup_examine(uint8_t *sp, uint8_t *bp)
{
	uint8_t *ep;

	if (*sp++ != 'F') {
		logmsg_err("This file seems not be CVSup scan file");
		return (NULL);
	}
	if ((ep = memchr(sp, '\n', (size_t)(bp - sp))) == NULL) {
		logmsg_err("premature EOF");
		return (NULL);
	}
	if (ep - sp < 4) {
		logmsg_err("premature EOF");
		return (NULL);
	}
	if (*sp++ != ' ') {
		logmsg_err("no separators");
		return (NULL);
	}
	if (*sp != '5') {
		logmsg_err("%c: invalid version (!= 5)", *sp);
		return (NULL);
	}
	if (*++sp != ' ') {
		logmsg_err("no separators");
		return (NULL);
	}

	return (ep + 1);
}

bool
cvsup_decode_dir(struct cvsync_attr *attr, uint8_t *sp, uint8_t *bp)
{
	if (!cvsup_search_dir(attr, sp, bp))
		return (false);

	attr->ca_tag = FILETYPE_DIR;
	attr->ca_auxlen = attr_rcs_encode_dir(attr->ca_aux,
					      sizeof(attr->ca_aux),
					      attr->ca_mode);
	if (attr->ca_auxlen == 0) {
		logmsg_err("%s", strerror(ENOBUFS));
		return (false);
	}

	logmsg_verbose("%.*s %o", attr->ca_namelen, attr->ca_name,
		       attr->ca_mode);

	return (true);
}

bool
cvsup_search_dir(struct cvsync_attr *attr, uint8_t *sp, uint8_t *bp)
{
	uint8_t *ep, tag;
	int n = 1;

	while (sp < bp) {
		if (bp - sp < 3) {
			logmsg_err("premature EOF");
			return (false);
		}
		tag = *sp++;
		if (*sp++ != ' ') {
			logmsg_err("no separators");
			return (false);
		}

		if ((ep = memchr(sp, '\n', (size_t)(bp - sp))) == NULL) {
			logmsg_err("premature EOF");
			return (false);
		}

		switch (tag) {
		case 'D':
			/* skip in this context. */
			n++;
			break;
		case 'U':
			if (--n < 0) {
				logmsg_err("Found extra 'U's");
				return (false);
			}
			if (n > 0) {
				/* skip in this context. */
				break;
			}
			if ((size_t)(ep - sp) <= attr->ca_namelen) {
				logmsg_err("Not found 'U' for %.*s",
					   attr->ca_namelen, attr->ca_name);
				return (false);
			}
			if (memcmp(sp, attr->ca_name,
				   attr->ca_namelen) != 0) {
				logmsg_err("Not found 'U' for %.*s",
					   attr->ca_namelen, attr->ca_name);
				return (false);
			}
			if (sp[attr->ca_namelen] != ' ') {
				logmsg_err("no separators");
				return (false);
			}
			sp += attr->ca_namelen + 1;

			if (!cvsup_decode_dirattr(attr, sp, ep))
				return (false);

			return (true);
		case 'V':
		case 'v':
			/* Nothing to do. */
			break;
		default:
			logmsg_err("%c: unknown entry tag", tag);
			return (false);
		}

		sp = ep + 1;
	}

	logmsg_err("Not found 'U' for %.*s", attr->ca_namelen,
		   attr->ca_name);

	return (false);
}

bool
cvsup_decode_dirattr(struct cvsync_attr *attr, uint8_t *sp, uint8_t *ep)
{
	uint32_t attrs;
	uint8_t type;
	int i;

	if ((attrs = cvsup_decode_attrs(sp, ep, &sp)) == (uint32_t)-1)
		return (false);
	if ((attrs & CVSUP_ATTR_FILETYPE) != CVSUP_ATTR_FILETYPE) {
		logmsg_err("Not found FileType field");
		return (false);
	}
	if ((type = cvsup_decode_filetype(sp, ep, &sp)) == (uint8_t)-1)
		return (false);
	if (type != FILETYPE_DIR) {
		logmsg_err("%c: invalid FileType", type);
		return (false);
	}
	if ((attrs & CVSUP_ATTRS_DIR) != CVSUP_ATTRS_DIR) {
		logmsg_err("Not enough fields");
		return (false);
	}
	for (i = 0 ; i < 6 ; i++) {
		if (attrs & 1) {
			if ((sp = cvsup_ignore_bit(sp, ep)) == NULL)
				return (false);
		}
		attrs >>= 1;
	}
	if ((attr->ca_mode = cvsup_decode_mode(sp, ep, &sp)) == (uint16_t)-1)
		return (false);

	return (true);
}

bool
cvsup_decode_file(struct cvsync_attr *attr, uint8_t *sp, uint8_t *ep)
{
	uint32_t attrs;
	uint8_t *sv_sp = sp, type;
	int i;

	if ((attrs = cvsup_decode_attrs(sp, ep, &sp)) == (uint32_t)-1)
		return (false);
	if ((attrs & CVSUP_ATTR_FILETYPE) != CVSUP_ATTR_FILETYPE) {
		logmsg_err("Not found FileType field");
		return (false);
	}
	if ((type = cvsup_decode_filetype(sp, ep, &sp)) == (uint8_t)-1)
		return (false);
	switch (type) {
	case FILETYPE_FILE:
		if (IS_FILE_RCS(attr->ca_name, attr->ca_namelen))
			return (cvsup_decode_rcs(attr, sv_sp, ep));
		break;
	case FILETYPE_SYMLINK:
		return (cvsup_decode_symlink(attr, sv_sp, ep));
	default:
		logmsg_err("%c: invalid FileType", type);
		return (false);
	}
	if ((attrs & CVSUP_ATTRS_FILE) != CVSUP_ATTRS_FILE) {
		logmsg_err("Not enough fields");
		return (false);
	}
	if ((attr->ca_mtime = cvsup_decode_mtime(sp, ep, &sp)) == (int64_t)-1)
		return (false);
	if ((attr->ca_size = cvsup_decode_size(sp, ep, &sp)) == (uint64_t)-1)
		return (false);
	attrs >>= 4;
	for (i = 0 ; i < 3 ; i++) {
		if (attrs & 1) {
			if ((sp = cvsup_ignore_bit(sp, ep)) == NULL)
				return (false);
		}
		attrs >>= 1;
	}
	if ((attr->ca_mode = cvsup_decode_mode(sp, ep, &sp)) == (uint16_t)-1)
		return (false);

	attr->ca_tag = FILETYPE_FILE;
	attr->ca_auxlen = attr_rcs_encode_file(attr->ca_aux,
					       sizeof(attr->ca_aux),
					       (time_t)attr->ca_mtime,
					       (off_t)attr->ca_size,
					       attr->ca_mode);
	if (attr->ca_auxlen == 0) {
		logmsg_err("%s", strerror(ENOBUFS));
		return (false);
	}

	logmsg_verbose("%.*s %" PRId64 " %" PRIu64 " %o", attr->ca_namelen,
		       attr->ca_name, attr->ca_mtime, attr->ca_size,
		       attr->ca_mode);

	return (true);
}

bool
cvsup_decode_rcs(struct cvsync_attr *attr, uint8_t *sp, uint8_t *ep)
{
	uint32_t attrs;
	uint8_t type;
	int i;

	if ((attrs = cvsup_decode_attrs(sp, ep, &sp)) == (uint32_t)-1)
		return (false);
	if ((attrs & CVSUP_ATTR_FILETYPE) != CVSUP_ATTR_FILETYPE) {
		logmsg_err("Not found FileType field");
		return (false);
	}
	if ((type = cvsup_decode_filetype(sp, ep, &sp)) == (uint8_t)-1)
		return (false);
	if (type != FILETYPE_FILE) {
		logmsg_err("%c: invalid FileType", type);
		return (false);
	}
	if ((attrs & CVSUP_ATTRS_RCS) != CVSUP_ATTRS_RCS) {
		logmsg_err("Not enough fields");
		return (false);
	}
	if ((attr->ca_mtime = cvsup_decode_mtime(sp, ep, &sp)) == (int64_t)-1)
		return (false);
	attrs >>= 2;
	for (i = 0 ; i < 5 ; i++) {
		if (attrs & 1) {
			if ((sp = cvsup_ignore_bit(sp, ep)) == NULL)
				return (false);
		}
		attrs >>= 1;
	}
	if ((attr->ca_mode = cvsup_decode_mode(sp, ep, &sp)) == (uint16_t)-1)
		return (false);

	attr->ca_tag = FILETYPE_RCS;
	attr->ca_auxlen = attr_rcs_encode_rcs(attr->ca_aux,
					      sizeof(attr->ca_aux),
					      (time_t)attr->ca_mtime,
					      attr->ca_mode);
	if (attr->ca_auxlen == 0) {
		logmsg_err("%s", strerror(ENOBUFS));
		return (false);
	}

	logmsg_verbose("%.*s %" PRId64 " %o", attr->ca_namelen, attr->ca_name,
		       attr->ca_mtime, attr->ca_mode);

	return (true);
}

bool
cvsup_decode_symlink(struct cvsync_attr *attr, uint8_t *sp, uint8_t *ep)
{
	uint32_t attrs;
	uint8_t type;

	if ((attrs = cvsup_decode_attrs(sp, ep, &sp)) == (uint32_t)-1)
		return (false);
	if ((attrs & CVSUP_ATTR_FILETYPE) != CVSUP_ATTR_FILETYPE) {
		logmsg_err("Not found FileType field");
		return (false);
	}
	if ((type = cvsup_decode_filetype(sp, ep, &sp)) == (uint8_t)-1)
		return (false);
	if (type != FILETYPE_SYMLINK) {
		logmsg_err("%c: invalid FileType", type);
		return (false);
	}
	if ((attrs & CVSUP_ATTRS_SYMLINK) != CVSUP_ATTRS_SYMLINK) {
		logmsg_err("Not enough fields");
		return (false);
	}
	attrs >>= 1;
	attr->ca_auxlen = cvsup_decode_linktarget(sp, ep, &sp, attr->ca_aux,
						  sizeof(attr->ca_aux));
	if (attr->ca_auxlen == 0)
		return (false);

	attr->ca_tag = FILETYPE_SYMLINK;

	logmsg_verbose("%.*s -> %.*s", attr->ca_namelen, attr->ca_name,
		       attr->ca_auxlen, attr->ca_aux);

	return (true);
}

uint32_t
cvsup_decode_attrs(uint8_t *sp, uint8_t *ep, uint8_t **new_sp)
{
	uint32_t attrs = 0;
	uint8_t *sep;
	size_t n;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return ((uint32_t)-1);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return ((uint32_t)-1);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return ((uint32_t)-1);
	}
	if ((n == 0) || (n > sizeof(attrs) * 2)) {
		logmsg_err("%d: invalid Attrs length", n);
		return ((uint32_t)-1);
	}

	*new_sp = sp + n;

	while (n-- > 0) {
		if (!isxdigit((int)(*sp))) {
			logmsg_err("%c: invalid Attrs specifier", *sp);
			return ((uint32_t)-1);
		}
		attrs <<= 4;
		if (isdigit((int)(*sp)))
			attrs += *sp - '0';
		else
			attrs += (uint32_t)(tolower(*sp) - 'a' + 10);
		sp++;
	}

	return (attrs);
}

uint8_t
cvsup_decode_filetype(uint8_t *sp, uint8_t *ep, uint8_t **new_sp)
{
	uint8_t tag, *sep;
	size_t n, i;
	int type = 0, c;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return ((uint8_t)-1);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return ((uint8_t)-1);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return ((uint8_t)-1);
	}

	for (i = 0 ; i < n ; i++) {
		if (!isdigit((int)(*sp))) {
			logmsg_err("%c: invalid FileType specifier", *sp);
			return ((uint8_t)-1);
		}
		c = (int)(*sp++ - '0');
		if (INT_MAX / 10 < type) {
			logmsg_err("%.*s: %s", n, sep + 1, strerror(ERANGE));
			return ((uint8_t)-1);
		}
		type *= 10;
		if (INT_MAX - type < c) {
			logmsg_err("%.*s: %s", n, sep + 1, strerror(ERANGE));
			return ((uint8_t)-1);
		}
		type += c;
	}

	*new_sp = sp;

	switch (type) {
	case 0:
		logmsg_err("%d: unsupported FileType", type);
		return ((uint8_t)-1);
	case 1:
		tag = FILETYPE_FILE;
		break;
	case 2:
		tag = FILETYPE_DIR;
		break;
	case 3:
		tag = FILETYPE_CHRDEV;
		break;
	case 4:
		tag = FILETYPE_BLKDEV;
		break;
	case 5:
		tag = FILETYPE_SYMLINK;
		break;
	default:
		logmsg_err("%d: unsupported FileType", type);
		return ((uint8_t)-1);
	}

	return (tag);
}

int64_t
cvsup_decode_mtime(uint8_t *sp, uint8_t *ep, uint8_t **new_sp)
{
	int64_t mtime = 0, c;
	uint8_t *sep;
	size_t n, i;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return (-1);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return (-1);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return (-1);
	}

	for (i = 0 ; i < n ; i++) {
		if (!isdigit((int)(*sp))) {
			logmsg_err("%c: invalid ModTime specifier", *sp);
			return (-1);
		}
		c = (int64_t)(*sp++ - '0');
		if (INT64_MAX / 10 < mtime) {
			logmsg_err("%.*s: %s", n, sep + 1, strerror(ERANGE));
			return (-1);
		}
		mtime *= 10;
		if (INT64_MAX - mtime < c) {
			logmsg_err("%.*s: %s", n, sep + 1, strerror(ERANGE));
			return (-1);
		}
		mtime += c;
	}

	*new_sp = sp;

	return (mtime);
}

uint64_t
cvsup_decode_size(uint8_t *sp, uint8_t *ep, uint8_t **new_sp)
{
	uint64_t size = 0, c;
	uint8_t *sep;
	size_t n, i;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return ((uint64_t)-1);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return ((uint64_t)-1);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return ((uint64_t)-1);
	}

	for (i = 0 ; i < n ; i++) {
		if (!isdigit((int)(*sp))) {
			logmsg_err("%c: invalid ModTime specifier", *sp);
			return ((uint64_t)-1);
		}
		c = (uint64_t)(*sp++ - '0');
		if (UINT64_MAX / 10 < size) {
			logmsg_err("%.*s: %s", n, sep + 1, strerror(ERANGE));
			return ((uint64_t)-1);
		}
		size *= 10;
		if (UINT64_MAX - size < c) {
			logmsg_err("%.*s: %s", n, sep + 1, strerror(ERANGE));
			return ((uint64_t)-1);
		}
		size += c;
	}

	*new_sp = sp;

	return (size);
}

size_t
cvsup_decode_linktarget(uint8_t *sp, uint8_t *ep, uint8_t **new_sp,
			void *buffer, size_t bufsize)
{
	uint8_t *linksp = buffer, *linkbp = linksp + bufsize, *sep;
	size_t n;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return (0);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return (0);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return (0);
	}
	if ((n == 0) || (n > bufsize)) {
		logmsg_err("%d: invalid LinkTarget length", n);
		return (0);
	}

	while (n-- > 0) {
		if (linksp == linkbp)
			return (0);
		if (*sp != '\\') {
			*linksp++ = *sp++;
			continue;
		}
		sp++;
		n--;
		if (ep - sp < 1) {
			logmsg_err("premature EOF");
			return (0);
		}
		switch (*sp) {
		case '_':
			*linksp++ = ' ';
			break;
		default:
			*linksp++ = '\\';
			*linksp++ = *sp++;
		}
	}

	*new_sp = sp;

	return ((size_t)(linksp - (uint8_t *)buffer));
}

uint16_t
cvsup_decode_mode(uint8_t *sp, uint8_t *ep, uint8_t **new_sp)
{
	uint16_t mode = 0;
	uint8_t *sep;
	size_t n;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return ((uint16_t)-1);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return ((uint16_t)-1);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return ((uint16_t)-1);
	}
	if ((n == 0) || (n > sizeof(mode) * 2)) {
		logmsg_err("%d: invalid Mode length", n);
		return ((uint16_t)-1);
	}

	while (n-- > 0) {
		if ((*sp < '0') || (*sp > '7')) {
			logmsg_err("%c: invalid Mode specifier", *sp);
			return ((uint16_t)-1);
		}
		mode = (uint16_t)((mode << 3) + *sp++ - '0');
	}

	*new_sp = sp;

	return (mode & mode_umask);
}

size_t
cvsup_decode_length(uint8_t *sp, uint8_t *ep)
{
	uint8_t *sv_sp = sp;
	size_t n = 0, c;

	while (sp < ep) {
		if (!isdigit((int)(*sp))) {
			logmsg_err("%c: invalid length specifier", *sp);
			return ((size_t)-1);
		}
		c = (size_t)(*sp++ - '0');
		if (SIZE_MAX / 10 < n) {
			logmsg_err("%.*s: %s", ep - sv_sp, sv_sp,
				   strerror(ERANGE));
			return ((size_t)-1);
		}
		n *= 10;
		if (SIZE_MAX - n < c) {
			logmsg_err("%.*s: %s", ep - sv_sp, sv_sp,
				   strerror(ERANGE));
			return ((size_t)-1);
		}
		n += c;
	}

	return (n);
}

uint8_t *
cvsup_ignore_bit(uint8_t *sp, uint8_t *ep)
{
	uint8_t *sep;
	size_t n;

	if ((sep = memchr(sp, '#', (size_t)(ep - sp))) == NULL) {
		logmsg_err("no separators");
		return (NULL);
	}
	if ((n = cvsup_decode_length(sp, sep)) == (size_t)-1)
		return (NULL);
	if (((sp = sep + 1) >= ep) || ((size_t)(ep - sp) < n)) {
		logmsg_err("premature EOF");
		return (NULL);
	}

	return (sep + n + 1);
}
