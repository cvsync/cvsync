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
#include <sys/stat.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "compat_sys_stat.h"
#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "cvsync.h"
#include "filetypes.h"
#include "list.h"
#include "logmsg.h"
#include "scanfile.h"

bool scanfile_insert_attr(struct scanfile_args *, struct scanfile_attr *);
bool scanfile_remove_attr(struct scanfile_args *, struct scanfile_attr *);
bool scanfile_flush_attr(struct scanfile_args *, struct scanfile_attr *);

void
scanfile_init(struct scanfile_args *sa)
{
	sa->sa_scanfile = NULL;
	sa->sa_scanfile_name = NULL;
	sa->sa_start = NULL;
	sa->sa_end = NULL;
	sa->sa_tmp = -1;
	sa->sa_tmp_name[0] = '\0';
	sa->sa_tmp_mode = 0;
	sa->sa_dirlist = NULL;
	(void)memset(&sa->sa_attr, 0, sizeof(sa->sa_attr));
	sa->sa_changed = false;
}

struct scanfile_args *
scanfile_open(const char *fname)
{
	struct scanfile_args *sa;

	if ((sa = malloc(sizeof(*sa))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	scanfile_init(sa);

	sa->sa_scanfile_name = fname;
	if ((sa->sa_scanfile = cvsync_fopen(sa->sa_scanfile_name)) == NULL) {
		free(sa);
		return (NULL);
	}
	if (!cvsync_mmap(sa->sa_scanfile, (off_t)0,
			 sa->sa_scanfile->cf_size)) {
		cvsync_fclose(sa->sa_scanfile);
		free(sa);
		return (NULL);
	}
	sa->sa_start = sa->sa_scanfile->cf_addr;
	sa->sa_end = sa->sa_start + (size_t)sa->sa_scanfile->cf_size;

	return (sa);
}

void
scanfile_close(struct scanfile_args *sa)
{
	if (sa == NULL)
		return;

	if (sa->sa_scanfile != NULL)
		cvsync_fclose(sa->sa_scanfile);
	free(sa);
}

bool
scanfile_read_attr(uint8_t *sp, const uint8_t *bp, struct scanfile_attr *attr)
{
	if (bp - sp < 3) {
		logmsg_err("Scanfile Error: read attr type/namelen");
		return (false);
	}
	attr->a_type = sp[0];
	if ((attr->a_namelen = GetWord(&sp[1])) == 0) {
		logmsg_err("Scanfile Error: attr namelen is zero");
		return (false);
	}
	sp += 3;
	if ((size_t)(bp - sp) < attr->a_namelen) {
		logmsg_err("Scanfile Error: read attr name");
		return (false);
	}
	attr->a_name = sp;
	sp += attr->a_namelen;
	if (bp - sp < 2) {
		logmsg_err("Scanfile Error: read attr auxlen");
		return (false);
	}
	if ((attr->a_auxlen = GetWord(sp)) == 0) {
		logmsg_err("Scanfile Error: attr auxlen is zero");
		return (false);
	}
	sp += 2;
	if ((size_t)(bp - sp) < attr->a_auxlen) {
		logmsg_err("Scanfile Error: read attr aux");
		return (false);
	}
	attr->a_aux = sp;

	attr->a_size = attr->a_namelen + attr->a_auxlen + 5;

	return (true);
}

bool
scanfile_write_attr(struct scanfile_args *sa, struct scanfile_attr *attr)
{
	struct iovec iov[4];
	uint8_t buffer1[3], buffer2[2];
	size_t len;
	ssize_t wn;

	buffer1[0] = attr->a_type;
	SetWord(&buffer1[1], attr->a_namelen);
	SetWord(buffer2, attr->a_auxlen);

	iov[0].iov_base = (void *)buffer1;
	iov[0].iov_len = 3;
	iov[1].iov_base = attr->a_name;
	iov[1].iov_len = attr->a_namelen;
	iov[2].iov_base = (void *)buffer2;
	iov[2].iov_len = 2;
	iov[3].iov_base = attr->a_aux;
	iov[3].iov_len = attr->a_auxlen;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
	      iov[3].iov_len;
	if ((wn = writev(sa->sa_tmp, iov, 4)) == -1) {
		logmsg_err("Scanfile Error: write attr");
		return (false);
	}
	if ((size_t)wn != len) {
		logmsg_err("Scanfile Error: write attr");
		return (false);
	}

	return (true);
}

bool
scanfile_create(struct scanfile_create_args *sca)
{
	switch (cvsync_release_pton(sca->sca_release)) {
	case CVSYNC_RELEASE_RCS:
		if (!scanfile_rcs(sca)) {
			logmsg_err("Scanfile Error: %s: failed to create",
				   sca->sca_name);
			return (false);
		}
		break;
	default:
		logmsg_err("Scanfile Error: unknown release type");
		return (false);
	}

	return (true);
}

bool
scanfile_create_tmpfile(struct scanfile_args *sa, mode_t mode)
{
	const char *ep;
	size_t len;

	if (sa == NULL)
		return (true);

	len = strlen(sa->sa_scanfile_name);
	for (ep = &sa->sa_scanfile_name[len - 1] ;
	     ep >= sa->sa_scanfile_name ;
	     ep--) {
		if (*ep == '/')
			break;
	}
	if (ep > sa->sa_scanfile_name)
		len = (size_t)(ep - sa->sa_scanfile_name + 1);
	else
		len = 0;
	if (len > 0)
		(void)memcpy(sa->sa_tmp_name, sa->sa_scanfile_name, len);
	(void)memcpy(&sa->sa_tmp_name[len], CVSYNC_TMPFILE,
		     CVSYNC_TMPFILE_LEN);
	sa->sa_tmp_name[len + CVSYNC_TMPFILE_LEN] = '\0';

	if ((sa->sa_tmp = mkstemp(sa->sa_tmp_name)) == -1) {
		logmsg_err("Scanfile Error: %s: %s",  sa->sa_tmp_name,
			   strerror(errno));
		return (false);
	}
	sa->sa_tmp_mode = mode;

	if ((sa->sa_dirlist = list_init()) == NULL) {
		logmsg_err("Scanfile Error: list init");
		(void)unlink(sa->sa_tmp_name);
		(void)close(sa->sa_tmp);
		return (false);
	}
	list_set_destructor(sa->sa_dirlist, free);

	return (true);
}

void
scanfile_remove_tmpfile(struct scanfile_args *sa)
{
	if (sa == NULL)
		return;

	if (sa->sa_tmp != -1)
		(void)close(sa->sa_tmp);
	if (strlen(sa->sa_tmp_name) != 0)
		(void)unlink(sa->sa_tmp_name);
	if (sa->sa_dirlist != NULL)
		list_destroy(sa->sa_dirlist);
}

bool
scanfile_rename(struct scanfile_args *sa)
{
	ssize_t wn;
	size_t len;

	if (sa == NULL)
		return (true);

	if (!sa->sa_changed) {
		if (close(sa->sa_tmp) == -1) {
			logmsg_err("Scanfile Error: %s", strerror(errno));
			return (false);
		}
		sa->sa_tmp = -1;
		if (unlink(sa->sa_tmp_name) == -1) {
			logmsg_err("Scanfile Error: %s", strerror(errno));
			return (false);
		}
		sa->sa_tmp_name[0] = '\0';
		if (sa->sa_dirlist != NULL) {
			list_destroy(sa->sa_dirlist);
			sa->sa_dirlist = NULL;
		}
		return (true);
	}

	if (sa->sa_scanfile != NULL) {
		if (!scanfile_flush_attr(sa, NULL)) {
			logmsg_err("Scanfile Error: Flush");
			return (false);
		}
		while (sa->sa_start < sa->sa_end) {
			if ((len = sa->sa_end - sa->sa_start) > CVSYNC_BSIZE)
				len = CVSYNC_BSIZE;
			if ((wn = write(sa->sa_tmp, sa->sa_start,
					len)) == -1) {
				if (errno == EINTR) {
					logmsg_intr();
					continue;
				}
				logmsg_err("Scanfile Error: Flush: %s",
					   strerror(errno));
				return (false);
			}
			if (wn == 0)
				break;
			sa->sa_start += wn;
		}
		if (sa->sa_start != sa->sa_end) {
			logmsg_err("Scanfile Error: Flush");
			return (false);
		}
		cvsync_fclose(sa->sa_scanfile);
		sa->sa_scanfile = NULL;
	}
	if (sa->sa_tmp != -1) {
		if (fchmod(sa->sa_tmp, sa->sa_tmp_mode) == -1) {
			logmsg_err("Scanfile Error: %s", strerror(errno));
			return (false);
		}
		if (close(sa->sa_tmp) == -1) {
			logmsg_err("Scanfile Error: %s", strerror(errno));
			return (false);
		}
		sa->sa_tmp = -1;
		if (rename(sa->sa_tmp_name, sa->sa_scanfile_name) == -1) {
			logmsg_err("Scanfile Error: %s", strerror(errno));
			return (false);
		}
		sa->sa_tmp_name[0] = '\0';
	}
	if (sa->sa_dirlist != NULL) {
		list_destroy(sa->sa_dirlist);
		sa->sa_dirlist = NULL;
	}

	return (true);
}

bool
scanfile_add(struct scanfile_args *sa, uint8_t type, void *name,
	     size_t namelen, void *aux, size_t auxlen)
{
	struct scanfile_attr *attr = &sa->sa_attr;
	int rv;

	sa->sa_changed = true;

	while (sa->sa_start < sa->sa_end) {
		if (!scanfile_read_attr(sa->sa_start, sa->sa_end, attr))
			return (false);

		if ((rv = cvsync_cmp_pathname(attr->a_name, attr->a_namelen,
					      name, namelen)) == 0) {
			return (false);
		}
		if (rv > 0)
			break;
		/* rv < 0 */
		if (!scanfile_flush_attr(sa, attr))
			return (false);
		if (!scanfile_write_attr(sa, attr))
			return (false);
		sa->sa_start += attr->a_size;
	}

	attr->a_type = type;
	attr->a_name = name;
	attr->a_namelen = namelen;
	attr->a_aux = aux;
	attr->a_auxlen = auxlen;

	if (!scanfile_flush_attr(sa, attr))
		return (false);
	if (!scanfile_write_attr(sa, attr))
		return (false);

	return (true);
}

bool
scanfile_remove(struct scanfile_args *sa, uint8_t type, void *name,
		size_t namelen)
{
	struct scanfile_attr *attr = &sa->sa_attr;
	int rv;

	sa->sa_changed = true;

	while (sa->sa_start < sa->sa_end) {
		if (!scanfile_read_attr(sa->sa_start, sa->sa_end, attr))
			return (false);

		if ((rv = cvsync_cmp_pathname(attr->a_name, attr->a_namelen,
					      name, namelen)) == 0) {
			sa->sa_start += attr->a_size;
			break;
		}
		if (rv > 0)
			break;
		/* rv < 0 */
		if (attr->a_type == FILETYPE_DIR) {
			if (!scanfile_insert_attr(sa, attr))
				return (false);
		} else {
			if (!scanfile_flush_attr(sa, attr))
				return (false);
			if (!scanfile_write_attr(sa, attr))
				return (false);
		}
		sa->sa_start += attr->a_size;
	}

	attr->a_type = type;
	attr->a_name = name;
	attr->a_namelen = namelen;

	if (attr->a_type == FILETYPE_DIR) {
		if (!scanfile_remove_attr(sa, attr))
			return (false);
	}

	return (true);
}

bool
scanfile_replace(struct scanfile_args *sa, uint8_t type, void *name,
		 size_t namelen, void *aux, size_t auxlen)
{
	struct scanfile_attr *attr = &sa->sa_attr;
	int rv;

	sa->sa_changed = true;

	while (sa->sa_start < sa->sa_end) {
		if (!scanfile_read_attr(sa->sa_start, sa->sa_end, attr))
			return (false);

		if ((rv = cvsync_cmp_pathname(attr->a_name, attr->a_namelen,
					      name, namelen)) > 0) {
			return (false);
		}
		if (rv == 0)
			break;
		/* rv < 0 */
		if (!scanfile_flush_attr(sa, attr))
			return (false);
		if (!scanfile_write_attr(sa, attr))
			return (false);
		sa->sa_start += attr->a_size;
	}
	if (sa->sa_start == sa->sa_end)
		return (false);

	sa->sa_start += attr->a_size;

	attr->a_type = type;
	attr->a_name = name;
	attr->a_namelen = namelen;
	attr->a_aux = aux;
	attr->a_auxlen = auxlen;

	if (!scanfile_flush_attr(sa, attr))
		return (false);
	if (!scanfile_write_attr(sa, attr))
		return (false);

	return (true);
}

bool
scanfile_update(struct scanfile_args *sa, uint8_t type, void *name,
		size_t namelen, void *aux, size_t auxlen)
{
	struct scanfile_attr *attr = &sa->sa_attr;
	int rv;

	sa->sa_changed = true;

	while (sa->sa_start < sa->sa_end) {
		if (!scanfile_read_attr(sa->sa_start, sa->sa_end, attr))
			return (false);

		if ((rv = cvsync_cmp_pathname(attr->a_name, attr->a_namelen,
					      name, namelen)) > 0) {
			return (false);
		}
		if (rv == 0)
			break;
		/* rv < 0 */
		if (!scanfile_flush_attr(sa, attr))
			return (false);
		if (!scanfile_write_attr(sa, attr))
			return (false);
		sa->sa_start += attr->a_size;
	}
	if (sa->sa_start == sa->sa_end)
		return (false);

	sa->sa_start += attr->a_size;

	if (attr->a_type != type)
		return (false);
	if ((attr->a_type != FILETYPE_SYMLINK) && (attr->a_auxlen != auxlen))
		return (false);

	attr->a_type = type;
	attr->a_name = name;
	attr->a_namelen = namelen;
	attr->a_aux = aux;
	attr->a_auxlen = auxlen;

	if (!scanfile_flush_attr(sa, attr))
		return (false);
	if (!scanfile_write_attr(sa, attr))
		return (false);

	return (true);
}

bool
scanfile_insert_attr(struct scanfile_args *sa, struct scanfile_attr *attr)
{
	struct scanfile_attr *dirattr;

	if ((dirattr = malloc(sizeof(*dirattr))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	(void)memcpy(dirattr, attr, sizeof(*dirattr));

	if (!list_insert_tail(sa->sa_dirlist, dirattr)) {
		free(dirattr);
		return (false);
	}

	return (true);
}

bool
scanfile_remove_attr(struct scanfile_args *sa, struct scanfile_attr *attr)
{
	struct listent *lep;
	struct scanfile_attr *dirattr;

	for (lep = sa->sa_dirlist->l_head ; lep != NULL ; lep = lep->le_next) {
		dirattr = lep->le_elm;
		if ((dirattr->a_namelen == attr->a_namelen) &&
		    (memcmp(dirattr->a_name, attr->a_name,
			    attr->a_namelen) == 0)) {
			break;
		}
	}
	if (lep != NULL) {
		dirattr = lep->le_elm;
		if (!list_remove(sa->sa_dirlist, lep))
			return (false);
		free(dirattr);
	}

	return (true);
}

bool
scanfile_flush_attr(struct scanfile_args *sa, struct scanfile_attr *attr)
{
	struct scanfile_attr *dirattr;

	while (!list_isempty(sa->sa_dirlist)) {
		if ((dirattr = list_remove_head(sa->sa_dirlist)) == NULL)
			return (false);
		if (attr != NULL) {
			if (cvsync_cmp_pathname(dirattr->a_name,
						dirattr->a_namelen,
						attr->a_name,
						attr->a_namelen) > 0) {
				if (!list_insert_head(sa->sa_dirlist, dirattr))
					return (false);
				break;
			}
		}
		if (!scanfile_write_attr(sa, dirattr))
			return (false);
		free(dirattr);
	}

	return (true);
}
