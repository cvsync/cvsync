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
#include <sys/stat.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#include "compat_sys_stat.h"
#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_unistd.h"
#include "basedef.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "rcslib.h"

#include "updater.h"

bool updater_rcs_fetch(struct updater_args *);

bool updater_rcs_add(struct updater_args *);
bool updater_rcs_add_dir(struct updater_args *);
bool updater_rcs_add_file(struct updater_args *);
bool updater_rcs_add_symlink(struct updater_args *);
bool updater_rcs_remove(struct updater_args *);
bool updater_rcs_attic(struct updater_args *);
bool updater_rcs_setattr(struct updater_args *);
bool updater_rcs_update(struct updater_args *);
bool updater_rcs_update_rcs(struct updater_args *);
bool updater_rcs_update_symlink(struct updater_args *);

bool updater_rcs_admin(struct updater_args *, struct rcslib_file *);
bool updater_rcs_delta(struct updater_args *, struct rcslib_file *);
bool updater_rcs_desc(struct updater_args *);
bool updater_rcs_deltatext(struct updater_args *, struct rcslib_file *);

bool updater_rcs_write_delta(struct updater_args *, uint8_t *, size_t);
bool updater_rcs_write_deltatext(struct updater_args *, struct rcsnum *);

bool
updater_rcs(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;

	for (;;) {
		if (cvsync_isinterrupted())
			return (false);

		if (!updater_rcs_fetch(uda))
			return (false);

		if (cap->ca_tag == UPDATER_END)
			break;

		switch (cap->ca_tag) {
		case UPDATER_ADD:
			if (!updater_rcs_add(uda)) {
				logmsg_err("Updater(RCS): ADD: %s",
					   uda->uda_path);
				return (false);
			}
			break;
		case UPDATER_REMOVE:
			if (!updater_rcs_remove(uda)) {
				logmsg_err("Updater(RCS): REMOVE: %s",
					   uda->uda_path);
				return (false);
			}
			break;
		case UPDATER_RCS_ATTIC:
			if (!updater_rcs_attic(uda)) {
				logmsg_err("Updater(RCS): ATTIC: %s",
					   uda->uda_path);
				return (false);
			}
			break;
		case UPDATER_SETATTR:
			if (!updater_rcs_setattr(uda)) {
				logmsg_err("Updater(RCS): SETATTR: %s",
					   uda->uda_path);
				return (false);
			}
			break;
		case UPDATER_UPDATE:
			if (!updater_rcs_update(uda)) {
				logmsg_err("Updater(RCS): UPDATE: %s",
					   uda->uda_path);
				return (false);
			}
			break;
		default:
			return (false);
		}
	}

	return (true);
}

bool
updater_rcs_fetch(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	uint8_t *cmd = uda->uda_cmd;
	size_t pathlen, len;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > uda->uda_cmdmax - 2))
		return (false);
	if ((cap->ca_tag = cmd[2]) == UPDATER_END)
		return (len == 1);
	if (len < 2)
		return (false);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);

	cap->ca_type = cmd[0];
	if ((cap->ca_namelen = GetWord(&cmd[1])) > sizeof(cap->ca_name))
		return (false);

	switch (cap->ca_tag) {
	case UPDATER_ADD:
	case UPDATER_REMOVE:
		if (cap->ca_namelen != len - 4)
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		break;
	case UPDATER_RCS_ATTIC:
		if ((cap->ca_type != FILETYPE_RCS) &&
		    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
			return (false);
		}
		if (cap->ca_namelen != len - RCS_ATTRLEN_RCS - 4)
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!IS_FILE_RCS(cap->ca_name, cap->ca_namelen))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd,
			      RCS_ATTRLEN_RCS)) {
			return (false);
		}
		if (!attr_rcs_decode_rcs(cmd, RCS_ATTRLEN_RCS, cap))
			return (false);
		break;
	case UPDATER_UPDATE:
		if (cap->ca_type == FILETYPE_DIR)
			return (false);
		if (cap->ca_type == FILETYPE_SYMLINK) {
			if (len <= cap->ca_namelen + 4)
				return (false);
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			cap->ca_auxlen = len - cap->ca_namelen - 4;
			if (cap->ca_auxlen > sizeof(cap->ca_aux))
				return (false);
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      cap->ca_aux, cap->ca_auxlen)) {
				return (false);
			}
			break;
		}
		/* FALLTHROUGH */
	case UPDATER_SETATTR:
		switch (cap->ca_type) {
		case FILETYPE_DIR:
			if (cap->ca_namelen != len - RCS_ATTRLEN_DIR - 4)
				return (false);
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd,
				      RCS_ATTRLEN_DIR)) {
				return (false);
			}
			if (!attr_rcs_decode_dir(cmd, RCS_ATTRLEN_DIR, cap))
				return (false);
			break;
		case FILETYPE_FILE:
			if (cap->ca_namelen != len - RCS_ATTRLEN_FILE - 4)
				return (false);
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd,
				      RCS_ATTRLEN_FILE)) {
				return (false);
			}
			if (!attr_rcs_decode_file(cmd, RCS_ATTRLEN_FILE, cap))
				return (false);
			break;
		case FILETYPE_RCS:
		case FILETYPE_RCS_ATTIC:
			if (cap->ca_namelen != len - RCS_ATTRLEN_RCS - 4)
				return (false);
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!IS_FILE_RCS(cap->ca_name, cap->ca_namelen))
				return (false);
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd,
				      RCS_ATTRLEN_RCS)) {
				return (false);
			}
			if (!attr_rcs_decode_rcs(cmd, RCS_ATTRLEN_RCS, cap))
				return (false);
			break;
		default:
			return (false);
		}
		break;
	default:
		return (false);
	}

	if (!cvsync_rcs_pathname((char *)cap->ca_name, cap->ca_namelen))
		return (false);

	if ((pathlen = uda->uda_pathlen + cap->ca_namelen) >= uda->uda_pathmax)
		return (false);

	(void)memcpy(uda->uda_rpath, cap->ca_name, cap->ca_namelen);
	uda->uda_rpath[cap->ca_namelen] = '\0';
	if (cap->ca_type == FILETYPE_RCS_ATTIC) {
		if (!cvsync_rcs_insert_attic(uda->uda_path, pathlen,
					     uda->uda_pathmax)) {
			return (false);
		}
	}

	cap->ca_mode = RCS_MODE(cap->ca_mode, uda->uda_umask);

	return (true);
}

bool
updater_rcs_add(struct updater_args *uda)
{
	switch (uda->uda_attr.ca_type) {
	case FILETYPE_DIR:
		return (updater_rcs_add_dir(uda));
	case FILETYPE_FILE:
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		return (updater_rcs_add_file(uda));
	case FILETYPE_SYMLINK:
		return (updater_rcs_add_symlink(uda));
	default:
		break;
	}

	return (false);
}

bool
updater_rcs_add_dir(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = uda->uda_cmd;
	size_t len;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
		return (false);
	if ((len = GetWord(cmd)) != RCS_ATTRLEN_DIR)
		return (false);
	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
		return (false);
	if (!attr_rcs_decode_dir(cmd, len, cap))
		return (false);
	mode = RCS_MODE(cap->ca_mode, uda->uda_umask);

	if (mkdir(uda->uda_path, mode) == -1) {
		if (errno != EEXIST) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			return (false);
		}
		if (lstat(uda->uda_path, &st) == -1) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			return (false);
		}
		if (!S_ISDIR(st.st_mode)) {
			logmsg_err("%s: %s", uda->uda_path, strerror(ENOTDIR));
			return (false);
		}
		if (RCS_MODE(st.st_mode, 0) == mode) {
			if (!updater_rcs_scanfile_remove(uda))
				return (false);
			return (true);
		}
	}
	if (chmod(uda->uda_path, mode) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		return (false);
	}

	if (!updater_rcs_scanfile_mkdir(uda))
		return (false);

	return (true);
}

bool
updater_rcs_add_file(struct updater_args *uda)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	struct cvsync_attr *cap = &uda->uda_attr;
	struct stat st;
	struct utimbuf times;
	uint64_t size;
	uint16_t mode = RCS_MODE(RCS_PERMS, uda->uda_umask);
	uint8_t *cmd = uda->uda_cmd;
	char *ep;
	ssize_t wn;
	size_t len;
	int fd;

	if (cap->ca_type == FILETYPE_RCS_ATTIC) {
		len = uda->uda_pathlen + cap->ca_namelen + 6;
		for (ep = &uda->uda_path[len - 1] ;
		     ep >= uda->uda_path ;
		     ep--) {
			if (*ep == '/')
				break;
		}
		if (ep < uda->uda_path)
			return (false);
		*ep = '\0';
		if (mkdir(uda->uda_path, mode) == -1) {
			if (errno != EEXIST) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (lstat(uda->uda_path, &st) == -1) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (!S_ISDIR(st.st_mode)) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(ENOTDIR));
				return (false);
			}
		}
		*ep = '/';
	}

	ep = &uda->uda_path[uda->uda_pathlen + cap->ca_namelen - 1];
	while (ep >= uda->uda_path) {
		if (*ep == '/') {
			ep++;
			break;
		}
		ep--;
	}
	len = (size_t)(ep - uda->uda_path);
	(void)memcpy(uda->uda_tmpfile, uda->uda_path, len);
	(void)memcpy(&uda->uda_tmpfile[len], CVSYNC_TMPFILE,
		     CVSYNC_TMPFILE_LEN);
	uda->uda_tmpfile[len + CVSYNC_TMPFILE_LEN] = '\0';

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len <= 8) || (len > uda->uda_cmdmax - 2))
		return (false);
	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
		return (false);

	if (!attr_rcs_decode_file(cmd, len, cap))
		return (false);

	if ((fd = mkstemp(uda->uda_tmpfile)) == -1) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	if (!(*hashops->init)(&uda->uda_hash_ctx)) {
		logmsg_err("%s: hash init", uda->uda_path);
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	size = (uint64_t)cap->ca_size;

	while (size > 0) {
		if (size < (uint64_t)uda->uda_bufsize)
			len = (size_t)size;
		else
			len = uda->uda_bufsize;

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
			      uda->uda_buffer, len)) {
			(*hashops->destroy)(uda->uda_hash_ctx);
			(void)unlink(uda->uda_tmpfile);
			(void)close(fd);
			return (false);
		}

		if ((wn = write(fd, uda->uda_buffer, len)) == -1) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			(void)unlink(uda->uda_tmpfile);
			(void)close(fd);
			return (false);
		}
		if ((size_t)wn != len) {
			logmsg_err("%s: write error", uda->uda_path);
			(*hashops->destroy)(uda->uda_hash_ctx);
			(void)unlink(uda->uda_tmpfile);
			(void)close(fd);
			return (false);
		}

		(*hashops->update)(uda->uda_hash_ctx, uda->uda_buffer, len);

		size -= (uint64_t)len;
	}
	if (size > 0) {
		logmsg_err("%s: residue %" PRIu64, uda->uda_path, size);
		(*hashops->destroy)(uda->uda_hash_ctx);
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	(*hashops->final)(uda->uda_hash_ctx, uda->uda_hash);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, hashops->length)) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}
	if (memcmp(uda->uda_hash, cmd, hashops->length) != 0) {
		logmsg_err("Updater Error: rcs: %s: hash mismatch",
			   uda->uda_path);
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3)) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}
	if (GetWord(cmd) != 1) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}
	if (cmd[2] != UPDATER_END) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	if (fchmod(fd, cap->ca_mode) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	if (close(fd) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	times.actime = (time_t)cap->ca_mtime;
	times.modtime = (time_t)cap->ca_mtime;

	if (utime(uda->uda_tmpfile, &times) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	if (rename(uda->uda_tmpfile, uda->uda_path) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	if (!updater_rcs_scanfile_create(uda))
		return (false);

	return (true);
}

bool
updater_rcs_add_symlink(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	uint8_t *cmd = uda->uda_cmd;
	size_t len;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len >= uda->uda_pathmax))
		return (false);
	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
		return (false);
	cmd[len] = '\0';

	if (symlink((char *)cmd, uda->uda_path) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		return (false);
	}

	(void)memcpy(cap->ca_aux, cmd, len);
	cap->ca_auxlen = len;

	if (!updater_rcs_scanfile_create(uda))
		return (false);

	return (true);
}

bool
updater_rcs_remove(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	char *sp, *ep;
	size_t pathlen;

	switch (cap->ca_type) {
	case FILETYPE_DIR:
		if (rmdir(uda->uda_path) != -1)
			break;
		if ((errno != ENOENT) && (errno != ENOTEMPTY)) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			return (false);
		}
		if (errno == ENOENT)
			break;
		pathlen = uda->uda_pathlen + cap->ca_namelen;
		if (!cvsync_rcs_append_attic(uda->uda_path, pathlen,
					     uda->uda_pathmax)) {
			return (false);
		}
		if (rmdir(uda->uda_path) == -1) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			return (false);
		}
		uda->uda_path[pathlen] = '\0';
		if (rmdir(uda->uda_path) == -1) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			return (false);
		}
		break;
	case FILETYPE_FILE:
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if (unlink(uda->uda_path) == -1) {
			if (errno != ENOENT) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
		}
		if (cap->ca_type == FILETYPE_RCS_ATTIC) {
			pathlen = uda->uda_pathlen + cap->ca_namelen + 6;
			for (ep = &uda->uda_path[pathlen - 1] ;
			     ep >= uda->uda_path ;
			     ep--) {
				if (*ep == '/')
					break;
			}
			if (ep < uda->uda_path)
				return (false);
			for (sp = ep - 1 ; sp >= uda->uda_path ; sp--) {
				if (*sp == '/')
					break;
			}
			if (sp < uda->uda_path)
				sp = uda->uda_path;
			else
				sp++;
			if (!IS_DIR_ATTIC(sp, ep - sp))
				return (false);
			*ep = '\0';
			if (rmdir(uda->uda_path) == -1) {
				if ((errno != EEXIST) &&
				    (errno != ENOTEMPTY)) {
					logmsg_err("%s: %s", uda->uda_path,
						   strerror(errno));
					return (false);
				}
			}
			*ep = '/';
		}
		break;
	case FILETYPE_SYMLINK:
		if (unlink(uda->uda_path) == -1) {
			if (errno != ENOENT) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
		}
		break;
	default:
		return (false);
	}

	if (!updater_rcs_scanfile_remove(uda))
		return (false);

	return (true);
}

bool
updater_rcs_attic(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct stat st;
	uint16_t mode = RCS_MODE(RCS_PERMS, uda->uda_umask);
	uint8_t *cmd = uda->uda_cmd;
	char *sp, *ep;
	size_t pathlen, len;

	ep = &uda->uda_path[uda->uda_pathlen + cap->ca_namelen - 1];
	while (ep >= uda->uda_path) {
		if (*ep == '/') {
			ep++;
			break;
		}
		ep--;
	}
	len = (size_t)(ep - uda->uda_path);
	(void)memcpy(uda->uda_tmpfile, uda->uda_path, len);
	(void)memcpy(&uda->uda_tmpfile[len], CVSYNC_TMPFILE,
		     CVSYNC_TMPFILE_LEN);
	uda->uda_tmpfile[len + CVSYNC_TMPFILE_LEN] = '\0';

	switch (cap->ca_type) {
	case FILETYPE_RCS:
		pathlen = uda->uda_pathlen + cap->ca_namelen;
		if (!cvsync_rcs_insert_attic(uda->uda_path, pathlen,
					     uda->uda_pathmax)) {
			return (false);
		}
		break;
	case FILETYPE_RCS_ATTIC:
		pathlen = uda->uda_pathlen + cap->ca_namelen + 6;
		for (ep = &uda->uda_path[pathlen - 1] ;
		     ep >= uda->uda_path ;
		     ep--) {
			if (*ep == '/')
				break;
		}
		if (ep < uda->uda_path)
			return (false);
		*ep = '\0';
		if (mkdir(uda->uda_path, mode) == -1) {
			if (errno != EEXIST) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (lstat(uda->uda_path, &st) == -1) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (!S_ISDIR(st.st_mode)) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(ENOTDIR));
				return (false);
			}
		}
		*ep = '/';
		pathlen = uda->uda_pathlen + cap->ca_namelen + 6;
		if (!cvsync_rcs_remove_attic(uda->uda_path, pathlen))
			return (false);
		break;
	default:
		return (false);
	}

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	cap->ca_tag = cmd[2];

	switch (cap->ca_tag) {
	case UPDATER_UPDATE_GENERIC:
		if (!updater_generic_update(uda))
			return (false);
		break;
	case UPDATER_UPDATE_RCS:
		if (!updater_rcs_update_rcs(uda))
			return (false);
		break;
	case UPDATER_UPDATE_RDIFF:
		if (!updater_rdiff_update(uda))
			return (false);
		break;
	default:
		return (false);
	}

	if (unlink(uda->uda_path) == -1) {
		if (errno != ENOENT) {
			logmsg_err("%s: %s", uda->uda_path, strerror(errno));
			(void)unlink(uda->uda_tmpfile);
			return (false);
		}
	}

	switch (cap->ca_type) {
	case FILETYPE_RCS:
		pathlen = uda->uda_pathlen + cap->ca_namelen + 6;
		for (ep = &uda->uda_path[pathlen - 1] ;
		     ep >= uda->uda_path ;
		     ep--) {
			if (*ep == '/')
				break;
		}
		if (ep < uda->uda_path)
			return (false);
		for (sp = ep - 1 ; sp >= uda->uda_path ; sp--) {
			if (*sp == '/')
				break;
		}
		if (sp < uda->uda_path)
			sp = uda->uda_path;
		else
			sp++;
		if (!IS_DIR_ATTIC(sp, ep - sp))
			return (false);
		*ep = '\0';
		if (rmdir(uda->uda_path) == -1) {
			if ((errno != EEXIST) && (errno != ENOTEMPTY)) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
		}
		*ep = '/';
		if (!cvsync_rcs_remove_attic(uda->uda_path, pathlen))
			return (false);
		break;
	case FILETYPE_RCS_ATTIC:
		pathlen = uda->uda_pathlen + cap->ca_namelen;
		if (!cvsync_rcs_insert_attic(uda->uda_path, pathlen,
					     uda->uda_pathmax)) {
			return (false);
		}
		break;
	}

	if (rename(uda->uda_tmpfile, uda->uda_path) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != UPDATER_UPDATE_END)
		return (false);

	if (!updater_rcs_scanfile_attic(uda))
		return (false);

	return (true);
}

bool
updater_rcs_setattr(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct utimbuf times;

	switch (cap->ca_type) {
	case FILETYPE_DIR:
		if (chmod(uda->uda_path, cap->ca_mode) == -1) {
			if (errno != ENOENT) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (!updater_rcs_scanfile_remove(uda))
				return (false);
			return (true);
		}
		break;
	case FILETYPE_FILE:
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		times.actime = (time_t)cap->ca_mtime;
		times.modtime = (time_t)cap->ca_mtime;

		if (utime(uda->uda_path, &times) == -1) {
			if (errno != ENOENT) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (!updater_rcs_scanfile_remove(uda))
				return (false);
			return (true);
		}
		if (chmod(uda->uda_path, cap->ca_mode) == -1) {
			if (errno != ENOENT) {
				logmsg_err("%s: %s", uda->uda_path,
					   strerror(errno));
				return (false);
			}
			if (!updater_rcs_scanfile_remove(uda))
				return (false);
			return (true);
		}
		break;
	default:
		return (false);
	}

	if (!updater_rcs_scanfile_setattr(uda))
		return (false);

	return (true);
}

bool
updater_rcs_update(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	uint8_t *cmd = uda->uda_cmd;
	char *ep;
	size_t len;

	if (cap->ca_type == FILETYPE_SYMLINK)
		return (updater_rcs_update_symlink(uda));

	ep = &uda->uda_path[uda->uda_pathlen + cap->ca_namelen - 1];
	while (ep >= uda->uda_path) {
		if (*ep == '/') {
			ep++;
			break;
		}
		ep--;
	}
	len = (size_t)(ep - uda->uda_path);
	(void)memcpy(uda->uda_tmpfile, uda->uda_path, len);
	(void)memcpy(&uda->uda_tmpfile[len], CVSYNC_TMPFILE,
		     CVSYNC_TMPFILE_LEN);
	uda->uda_tmpfile[len + CVSYNC_TMPFILE_LEN] = '\0';

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	cap->ca_tag = cmd[2];

	switch (cap->ca_tag) {
	case UPDATER_UPDATE_GENERIC:
		if (!updater_generic_update(uda))
			return (false);
		break;
	case UPDATER_UPDATE_RCS:
		if (!updater_rcs_update_rcs(uda))
			return (false);
		break;
	case UPDATER_UPDATE_RDIFF:
		if (!updater_rdiff_update(uda))
			return (false);
		break;
	default:
		return (false);
	}

	if (rename(uda->uda_tmpfile, uda->uda_path) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}
	if ((unlink(uda->uda_tmpfile) == -1) && (errno != ENOENT)) {
		logmsg_err("%s: %s", uda->uda_tmpfile, strerror(errno));
		return (false);
	}

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != UPDATER_UPDATE_END)
		return (false);

	if (!updater_rcs_scanfile_update(uda))
		return (false);

	return (true);
}

bool
updater_rcs_update_rcs(struct updater_args *uda)
{
	struct cvsync_file *cfp;
	struct rcslib_file *rcs;
	struct cvsync_attr *cap = &uda->uda_attr;
	struct utimbuf times;

	if ((uda->uda_fileno = mkstemp(uda->uda_tmpfile)) == -1) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	if ((cfp = cvsync_fopen(uda->uda_path)) == NULL) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}
	if (!cvsync_mmap(cfp, 0, cfp->cf_size)) {
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if ((rcs = rcslib_init(cfp->cf_addr, cfp->cf_size)) == NULL) {
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (!updater_rcs_admin(uda, rcs)) {
		logmsg_err("Updater(RCS): UPDATE(admin): error");
		rcslib_destroy(rcs);
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}
	if (!updater_rcs_delta(uda, rcs)) {
		logmsg_err("Updater(RCS): UPDATE(delta): error");
		rcslib_destroy(rcs);
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}
	if (!updater_rcs_desc(uda)) {
		logmsg_err("Updater(RCS): UPDATE(desc): error");
		rcslib_destroy(rcs);
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}
	if (!updater_rcs_deltatext(uda, rcs)) {
		logmsg_err("Updater(RCS): UPDATE(deltatext): error");
		rcslib_destroy(rcs);
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	rcslib_destroy(rcs);

	if (!cvsync_fclose(cfp)) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (fchmod(uda->uda_fileno, cap->ca_mode) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (close(uda->uda_fileno) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	times.actime = (time_t)cap->ca_mtime;
	times.modtime = (time_t)cap->ca_mtime;

	if (utime(uda->uda_tmpfile, &times) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	return (true);
}

bool
updater_rcs_update_symlink(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;

	if (cap->ca_auxlen >= uda->uda_pathmax)
		return (false);
	(void)memcpy(uda->uda_symlink, cap->ca_aux, cap->ca_auxlen);
	uda->uda_symlink[cap->ca_auxlen] = '\0';

	if (unlink(uda->uda_path) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		return (false);
	}
	if (symlink(uda->uda_symlink, uda->uda_path) == -1) {
		logmsg_err("%s: %s", uda->uda_path, strerror(errno));
		return (false);
	}

	cap->ca_tag = UPDATER_UPDATE_GENERIC;
	if (!updater_rcs_scanfile_update(uda))
		return (false);

	return (true);
}

bool
updater_rcs_admin(struct updater_args *uda, struct rcslib_file *rcs)
{
	struct rcslib_lock *lock1, *lock2, t_lock;
	struct rcslib_symbol *sym1, *sym2, t_sym;
	struct rcsid *i1, *i2;
	struct rcsnum *n1, *n2;
	struct rcssym *s1, *s2;
	struct iovec iov[4];
	uint8_t *cmd = uda->uda_cmd;
	size_t len, iovlen, c;
	ssize_t wn;
	int fd = uda->uda_fileno, rv;

	lock2 = &t_lock;
	sym2 = &t_sym;
	i2 = &lock2->id;
	n2 = &sym2->num;
	s2 = &sym2->sym;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > uda->uda_cmdmax - 2))
		return (false);
	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
		return (false);

	/* head */
	iov[0].iov_base = "head\t";
	if (cmd[0] == UPDATER_UPDATE_RCS_HEAD) {
		if ((len < 2) || (len != (size_t)cmd[1] + 2))
			return (false);
		(void)memcpy(uda->uda_buffer, &cmd[2], cmd[1]);
		iov[1].iov_base = uda->uda_buffer;
		iov[1].iov_len = cmd[1];

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	} else {
		iov[1].iov_base = rcs->head.n_str;
		iov[1].iov_len = rcs->head.n_len;
	}
	if (iov[1].iov_len > 0)
		iov[0].iov_len = 5;
	else
		iov[0].iov_len = 4;
	iov[2].iov_base = ";\n";
	iov[2].iov_len = 2;
	iovlen = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;

	if ((wn = writev(fd, iov, 3)) == -1) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	if ((size_t)wn != iovlen) {
		logmsg_err("writev error");
		return (false);
	}

	/* branch */
	if (cmd[0] == UPDATER_UPDATE_RCS_BRANCH) {
		if ((len < 2) || (len != (size_t)cmd[1] + 2))
			return (false);
		if (cmd[1] > 0) {
			(void)memcpy(uda->uda_buffer, &cmd[2], cmd[1]);
			iov[1].iov_base = uda->uda_buffer;
			iov[1].iov_len = cmd[1];
		} else {
			iov[1].iov_len = 0;
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	} else {
		if (rcs->branch.n_len > 0) {
			iov[1].iov_base = rcs->branch.n_str;
			iov[1].iov_len = rcs->branch.n_len;
		} else {
			iov[1].iov_len = 0;
		}
	}
	if (iov[1].iov_len > 0) {
		iov[0].iov_base = "branch ";
		iov[0].iov_len = 7;
		iov[2].iov_base = ";\n";
		iov[2].iov_len = 2;
		iovlen = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;

		if ((wn = writev(fd, iov, 3)) == -1) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
		if ((size_t)wn != iovlen) {
			logmsg_err("writev error");
			return (false);
		}
	}

	/* access */
	c = 0;
	if (write(fd, "access", 6) != 6) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	iov[0].iov_base = "\n\t";
	iov[0].iov_len = 2;
	while (cmd[0] == UPDATER_UPDATE_RCS_ACCESS) {
		if ((len < 3) || (len != (size_t)cmd[2] + 3))
			return (false);
		if ((i2->i_len = cmd[2]) == 0)
			return (false);
		i2->i_id = (char *)&cmd[3];

		switch (cmd[1]) {
		case UPDATER_UPDATE_ADD:
			if (c < rcs->access.ra_count) {
				do {
					i1 = &rcs->access.ra_id[c];
					rv = rcslib_cmp_id(i1, i2);
					if (rv == 0)
						return (false);
					if (rv > 0)
						break;
					iov[1].iov_base = i1->i_id;
					iov[1].iov_len = i1->i_len;
					wn = (ssize_t)(i1->i_len + 2);
					if (writev(fd, iov, 2) != wn) {
						logmsg_err("%s",
							   strerror(errno));
						return (false);
					}
				} while (++c < rcs->access.ra_count);
			} else {
				if (rcs->access.ra_count > 0) {
					i1 = &rcs->access.ra_id[c - 1];
					if (rcslib_cmp_id(i1, i2) >= 0)
						return (false);
				}
			}
			iov[1].iov_base = i2->i_id;
			iov[1].iov_len = i2->i_len;
			if ((wn = writev(fd, iov, 2)) == -1) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
			if ((size_t)wn != i2->i_len + 2) {
				logmsg_err("writev error");
				return (false);
			}
			break;
		case UPDATER_UPDATE_REMOVE:
			if (c == rcs->access.ra_count)
				return (false);
			do {
				i1 = &rcs->access.ra_id[c];
				if ((rv = rcslib_cmp_id(i1, i2)) > 0)
					return (false);
				if (rv == 0) {
					c++;
					break;
				}
				iov[1].iov_base = i1->i_id;
				iov[1].iov_len = i1->i_len;
				if ((wn = writev(fd, iov, 2)) == -1) {
					logmsg_err("%s", strerror(errno));
					return (false);
				}
				if ((size_t)wn != i1->i_len + 2) {
					logmsg_err("writev error");
					return (false);
				}
			} while (++c < rcs->access.ra_count);
			break;
		default:
			return (false);
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	}
	while (c < rcs->access.ra_count) {
		i1 = &rcs->access.ra_id[c++];
		iov[1].iov_base = i1->i_id;
		iov[1].iov_len = i1->i_len;
		if ((wn = writev(fd, iov, 2)) == -1) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
		if ((size_t)wn != i1->i_len + 2) {
			logmsg_err("writev error");
			return (false);
		}
	}

	if (write(fd, ";\nsymbols", 9) != 9) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	/* symbols */
	iov[0].iov_base = "\n\t";
	iov[0].iov_len = 2;
	iov[2].iov_base = ":";
	iov[2].iov_len = 1;
	while (cmd[0] == UPDATER_UPDATE_RCS_SYMBOLS) {
		if (len < 6)
			return (false);
		if ((cmd[2] == 0) || (cmd[3] == 0))
			return (false);
		if (len != (size_t)cmd[2] + (size_t)cmd[3] + 4)
			return (false);
		s2->s_sym = (char *)&cmd[4];
		s2->s_len = cmd[2];
		if (!rcslib_str2num(&cmd[s2->s_len + 4], cmd[3], n2))
			return (false);

		switch (cmd[1]) {
		case UPDATER_UPDATE_ADD:
			if (c < rcs->symbols.rs_count) {
				do {
					sym1 = &rcs->symbols.rs_symbols[c];
					rv = rcslib_cmp_symbol(sym1, sym2);
					if (rv == 0)
						return (false);
					if (rv > 0)
						break;
					n1 = &sym1->num;
					s1 = &sym1->sym;
					iov[1].iov_base = s1->s_sym;
					iov[1].iov_len = s1->s_len;
					iov[3].iov_base = n1->n_str;
					iov[3].iov_len = n1->n_len;
					wn = (ssize_t)(s1->s_len + n1->n_len + 3);
					if (writev(fd, iov, 4) != wn) {
						logmsg_err("%s",
							   strerror(errno));
						return (false);
					}
				} while (++c < rcs->symbols.rs_count);
			} else {
				if (rcs->symbols.rs_count > 0) {
					sym1 = &rcs->symbols.rs_symbols[c - 1];
					if (rcslib_cmp_symbol(sym1, sym2) >= 0)
						return (false);
				}
			}
			iov[1].iov_base = s2->s_sym;
			iov[1].iov_len = s2->s_len;
			iov[3].iov_base = n2->n_str;
			iov[3].iov_len = n2->n_len;
			wn = (ssize_t)(s2->s_len + n2->n_len + 3);
			if (writev(fd, iov, 4) != wn) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
			break;
		case UPDATER_UPDATE_REMOVE:
			if (c == rcs->symbols.rs_count)
				return (false);
			do {
				sym1 = &rcs->symbols.rs_symbols[c];
				if ((rv = rcslib_cmp_symbol(sym1, sym2)) > 0)
					return (false);
				if (rv == 0) {
					c++;
					break;
				}
				n1 = &sym1->num;
				s1 = &sym1->sym;
				iov[1].iov_base = s1->s_sym;
				iov[1].iov_len = s1->s_len;
				iov[3].iov_base = n1->n_str;
				iov[3].iov_len = n1->n_len;
				wn = (ssize_t)(s1->s_len + n1->n_len + 3);
				if (writev(fd, iov, 4) != wn) {
					logmsg_err("%s", strerror(errno));
					return (false);
				}
			} while (++c < rcs->symbols.rs_count);
			break;
		default:
			return (false);
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	}
	while (c < rcs->symbols.rs_count) {
		sym1 = &rcs->symbols.rs_symbols[c++];
		n1 = &sym1->num;
		s1 = &sym1->sym;
		iov[1].iov_base = s1->s_sym;
		iov[1].iov_len = s1->s_len;
		iov[3].iov_base = n1->n_str;
		iov[3].iov_len = n1->n_len;
		wn = (ssize_t)(s1->s_len + n1->n_len + 3);
		if (writev(fd, iov, 4) != wn) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
	}

	if (write(fd, ";\nlocks", 7) != 7) {
		logmsg_err("%s", strerror(errno));
		return(false);
	}

	/* locks */
	if (cmd[0] == UPDATER_UPDATE_RCS_LOCKS_STRICT) {
		if (len != 2)
			return (false);

		switch (cmd[1]) {
		case UPDATER_UPDATE_ADD:
			rcs->locks.rl_strict = 1;
			break;
		case UPDATER_UPDATE_REMOVE:
			rcs->locks.rl_strict = 0;
			break;
		default:
			return (false);
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	}
	n2 = &lock2->num;
	c = 0;
	iov[0].iov_base = "\n\t";
	iov[0].iov_len = 2;
	iov[2].iov_base = ":";
	iov[2].iov_len = 1;
	while (cmd[0] == UPDATER_UPDATE_RCS_LOCKS) {
		if (len < 6)
			return (false);
		if ((cmd[2] == 0) || (cmd[3] == 0))
			return (false);
		if (len != (size_t)cmd[2] + (size_t)cmd[3] + 4)
			return (false);
		i2->i_id = (char *)&cmd[4];
		i2->i_len = cmd[2];
		if (!rcslib_str2num(&cmd[i2->i_len + 4], cmd[3], n2))
			return (false);
		switch (cmd[1]) {
		case UPDATER_UPDATE_ADD:
			if (c < rcs->locks.rl_count) {
				do {
					lock1 = &rcs->locks.rl_locks[c];
					rv = rcslib_cmp_lock(lock1, lock2);
					if (rv == 0)
						return (false);
					if (rv > 0)
						break;
					i1 = &lock1->id;
					n1 = &lock1->num;
					iov[1].iov_base = i1->i_id;
					iov[1].iov_len = i1->i_len;
					iov[3].iov_base = n1->n_str;
					iov[3].iov_len = n1->n_len;
					wn = (ssize_t)(i1->i_len + n1->n_len + 3);
					if (writev(fd, iov, 4) != wn) {
						logmsg_err("%s",
							   strerror(errno));
						return (false);
					}
				} while (++c < rcs->locks.rl_count);
			} else {
				if (rcs->locks.rl_count > 0) {
					lock1 = &rcs->locks.rl_locks[c - 1];
					if (rcslib_cmp_lock(lock1, lock2) >= 0)
						return (false);
				}
			}
			iov[1].iov_base = i2->i_id;
			iov[1].iov_len = i2->i_len;
			iov[3].iov_base = n2->n_str;
			iov[3].iov_len = n2->n_len;
			wn = (ssize_t)(i2->i_len + n2->n_len + 3);
			if (writev(fd, iov, 4) != wn) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
			break;
		case UPDATER_UPDATE_REMOVE:
			if (c == rcs->locks.rl_count)
				return (false);
			do {
				lock1 = &rcs->locks.rl_locks[c];
				if ((rv = rcslib_cmp_lock(lock1, lock2)) > 0)
					return (false);
				if (rv == 0) {
					c++;
					break;
				}
				i1 = &lock1->id;
				n1 = &lock1->num;
				iov[1].iov_base = i1->i_id;
				iov[1].iov_len = i1->i_len;
				iov[3].iov_base = n1->n_str;
				iov[3].iov_len = n1->n_len;
				wn = (ssize_t)(i1->i_len + n1->n_len + 3);
				if (writev(fd, iov, 4) != wn) {
					logmsg_err("%s", strerror(errno));
					return (false);
				}
			} while (++c < rcs->locks.rl_count);
			break;
		default:
			return (false);
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	}
	while (c < rcs->locks.rl_count) {
		lock1 = &rcs->locks.rl_locks[c++];
		i1 = &lock1->id;
		n1 = &lock1->num;
		iov[1].iov_base = i1->i_id;
		iov[1].iov_len = i1->i_len;
		iov[3].iov_base = n1->n_str;
		iov[3].iov_len = n1->n_len;
		wn = (ssize_t)(i1->i_len + n1->n_len + 3);
		if (writev(fd, iov, 4) != wn) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
	}
	if (rcs->locks.rl_strict != 0) {
		if (write(fd, "; strict;\n", 10) != 10) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
	} else {
		if (write(fd, ";\n", 2) != 2) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
	}

	/* comment */
	iov[0].iov_base = "comment\t@";
	iov[0].iov_len = 9;
	iov[2].iov_base = "@;\n";
	iov[2].iov_len = 3;
	if (cmd[0] == UPDATER_UPDATE_RCS_COMMENT) {
		if ((len < 2) || (len != (size_t)cmd[1] + 2))
			return(false);
		if (cmd[1] > 0) {
			iov[1].iov_base = (void *)&cmd[2];
			iov[1].iov_len = cmd[1];
			if (writev(fd, iov, 3) != cmd[1] + 12) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	} else {
		if (rcs->comment.s_len > 0) {
			iov[1].iov_base = rcs->comment.s_str;
			iov[1].iov_len = rcs->comment.s_len;
			if ((wn = writev(fd, iov, 3)) == -1) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
			if ((size_t)wn != rcs->comment.s_len + 12) {
				logmsg_err("writev error");
				return (false);
			}
		}
	}

	/* expand */
	iov[0].iov_base = "expand @";
	iov[0].iov_len = 8;
	iov[2].iov_base = "@;\n";
	iov[2].iov_len = 3;
	if (cmd[0] == UPDATER_UPDATE_RCS_EXPAND) {
		if ((len < 2) || (len != (size_t)cmd[1] + 2))
			return(false);
		if (cmd[1] > 0) {
			iov[1].iov_base = (void *)&cmd[2];
			iov[1].iov_len = cmd[1];
			if (writev(fd, iov, 3) != cmd[1] + 11) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
		}

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);
	} else {
		if (rcs->expand.s_len > 0) {
			iov[1].iov_base = rcs->expand.s_str;
			iov[1].iov_len = rcs->expand.s_len;
			if ((wn = writev(fd, iov, 3)) == -1) {
				logmsg_err("%s", strerror(errno));
				return (false);
			}
			if ((size_t)wn != rcs->expand.s_len + 11) {
				logmsg_err("writev error");
				return (false);
			}
		}
	}

	if (len != 1)
		return (false);
	if (cmd[0] != UPDATER_UPDATE_END)
		return (false);

	return (true);
}

bool
updater_rcs_delta(struct updater_args *uda, struct rcslib_file *rcs)
{
	struct rcslib_revision *rev;
	struct rcsnum *n1, *n2, t_num;
	uint8_t *cmd = uda->uda_cmd;
	size_t len = 0, c = 0;
	int fd = uda->uda_fileno, rv;

	if (write(fd, "\n\n", 2) != 2) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}

	n2 = &t_num;

	for (;;) {
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);

		if (cmd[0] == UPDATER_UPDATE_END)
			break;

		if (len < 3)
			return (false);
		if (cmd[0] != UPDATER_UPDATE_RCS_DELTA)
			return (false);

		switch (cmd[1]) {
		case UPDATER_UPDATE_ADD:
			if ((cmd[2] == 0) || (len < (size_t)cmd[2] + 3))
				return (false);
			if (!rcslib_str2num(&cmd[3], cmd[2], n2))
				return (false);

			if (c < rcs->delta.rd_count) {
				do {
					rev = &rcs->delta.rd_rev[c];
					n1 = &rev->num;

					if ((rv = rcslib_cmp_num(n1, n2)) == 0)
						return (false);
					if (rv > 0)
						break;
					if (!rcslib_write_delta(fd, rev))
						return (false);
				} while (++c < rcs->delta.rd_count);
			} else {
				if (rcs->delta.rd_count > 0) {
					rev = &rcs->delta.rd_rev[c - 1];
					if (rcslib_cmp_num(&rev->num, n2) >= 0)
						return (false);
				}
			}
			if (!updater_rcs_write_delta(uda, cmd, len))
				return (false);
			break;
		case UPDATER_UPDATE_REMOVE:
			if ((cmd[2] == 0) || (len != (size_t)cmd[2] + 3))
				return (false);
			if (!rcslib_str2num(&cmd[3], cmd[2], n2))
				return (false);

			if (c == rcs->delta.rd_count)
				return (false);

			do {
				rev = &rcs->delta.rd_rev[c];
				n1 = &rev->num;
				if ((rv = rcslib_cmp_num(n1, n2)) > 0)
					return (false);
				if (rv == 0) {
					c++;
					break;
				}
				if (!rcslib_write_delta(fd, rev))
					return (false);
			} while (++c < rcs->delta.rd_count);
			break;
		case UPDATER_UPDATE_UPDATE:
			if ((cmd[2] == 0) || (len < (size_t)cmd[2] + 3))
				return (false);
			if (!rcslib_str2num(&cmd[3], cmd[2], n2))
				return (false);

			if (c == rcs->delta.rd_count)
				return (false);

			do {
				rev = &rcs->delta.rd_rev[c++];
				n1 = &rev->num;
				if ((rv = rcslib_cmp_num(n1, n2)) > 0)
					return (false);
				if (rv < 0) {
					if (!rcslib_write_delta(fd, rev))
						return (false);
					continue;
				}
				if (!updater_rcs_write_delta(uda, cmd, len))
					return (false);
				break;
			} while (c < rcs->delta.rd_count);
			break;
		default:
			return (false);
		}
	}
	while (c < rcs->delta.rd_count) {
		rev = &rcs->delta.rd_rev[c++];
		if (!rcslib_write_delta(fd, rev))
			return (false);
	}

	if (len != 1)
		return (false);

	return (true);
}

bool
updater_rcs_write_delta(struct updater_args *uda, uint8_t *sp, size_t len)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	struct iovec iov[13];
	size_t slen, n, i;
	ssize_t wn;

	if (len < 2)
		return (false);
	sp += 2;
	len -= 2;

	if (!(*hashops->init)(&uda->uda_hash_ctx))
		return (false);

	if ((slen = *sp++) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if (len < slen + 1) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	iov[0].iov_base = (void *)sp;
	iov[0].iov_len = slen;
	iov[1].iov_base = "\n";
	iov[1].iov_len = 1;
	wn = (ssize_t)(slen + 1);
	sp += slen;

	/* date */
	if ((len -= slen + 1) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if ((slen = *sp++) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if (len < slen + 1) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	iov[2].iov_base = "date\t";
	iov[2].iov_len = 5;
	iov[3].iov_base = (void *)sp;
	iov[3].iov_len = slen;
	iov[4].iov_base = ";";
	iov[4].iov_len = 1;
	wn += slen + 6;
	(*hashops->update)(uda->uda_hash_ctx, sp, slen);
	sp += slen;

	/* author */
	if ((len -= slen + 1) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if ((slen = *sp++) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if (len < slen + 1) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	iov[5].iov_base = "\tauthor ";
	iov[5].iov_len = 8;
	iov[6].iov_base = (void *)sp;
	iov[6].iov_len = slen;
	iov[7].iov_base = ";";
	iov[7].iov_len = 1;
	wn += slen + 9;
	(*hashops->update)(uda->uda_hash_ctx, sp, slen);
	sp += slen;

	/* state */
	if ((len -= slen + 1) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	slen = *sp++;
	if (len < slen + 1) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	iov[8].iov_base = "\tstate ";
	if (slen > 0)
		iov[8].iov_len = 7;
	else
		iov[8].iov_len = 6;
	iov[9].iov_base = (void *)sp;
	iov[9].iov_len = slen;
	if (slen > 0) {
		(*hashops->update)(uda->uda_hash_ctx, sp, slen);
		sp += slen;
	}
	iov[10].iov_base = ";\n";
	iov[10].iov_len = 2;
	wn += slen + iov[8].iov_len + 2;

	if ((len -= slen + 1) < 2) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	n = GetWord(sp);
	sp += 2;
	if ((len -= 2) == 0) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	iov[11].iov_base = "branches";
	iov[11].iov_len = 8;

	if (writev(uda->uda_fileno, iov, 12) != wn + 8) {
		logmsg_err("%s", strerror(errno));
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}

	iov[0].iov_base = "\n\t";
	iov[0].iov_len = 2;
	for (i = 0 ; i < n ; i++) {
		slen = *sp++;
		if (len < slen + 1) {
			(*hashops->destroy)(uda->uda_hash_ctx);
			return (false);
		}
		iov[1].iov_base = (void *)sp;
		iov[1].iov_len = slen;
		if ((wn = writev(uda->uda_fileno, iov, 2)) == -1) {
			logmsg_err("%s", strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			return (false);
		}
		if ((size_t)wn != slen + 2) {
			logmsg_err("writev error");
			(*hashops->destroy)(uda->uda_hash_ctx);
			return (false);
		}
		(*hashops->update)(uda->uda_hash_ctx, sp, slen);
		sp += slen;
		if ((len -= slen + 1) == 0) {
			(*hashops->destroy)(uda->uda_hash_ctx);
			return (false);
		}
	}

	iov[0].iov_base = ";\nnext\t";
	iov[0].iov_len = 7;

	/* next */
	slen = *sp++;
	if (len < slen + 1) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	iov[1].iov_base = (void *)sp;
	iov[1].iov_len = slen;
	if (slen > 0) {
		(*hashops->update)(uda->uda_hash_ctx, sp, slen);
		sp += slen;
	}
	len -= slen + 1;
	iov[2].iov_base = ";\n\n";
	iov[2].iov_len = 3;

	if ((wn = writev(uda->uda_fileno, iov, 3)) == -1) {
		logmsg_err("%s", strerror(errno));
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if ((size_t)wn != slen + 10) {
		logmsg_err("writev error");
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}

	if (len != hashops->length) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}

	(*hashops->final)(uda->uda_hash_ctx, uda->uda_hash);

	if (memcmp(uda->uda_hash, sp, hashops->length) != 0)
		return (false);

	return (true);
}

bool
updater_rcs_desc(struct updater_args *uda)
{
	struct iovec iov[3];
	uint8_t *cmd = uda->uda_cmd;
	size_t len;
	ssize_t wn;

	iov[0].iov_base = "\n\ndesc\n@";
	iov[0].iov_len = 8;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > uda->uda_cmdmax - 2))
		return (false);
	if (cmd[2] != UPDATER_UPDATE_RCS_DESC)
		return (false);
	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len - 1))
		return (false);
	if ((size_t)cmd[0] != len - 2)
		return (false);
	iov[1].iov_base = (void *)&cmd[1];
	iov[1].iov_len = len - 2;
	iov[2].iov_base = "@\n";
	iov[2].iov_len = 2;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;

	if ((wn = writev(uda->uda_fileno, iov, 3)) == -1) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	if ((size_t)wn != len) {
		logmsg_err("writev error");
		return (false);
	}

	return (true);
}

bool
updater_rcs_deltatext(struct updater_args *uda, struct rcslib_file *rcs)
{
	struct rcslib_revision *rev;
	struct rcsnum *n1, *n2, t_num;
	uint8_t *cmd = uda->uda_cmd;
	size_t len, c = 0;
	int fd = uda->uda_fileno, rv;

	n2 = &t_num;

	for (;;) {
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len == 0) || (len > uda->uda_cmdmax - 2))
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, len))
			return (false);

		if (cmd[0] == UPDATER_UPDATE_END)
			break;

		if (len < 3)
			return (false);
		if (cmd[0] != UPDATER_UPDATE_RCS_DELTATEXT)
			return (false);
		if ((cmd[2] == 0) || (len != (size_t)cmd[2] + 3))
			return (false);
		if (!rcslib_str2num(&cmd[3], cmd[2], n2))
			return (false);

		switch (cmd[1]) {
		case UPDATER_UPDATE_ADD:
			if (c < rcs->delta.rd_count) {
				do {
					rev = &rcs->delta.rd_rev[c];
					n1 = &rev->num;

					if ((rv = rcslib_cmp_num(n1, n2)) == 0)
						return (false);
					if (rv > 0)
						break;
					if (!rcslib_write_deltatext(fd, rev))
						return (false);
				} while (++c < rcs->delta.rd_count);
			} else {
				if (rcs->delta.rd_count > 0) {
					rev = &rcs->delta.rd_rev[c - 1];
					if (rcslib_cmp_num(&rev->num, n2) >= 0)
						return (false);
				}
			}
			if (!updater_rcs_write_deltatext(uda, n2))
				return (false);
			break;
		case UPDATER_UPDATE_REMOVE:
			if (c == rcs->delta.rd_count)
				return (false);

			do {
				rev = &rcs->delta.rd_rev[c];
				n1 = &rev->num;
				if ((rv = rcslib_cmp_num(n1, n2)) > 0)
					return (false);
				if (rv == 0) {
					c++;
					break;
				}
				if (!rcslib_write_deltatext(fd, rev))
					return (false);
			} while (++c < rcs->delta.rd_count);
			break;
		case UPDATER_UPDATE_UPDATE:
			if (c == rcs->delta.rd_count)
				return (false);

			do {
				rev = &rcs->delta.rd_rev[c++];
				n1 = &rev->num;
				if ((rv = rcslib_cmp_num(n1, n2)) > 0)
					return (false);
				if (rv < 0) {
					if (!rcslib_write_deltatext(fd, rev))
						return (false);
					continue;
				}
				if (!updater_rcs_write_deltatext(uda, n2))
					return (false);
				break;
			} while (c < rcs->delta.rd_count);
			break;
		default:
			return (false);
		}
	}
	while (c < rcs->delta.rd_count) {
		rev = &rcs->delta.rd_rev[c++];
		if (!rcslib_write_deltatext(fd, rev))
			return (false);
	}

	if (len != 1)
		return (false);

	return (true);
}

bool
updater_rcs_write_deltatext(struct updater_args *uda, struct rcsnum *num)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	struct iovec iov[3];
	uint64_t textlen;
	uint32_t loglen;
	uint8_t *cmd = uda->uda_cmd;
	size_t len;
	ssize_t wn;
	int fd = uda->uda_fileno;

	if (!(*hashops->init)(&uda->uda_hash_ctx))
		return (false);

	iov[0].iov_base = "\n\n";
	iov[0].iov_len = 2;
	iov[1].iov_base = num->n_str;
	iov[1].iov_len = num->n_len;
	iov[2].iov_base = "\nlog\n@";
	iov[2].iov_len = 6;
	if ((wn = writev(fd, iov, 3)) == -1) {
		logmsg_err("%s", strerror(errno));
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if ((size_t)wn != num->n_len + 8) {
		logmsg_err("writev error");
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 4)) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if ((loglen = GetDWord(cmd)) != 0) {
		while (loglen > 0) {
			if (loglen > uda->uda_bufsize)
				len = uda->uda_bufsize;
			else
				len = (size_t)loglen;
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      uda->uda_buffer, len)) {
				(*hashops->destroy)(uda->uda_hash_ctx);
				return (false);
			}
			if ((wn = write(fd, uda->uda_buffer, len)) == -1) {
				logmsg_err("%s", strerror(errno));
				(*hashops->destroy)(uda->uda_hash_ctx);
				return (false);
			}
			if ((size_t)wn != len) {
				logmsg_err("writev error");
				(*hashops->destroy)(uda->uda_hash_ctx);
				return (false);
			}
			(*hashops->update)(uda->uda_hash_ctx,
					   uda->uda_buffer, len);
			loglen -= (uint32_t)len;
		}
	}

	if (write(fd, "@\ntext\n@", 8) != 8) {
		logmsg_err("%s", strerror(errno));
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 8)) {
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}
	if ((textlen = GetDDWord(cmd)) != 0) {
		while (textlen > 0) {
			if (textlen > uda->uda_bufsize)
				len = uda->uda_bufsize;
			else
				len = (size_t)textlen;
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
				      uda->uda_buffer, len)) {
				(*hashops->destroy)(uda->uda_hash_ctx);
				return (false);
			}
			if ((wn = write(fd, uda->uda_buffer, len)) == -1) {
				logmsg_err("%s", strerror(errno));
				(*hashops->destroy)(uda->uda_hash_ctx);
				return (false);
			}
			if ((size_t)wn != len) {
				logmsg_err("writev error");
				(*hashops->destroy)(uda->uda_hash_ctx);
				return (false);
			}
			(*hashops->update)(uda->uda_hash_ctx,
					   uda->uda_buffer, len);
			textlen -= (uint64_t)len;
		}
	}

	if (write(fd, "@\n", 2) != 2) {
		logmsg_err("%s", strerror(errno));
		(*hashops->destroy)(uda->uda_hash_ctx);
		return (false);
	}

	(*hashops->final)(uda->uda_hash_ctx, uda->uda_hash);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, hashops->length))
		return (false);
	if (memcmp(uda->uda_hash, cmd, hashops->length) != 0)
		return (false);

	return (true);
}
