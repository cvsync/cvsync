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

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

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
#include "distfile.h"
#include "filetypes.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "rcslib.h"
#include "version.h"

#include "filecmp.h"
#include "updater.h"

bool filecmp_rcs_fetch(struct filecmp_args *);

bool filecmp_rcs_add(struct filecmp_args *);
bool filecmp_rcs_add_dir(struct filecmp_args *);
bool filecmp_rcs_add_file(struct filecmp_args *);
bool filecmp_rcs_add_symlink(struct filecmp_args *);
bool filecmp_rcs_remove(struct filecmp_args *);
bool filecmp_rcs_attic(struct filecmp_args *);
bool filecmp_rcs_setattr(struct filecmp_args *);
bool filecmp_rcs_update(struct filecmp_args *);
bool filecmp_rcs_update_rcs(struct filecmp_args *, struct cvsync_file *);
bool filecmp_rcs_update_symlink(struct filecmp_args *);

bool filecmp_rcs_admin(struct filecmp_args *, struct rcslib_file *);
bool filecmp_rcs_admin_access(struct filecmp_args *, struct rcslib_file *,
			      size_t);
bool filecmp_rcs_admin_symbols(struct filecmp_args *, struct rcslib_file *,
			       size_t);
bool filecmp_rcs_admin_locks(struct filecmp_args *, struct rcslib_file *,
			     size_t);
bool filecmp_rcs_delta(struct filecmp_args *, struct rcslib_file *, uint32_t *);
bool filecmp_rcs_delta_add(struct filecmp_args *, struct rcslib_revision *);
bool filecmp_rcs_delta_remove(struct filecmp_args *, struct rcsnum *);
bool filecmp_rcs_delta_update(struct filecmp_args *, struct rcslib_revision *,
			      uint8_t *);
bool filecmp_rcs_desc(struct filecmp_args *, struct rcslib_file *);
bool filecmp_rcs_deltatext(struct filecmp_args *, struct rcslib_file *,
			   uint32_t);
bool filecmp_rcs_deltatext_add(struct filecmp_args *, struct rcslib_revision *);
bool filecmp_rcs_deltatext_remove(struct filecmp_args *, struct rcsnum *);
bool filecmp_rcs_deltatext_update(struct filecmp_args *,
				  struct rcslib_revision *, uint8_t *);

bool filecmp_rcs_ignore_rcs(struct filecmp_args *);
bool filecmp_rcs_ignore_rcs_delta(struct filecmp_args *, uint32_t *);
bool filecmp_rcs_ignore_rcs_deltatext(struct filecmp_args *, uint32_t);
bool filecmp_rcs_ignore_rcs_list(struct filecmp_args *, uint8_t);
bool filecmp_rcs_ignore_rcs_string(struct filecmp_args *, uint8_t);

static const uint8_t _cmde[3] = { 0x00, 0x01, UPDATER_UPDATE_END };

bool
filecmp_rcs(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;

	for (;;) {
		if (!filecmp_rcs_fetch(fca)) {
			logmsg_err("%s FileCmp(RCS): Fetch Error",
				   fca->fca_hostinfo);
			return (false);
		}

		if (cap->ca_tag == FILECMP_END)
			break;

		switch (cap->ca_tag) {
		case FILECMP_ADD:
			if (!filecmp_rcs_add(fca)) {
				logmsg_err("%s FileCmp(RCS): %s: ADD Error",
					   fca->fca_hostinfo, fca->fca_path);
				return (false);
			}
			break;
		case FILECMP_REMOVE:
			if (!filecmp_rcs_remove(fca)) {
				logmsg_err("%s FileCmp(RCS): %s: REMOVE Error",
					   fca->fca_hostinfo, fca->fca_path);
				return (false);
			}
			break;
		case FILECMP_RCS_ATTIC:
			if (!filecmp_rcs_attic(fca)) {
				logmsg_err("%s FileCmp(RCS): %s: ATTIC Error",
					   fca->fca_hostinfo, fca->fca_path);
				return (false);
			}
			break;
		case FILECMP_SETATTR:
			if (!filecmp_rcs_setattr(fca)) {
				logmsg_err("%s FileCmp(RCS): %s: SETATTR Error",
					   fca->fca_hostinfo, fca->fca_path);
				return (false);
			}
			break;
		case FILECMP_UPDATE:
			if (!filecmp_rcs_update(fca)) {
				logmsg_err("%s FileCmp(RCS): %s: UPDATE Error",
					   fca->fca_hostinfo, fca->fca_path);
				return (false);
			}
			break;
		default:
			logmsg_err("%s FileCmp(RCS): %02x: Unknown Command",
				   cap->ca_tag);
			return (false);
		}
	}

	return (true);
}

bool
filecmp_rcs_fetch(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;
	uint8_t *cmd = fca->fca_cmd;
	size_t pathlen, len;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > fca->fca_cmdmax - 2))
		return (false);
	if ((cap->ca_tag = cmd[2]) == FILECMP_END)
		return (len == 1);
	if (len < 2)
		return (false);

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);

	cap->ca_type = cmd[0];
	cap->ca_namelen = GetWord(&cmd[1]);
	if (cap->ca_namelen > sizeof(cap->ca_name))
		return (false);

	switch (cap->ca_tag) {
	case FILECMP_ADD:
	case FILECMP_REMOVE:
		if (cap->ca_namelen != len - 4)
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		break;
	case FILECMP_RCS_ATTIC:
		if ((cap->ca_type != FILETYPE_RCS) &&
		    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
			return (false);
		}
		if (cap->ca_namelen != len - RCS_ATTRLEN_RCS - 4)
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!IS_FILE_RCS(cap->ca_name, cap->ca_namelen))
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
			      RCS_ATTRLEN_RCS)) {
			return (false);
		}
		if (!attr_rcs_decode_rcs(cmd, RCS_ATTRLEN_RCS, cap))
			return (false);
		break;
	case FILECMP_UPDATE:
		if (cap->ca_type == FILETYPE_DIR)
			return (false);
		if (cap->ca_type == FILETYPE_SYMLINK) {
			if (len != cap->ca_namelen + 4)
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			break;
		}
		/* FALLTHROUGH */
	case FILECMP_SETATTR:
		switch (cap->ca_type) {
		case FILETYPE_DIR:
			if (cap->ca_namelen != len - RCS_ATTRLEN_DIR - 4)
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
				      RCS_ATTRLEN_DIR)) {
				return (false);
			}
			if (!attr_rcs_decode_dir(cmd, RCS_ATTRLEN_DIR, cap))
				return (false);
			break;
		case FILETYPE_FILE:
			if (cap->ca_namelen != len - RCS_ATTRLEN_FILE - 4)
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
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
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!IS_FILE_RCS(cap->ca_name, cap->ca_namelen))
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
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

	if ((pathlen = fca->fca_pathlen + cap->ca_namelen) >= fca->fca_pathmax)
		return (false);

	(void)memcpy(fca->fca_rpath, cap->ca_name, cap->ca_namelen);
	fca->fca_rpath[cap->ca_namelen] = '\0';
	if (cap->ca_type == FILETYPE_RCS_ATTIC) {
		if (!cvsync_rcs_insert_attic(fca->fca_path, pathlen,
					     fca->fca_pathmax)) {
			return (false);
		}
	}

	return (true);
}

bool
filecmp_rcs_add(struct filecmp_args *fca)
{
	switch (fca->fca_attr.ca_type) {
	case FILETYPE_DIR:
		return (filecmp_rcs_add_dir(fca));
	case FILETYPE_FILE:
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		return (filecmp_rcs_add_file(fca));
	case FILETYPE_SYMLINK:
		return (filecmp_rcs_add_symlink(fca));
	default:
		break;
	}

	return (false);
}

bool
filecmp_rcs_add_dir(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if (stat(fca->fca_path, &st) == -1) {
		if (errno != ENOENT)
			return (false);
		return (true);
	}
	if (!S_ISDIR(st.st_mode))
		return (false);

	if ((len = cap->ca_namelen + 6) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_ADD;
	cmd[3] = FILETYPE_DIR;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	mode = RCS_MODE(st.st_mode, fca->fca_umask);

	if ((len = attr_rcs_encode_dir(&cmd[2], fca->fca_cmdmax - 2,
				       mode)) == 0) {
		return (false);
	}
	SetWord(cmd, len);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, len + 2))
		return (false);

	return (true);
}

bool
filecmp_rcs_add_file(struct filecmp_args *fca)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct cvsync_attr *cap = &fca->fca_attr;
	struct cvsync_file *cfp;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fca->fca_cmd;
	off_t size = 0;
	size_t len;

	if ((len = cap->ca_namelen + 6) > fca->fca_cmdmax)
		return (false);

	if ((cfp = cvsync_fopen(fca->fca_path)) == NULL) {
		if (stat(fca->fca_path, &st) == -1) {
			if (errno == ENOENT)
				return (true);
		}
		return (false);
	}
	mode = RCS_MODE(cfp->cf_mode, fca->fca_umask);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_ADD;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		cvsync_fclose(cfp);
		return (false);
	}

	if ((len = attr_rcs_encode_file(&cmd[2], fca->fca_cmdmax - 2,
					cfp->cf_mtime, cfp->cf_size,
					mode)) == 0) {
		cvsync_fclose(cfp);
		return (false);
	}
	SetWord(cmd, len);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, len + 2)) {
		cvsync_fclose(cfp);
		return (false);
	}

	if (!(*hashops->init)(&fca->fca_hash_ctx)) {
		cvsync_fclose(cfp);
		return (false);
	}

	while (size < cfp->cf_size) {
		if ((cfp->cf_size - size) < CVSYNC_BSIZE)
			len = (size_t)(cfp->cf_size - size);
		else
			len = CVSYNC_BSIZE;

		if (!cvsync_mmap(cfp, size, (off_t)len)) {
			(*hashops->destroy)(fca->fca_hash_ctx);
			cvsync_fclose(cfp);
			return (false);
		}

		if (!mux_send(fca->fca_mux, MUX_UPDATER, cfp->cf_addr, len)) {
			(*hashops->destroy)(fca->fca_hash_ctx);
			cvsync_fclose(cfp);
			return (false);
		}

		(*hashops->update)(fca->fca_hash_ctx, cfp->cf_addr, len);

		if (!cvsync_munmap(cfp)) {
			(*hashops->destroy)(fca->fca_hash_ctx);
			cvsync_fclose(cfp);
			return (false);
		}

		size += (off_t)len;
	}
	(*hashops->final)(fca->fca_hash_ctx, cmd);

	if (!cvsync_fclose(cfp))
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, hashops->length))
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filecmp_rcs_add_symlink(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;
	int wn;

	if ((len = cap->ca_namelen + 6) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_ADD;
	cmd[3] = FILETYPE_SYMLINK;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	if ((wn = readlink(fca->fca_path, (char *)&cmd[2],
			   fca->fca_cmdmax - 2)) == -1) {
		if (errno == ENOENT)
			return (true);
		return (false);
	}
	SetWord(cmd, wn);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, (size_t)(wn + 2)))
		return (false);

	return (true);
}

bool
filecmp_rcs_remove(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if ((len = cap->ca_namelen + 6) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_REMOVE;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	return (true);
}

bool
filecmp_rcs_attic(struct filecmp_args *fca)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct cvsync_attr *cap = &fca->fca_attr;
	struct cvsync_file *cfp;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fca->fca_cmd, tag;
	size_t base, len;

	if ((cfp = cvsync_fopen(fca->fca_path)) == NULL) {
		switch (cap->ca_type) {
		case FILETYPE_RCS:
			len = fca->fca_pathlen + cap->ca_namelen;
			cap->ca_type = FILETYPE_RCS_ATTIC;
			if (!cvsync_rcs_insert_attic(fca->fca_path, len,
						     fca->fca_pathmax)) {
				return (false);
			}
			break;
		case FILETYPE_RCS_ATTIC:
			len = fca->fca_pathlen + cap->ca_namelen + 6;
			cap->ca_type = FILETYPE_RCS;
			if (!cvsync_rcs_remove_attic(fca->fca_path, len))
				return (false);
			break;
		default:
			return (false);
		}

		if (stat(fca->fca_path, &st) == -1)
			return (false);

		return (filecmp_rcs_update(fca));
	}
	if (!cvsync_mmap(cfp, 0, cfp->cf_size)) {
		cvsync_fclose(cfp);
		return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (GetWord(cmd) != 1) {
		cvsync_fclose(cfp);
		return (false);
	}
	tag = cmd[2];

	switch (tag) {
	case FILECMP_UPDATE_GENERIC:
		if (!(*hashops->init)(&fca->fca_hash_ctx)) {
			cvsync_fclose(cfp);
			return (false);
		}
		(*hashops->update)(fca->fca_hash_ctx, cfp->cf_addr,
				   (size_t)cfp->cf_size);
		(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);

		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
			      hashops->length)) {
			cvsync_fclose(cfp);
			return (false);
		}

		if (memcmp(fca->fca_hash, cmd, hashops->length) == 0) {
			if (!cvsync_fclose(cfp))
				return (false);
			if (!filecmp_rcs_setattr(fca))
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
				return (false);
			if (GetWord(cmd) != 1)
				return (false);
			if (cmd[2] != FILECMP_UPDATE_END)
				return (false);
			return (true);
		}

		break;
	case FILECMP_UPDATE_RCS:
	    {
		struct rcslib_file *rcs;

		if ((rcs = rcslib_init(cfp->cf_addr, cfp->cf_size)) == NULL) {
			if (!filecmp_rcs_ignore_rcs(fca)) {
				cvsync_fclose(cfp);
				return (false);
			}
			goto done;
		}
		rcslib_destroy(rcs);
		break;
	    }
	case FILECMP_UPDATE_RDIFF:
		if (filecmp_access(fca, cap) == DISTFILE_NORDIFF) {
			if (filecmp_rdiff_ischanged(fca, cfp)) {
				if (!filecmp_rcs_setattr(fca)) {
					cvsync_fclose(cfp);
					return (false);
				}
				goto done;
			}

			tag = FILECMP_UPDATE_GENERIC;

			if (!(*hashops->init)(&fca->fca_hash_ctx)) {
				cvsync_fclose(cfp);
				return (false);
			}
			(*hashops->update)(fca->fca_hash_ctx, cfp->cf_addr,
					   (size_t)cfp->cf_size);
			(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

	mode = RCS_MODE(cfp->cf_mode, fca->fca_umask);

	if ((base = cap->ca_namelen + 6) > fca->fca_cmdmax) {
		cvsync_fclose(cfp);
		return (false);
	}

	if ((len = attr_rcs_encode_rcs(&cmd[6], fca->fca_cmdmax - base,
				       cfp->cf_mtime, mode)) == 0) {
		cvsync_fclose(cfp);
		return (false);
	}

	SetWord(cmd, len + base - 2);
	cmd[2] = UPDATER_RCS_ATTIC;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, &cmd[6], len)) {
		cvsync_fclose(cfp);
		return (false);
	}

	switch (tag) {
	case FILECMP_UPDATE_GENERIC:
		if (!filecmp_generic_update(fca, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	case FILECMP_UPDATE_RCS:
		if (!filecmp_rcs_update_rcs(fca, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	case FILECMP_UPDATE_RDIFF:
		if (!filecmp_rdiff_update(fca, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

done:
	if (!cvsync_fclose(cfp))
		return (false);

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	return (true);
}

bool
filecmp_rcs_setattr(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fca->fca_cmd;
	size_t base, len;

	if (stat(fca->fca_path, &st) == -1) {
		if ((errno != ENOENT) && (errno != ENOTDIR))
			return (false);
		st.st_mode = 0;
	}
	mode = RCS_MODE(st.st_mode, fca->fca_umask);

	if ((base = cap->ca_namelen + 6) > fca->fca_cmdmax)
		return (false);
	len = fca->fca_cmdmax - base;

	switch (cap->ca_type) {
	case FILETYPE_DIR:
		if (cap->ca_mode == mode)
			return (true);
		if ((len = attr_rcs_encode_dir(&cmd[6], len, mode)) == 0)
			return (false);
		break;
	case FILETYPE_FILE:
		if ((cap->ca_mtime == (int64_t)st.st_mtime) &&
		    (cap->ca_size == (uint64_t)st.st_size) &&
		    (cap->ca_mode == mode)) {
			return (true);
		}
		if ((len = attr_rcs_encode_file(&cmd[6], len, st.st_mtime,
						st.st_size, mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((cap->ca_mtime == (int64_t)st.st_mtime) &&
		    (cap->ca_mode == mode)) {
			return (true);
		}
		if ((len = attr_rcs_encode_rcs(&cmd[6], len, st.st_mtime,
					       mode)) == 0) {
			return (false);
		}
		break;
	default:
		return (false);
	}

	SetWord(cmd, len + base - 2);
	cmd[2] = UPDATER_SETATTR;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, &cmd[6], len))
		return (false);

	return (true);
}

bool
filecmp_rcs_update(struct filecmp_args *fca)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct cvsync_attr *cap = &fca->fca_attr;
	struct cvsync_file *cfp;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fca->fca_cmd, tag;
	size_t base, len;

	if (cap->ca_type == FILETYPE_SYMLINK)
		return (filecmp_rcs_update_symlink(fca));

	if ((cfp = cvsync_fopen(fca->fca_path)) == NULL) {
		switch (cap->ca_type) {
		case FILETYPE_FILE:
			/* Nothing to do. */
			break;
		case FILETYPE_RCS:
			len = fca->fca_pathlen + cap->ca_namelen;
			cap->ca_type = FILETYPE_RCS_ATTIC;
			if (!cvsync_rcs_insert_attic(fca->fca_path, len,
						     fca->fca_pathmax)) {
				return (false);
			}
			if (stat(fca->fca_path, &st) == 0)
				return (filecmp_rcs_attic(fca));
			cap->ca_type = FILETYPE_RCS;
			if (!cvsync_rcs_remove_attic(fca->fca_path, len + 6))
				return (false);
			break;
		case FILETYPE_RCS_ATTIC:
			len = fca->fca_pathlen + cap->ca_namelen + 6;
			cap->ca_type = FILETYPE_RCS;
			if (!cvsync_rcs_remove_attic(fca->fca_path, len))
				return (false);
			if (stat(fca->fca_path, &st) == 0)
				return (filecmp_rcs_attic(fca));
			cap->ca_type = FILETYPE_RCS_ATTIC;
			if (!cvsync_rcs_insert_attic(fca->fca_path, len - 6,
						     fca->fca_pathmax)) {
				return (false);
			}
			break;
		default:
			return (false);
		}

		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
			return (false);
		if (GetWord(cmd) != 1)
			return (false);
		tag = cmd[2];

		switch (tag) {
		case FILECMP_UPDATE_GENERIC:
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
				      hashops->length)) {
				return (false);
			}
			break;
		case FILECMP_UPDATE_RCS:
			if (!filecmp_rcs_ignore_rcs(fca))
				return (false);
			break;
		case FILECMP_UPDATE_RDIFF:
			if (!filecmp_rdiff_ignore(fca))
				return (false);
			break;
		default:
			return (false);
		}

		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
			return (false);
		if (GetWord(cmd) != 1)
			return (false);
		if (cmd[2] != FILECMP_UPDATE_END)
			return (false);

		return (filecmp_rcs_remove(fca));
	}
	if (!cvsync_mmap(cfp, 0, cfp->cf_size)) {
		cvsync_fclose(cfp);
		return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (GetWord(cmd) != 1) {
		cvsync_fclose(cfp);
		return (false);
	}
	tag = cmd[2];

	switch (tag) {
	case FILECMP_UPDATE_GENERIC:
		if (!(*hashops->init)(&fca->fca_hash_ctx)) {
			cvsync_fclose(cfp);
			return (false);
		}
		(*hashops->update)(fca->fca_hash_ctx, cfp->cf_addr,
				   (size_t)cfp->cf_size);
		(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);

		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
			      hashops->length)) {
			cvsync_fclose(cfp);
			return (false);
		}

		if (memcmp(fca->fca_hash, cmd, hashops->length) == 0) {
			if (!cvsync_fclose(cfp))
				return (false);
			if (!filecmp_rcs_setattr(fca))
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
				return (false);
			if (GetWord(cmd) != 1)
				return (false);
			if (cmd[2] != FILECMP_UPDATE_END)
				return (false);
			return (true);
		}

		break;
	case FILECMP_UPDATE_RCS:
	    {
		struct rcslib_file *rcs;

		if ((rcs = rcslib_init(cfp->cf_addr, cfp->cf_size)) == NULL) {
			if (!filecmp_rcs_ignore_rcs(fca)) {
				cvsync_fclose(cfp);
				return (false);
			}
			goto done;
		}
		rcslib_destroy(rcs);
		break;
	    }
	case FILECMP_UPDATE_RDIFF:
		if (filecmp_access(fca, cap) == DISTFILE_NORDIFF) {
			if (filecmp_rdiff_ischanged(fca, cfp)) {
				if (!filecmp_rcs_setattr(fca)) {
					cvsync_fclose(cfp);
					return (false);
				}
				goto done;
			}

			tag = FILECMP_UPDATE_GENERIC;

			if (!(*hashops->init)(&fca->fca_hash_ctx)) {
				cvsync_fclose(cfp);
				return (false);
			}
			(*hashops->update)(fca->fca_hash_ctx, cfp->cf_addr,
					   (size_t)cfp->cf_size);
			(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

	mode = RCS_MODE(cfp->cf_mode, fca->fca_umask);

	if ((base = cap->ca_namelen + 6) > fca->fca_cmdmax) {
		cvsync_fclose(cfp);
		return (false);
	}
	len = fca->fca_cmdmax - base;

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if ((len = attr_rcs_encode_file(&cmd[6], len, cfp->cf_mtime,
						cfp->cf_size, mode)) == 0) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((len = attr_rcs_encode_rcs(&cmd[6], len, cfp->cf_mtime,
					       mode)) == 0) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

	SetWord(cmd, len + base - 2);
	cmd[2] = UPDATER_UPDATE;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, &cmd[6], len)) {
		cvsync_fclose(cfp);
		return (false);
	}

	switch (tag) {
	case FILECMP_UPDATE_GENERIC:
		if (!filecmp_generic_update(fca, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	case FILECMP_UPDATE_RCS:
		if (!filecmp_rcs_update_rcs(fca, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	case FILECMP_UPDATE_RDIFF:
		if (!filecmp_rdiff_update(fca, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

done:
	if (!cvsync_fclose(cfp))
		return (false);

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	return (true);
}

bool
filecmp_rcs_update_rcs(struct filecmp_args *fca, struct cvsync_file *cfp)
{
	static const uint8_t _cmds[3] = { 0x00, 0x01, UPDATER_UPDATE_RCS };
	struct rcslib_file *rcs;
	uint32_t ndeltas;

	if ((rcs = rcslib_init(cfp->cf_addr, cfp->cf_size)) == NULL)
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmds, sizeof(_cmds))) {
		rcslib_destroy(rcs);
		return (false);
	}

	if (!filecmp_rcs_admin(fca, rcs)) {
		rcslib_destroy(rcs);
		return (false);
	}
	if (!filecmp_rcs_delta(fca, rcs, &ndeltas)) {
		rcslib_destroy(rcs);
		return (false);
	}
	if (!filecmp_rcs_desc(fca, rcs)) {
		rcslib_destroy(rcs);
		return (false);
	}
	if (!filecmp_rcs_deltatext(fca, rcs, ndeltas)) {
		rcslib_destroy(rcs);
		return (false);
	}

	rcslib_destroy(rcs);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filecmp_rcs_update_symlink(struct filecmp_args *fca)
{
	struct cvsync_attr *cap = &fca->fca_attr;
	uint8_t *cmd = fca->fca_cmd;
	size_t base;
	ssize_t wn;

	if ((base = cap->ca_namelen + 6) >= fca->fca_cmdmax)
		return (false);

	cmd[2] = UPDATER_UPDATE;
	cmd[3] = FILETYPE_SYMLINK;
	SetWord(&cmd[4], cap->ca_namelen);
	if ((wn = readlink(fca->fca_path, (char *)&cmd[6],
			   fca->fca_cmdmax - base)) == -1) {
		return (false);
	}
	SetWord(cmd, (size_t)((size_t)wn + base - 2));
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, &cmd[6], (size_t)wn))
		return (false);

	return (true);
}

bool
filecmp_rcs_admin(struct filecmp_args *fca, struct rcslib_file *rcs)
{
	struct rcsnum *num;
	struct rcsstr *str;
	uint8_t *cmd = fca->fca_cmd;
	size_t len, n;

	/* head */
	num = &rcs->head;
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != FILECMP_UPDATE_RCS_HEAD)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);
	len = cmd[1];
	if ((len != num->n_len) || (memcmp(&cmd[2], num->n_str, len) != 0)) {
		if ((len = num->n_len + 4) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_HEAD;
		cmd[3] = (uint8_t)num->n_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
			return (false);
		if (num->n_len > 0) {
			if (!mux_send(fca->fca_mux, MUX_UPDATER, num->n_str,
				      num->n_len)) {
				return (false);
			}
		}
	}

	/* branch */
	num = &rcs->branch;
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != FILECMP_UPDATE_RCS_BRANCH)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);
	len = cmd[1];
	if ((len != num->n_len) || (memcmp(&cmd[2], num->n_str, len) != 0)) {
		if ((len = num->n_len + 4) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_BRANCH;
		cmd[3] = (uint8_t)num->n_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
			return (false);
		if (num->n_len > 0) {
			if (!mux_send(fca->fca_mux, MUX_UPDATER, num->n_str,
				      num->n_len)) {
				return (false);
			}
		}
	}

	/* access */
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
		return (false);
	if (GetWord(cmd) != 2)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_RCS_ACCESS)
		return (false);
	if (!filecmp_rcs_admin_access(fca, rcs, cmd[3]))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	/* symbols */
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	if (fca->fca_proto < CVSYNC_PROTO(0, 24)) {
		if (GetWord(cmd) != 2)
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
			return (false);
		n = cmd[1];
	} else {
		if (GetWord(cmd) != 5)
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 5))
			return (false);
		n = GetDWord(&cmd[1]);
	}
	if (cmd[0] != FILECMP_UPDATE_RCS_SYMBOLS)
		return (false);
	if (!filecmp_rcs_admin_symbols(fca, rcs, n))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	/* locks */
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 5))
		return (false);
	if (GetWord(cmd) != 3)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_RCS_LOCKS)
		return (false);
	n = cmd[4];
	if ((cmd[3] != 0) != (rcs->locks.rl_strict != 0)) {
		SetWord(cmd, 2);
		cmd[2] = UPDATER_UPDATE_RCS_LOCKS_STRICT;
		if (rcs->locks.rl_strict != 0)
			cmd[3] = UPDATER_UPDATE_ADD;
		else
			cmd[3] = UPDATER_UPDATE_REMOVE;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
			return (false);
	}
	if (!filecmp_rcs_admin_locks(fca, rcs, n))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	/* comment */
	str = &rcs->comment;
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != FILECMP_UPDATE_RCS_COMMENT)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);
	if ((cmd[1] != str->s_len) ||
	    (memcmp(&cmd[2], str->s_str, cmd[1]) != 0)) {
		if ((len = str->s_len + 4) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_COMMENT;
		cmd[3] = (uint8_t)str->s_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
			return (false);
		if (str->s_len > 0) {
			if (!mux_send(fca->fca_mux, MUX_UPDATER, str->s_str,
				      str->s_len)) {
				return (false);
			}
		}
	}

	/* expand */
	str = &rcs->expand;
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != FILECMP_UPDATE_RCS_EXPAND)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);
	if ((cmd[1] != str->s_len) ||
	    (memcmp(&cmd[2], str->s_str, cmd[1]) != 0)) {
		if ((len = str->s_len + 4) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_EXPAND;
		cmd[3] = (uint8_t)str->s_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
			return (false);
		if (str->s_len > 0) {
			if (!mux_send(fca->fca_mux, MUX_UPDATER, str->s_str,
				      str->s_len)) {
				return (false);
			}
		}
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filecmp_rcs_admin_access(struct filecmp_args *fca, struct rcslib_file *rcs,
			 size_t n)
{
	struct rcsid *i1, *i2, t_id;
	uint8_t *cmd = fca->fca_cmd;
	size_t len, c = 0, i = 0;
	int rv;
	bool fetched = false;

	i2 = &t_id;

	while (i < n) {
		if (!fetched) {
			size_t ilen;

			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 1))
				return (false);
			ilen = cmd[0];
			if ((ilen == 0) || (ilen > fca->fca_cmdmax - 1))
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      fca->fca_rvcmd, ilen)) {
				return (false);
			}

			i2->i_id = (char *)fca->fca_rvcmd;
			i2->i_len = ilen;

			fetched = true;
		}

		if (c == rcs->access.ra_count) {
			if ((len = i2->i_len + 5) > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_ACCESS;
			cmd[3] = UPDATER_UPDATE_REMOVE;
			cmd[4] = (uint8_t)i2->i_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, i2->i_id,
				      i2->i_len)) {
				return (false);
			}
			fetched = false;
			i++;
			continue;
		}

		i1 = &rcs->access.ra_id[c];
		rv = rcslib_cmp_id(i1, i2);
		if (rv == 0) {
			fetched = false;
			c++;
			i++;
		} else if (rv > 0) {
			if ((len = i2->i_len + 5) > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_ACCESS;
			cmd[3] = UPDATER_UPDATE_REMOVE;
			cmd[4] = (uint8_t)i2->i_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, i2->i_id,
				      i2->i_len)) {
				return (false);
			}
			fetched = false;
			i++;
		} else { /* rv < 0 */
			if ((len = i1->i_len + 5) > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_ACCESS;
			cmd[3] = UPDATER_UPDATE_ADD;
			cmd[4] = (uint8_t)i1->i_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, i1->i_id,
				      i1->i_len)) {
				return (false);
			}
			c++;
		}
	}
	while (c < rcs->access.ra_count) {
		i1 = &rcs->access.ra_id[c];
		if ((len = i1->i_len + 5) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_ACCESS;
		cmd[3] = UPDATER_UPDATE_ADD;
		cmd[4] = (uint8_t)i1->i_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, i1->i_id, i1->i_len))
			return (false);
		c++;
	}

	return (true);
}

bool
filecmp_rcs_admin_symbols(struct filecmp_args *fca, struct rcslib_file *rcs,
			  size_t n)
{
	struct rcslib_symbol *sym1, *sym2, t_sym;
	struct rcsnum *n1, *n2;
	struct rcssym *s1, *s2;
	uint8_t *cmd = fca->fca_cmd;
	size_t len, c = 0, i = 0;
	int rv;
	bool fetched = false;

	sym2 = &t_sym;
	n2 = &sym2->num;
	s2 = &sym2->sym;

	while (i < n) {
		if (!fetched) {
			size_t nlen, slen;

			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
				return (false);
			if ((slen = (size_t)cmd[0]) == 0)
				return (false);
			if ((nlen = (size_t)cmd[1]) == 0)
				return (false);
			if ((len = nlen + slen) > fca->fca_cmdmax - 2)
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      fca->fca_rvcmd, len)) {
				return (false);
			}

			s2->s_sym = (char *)fca->fca_rvcmd;
			s2->s_len = slen;

			if (!rcslib_str2num(&fca->fca_rvcmd[slen], nlen, n2))
				return (false);

			fetched = true;
		}

		if (c == rcs->symbols.rs_count) {
			len = s2->s_len + n2->n_len + 6;
			if (len > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_SYMBOLS;
			cmd[3] = UPDATER_UPDATE_REMOVE;
			cmd[4] = (uint8_t)s2->s_len;
			cmd[5] = (uint8_t)n2->n_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, s2->s_sym,
				      s2->s_len)) {
				return (false);
			}
			if (!mux_send(fca->fca_mux, MUX_UPDATER, n2->n_str,
				      n2->n_len)) {
				return (false);
			}
			fetched = false;
			i++;
			continue;
		}

		sym1 = &rcs->symbols.rs_symbols[c];
		rv = rcslib_cmp_symbol(sym1, sym2);
		if (rv == 0) {
			fetched = false;
			c++;
			i++;
		} else if (rv > 0) {
			len = s2->s_len + n2->n_len + 6;
			if (len > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_SYMBOLS;
			cmd[3] = UPDATER_UPDATE_REMOVE;
			cmd[4] = (uint8_t)s2->s_len;
			cmd[5] = (uint8_t)n2->n_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, s2->s_sym,
				      s2->s_len)) {
				return (false);
			}
			if (!mux_send(fca->fca_mux, MUX_UPDATER, n2->n_str,
				      n2->n_len)) {
				return (false);
			}
			fetched = false;
			i++;
		} else { /* rv < 0 */
			n1 = &sym1->num;
			s1 = &sym1->sym;
			len = s1->s_len + n1->n_len + 6;
			if (len > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_SYMBOLS;
			cmd[3] = UPDATER_UPDATE_ADD;
			cmd[4] = (uint8_t)s1->s_len;
			cmd[5] = (uint8_t)n1->n_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, s1->s_sym,
				      s1->s_len)) {
				return (false);
			}
			if (!mux_send(fca->fca_mux, MUX_UPDATER, n1->n_str,
				      n1->n_len)) {
				return (false);
			}
			c++;
		}
	}
	while (c < rcs->symbols.rs_count) {
		sym1 = &rcs->symbols.rs_symbols[c];
		n1 = &sym1->num;
		s1 = &sym1->sym;
		if ((len = s1->s_len + n1->n_len + 6) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_SYMBOLS;
		cmd[3] = UPDATER_UPDATE_ADD;
		cmd[4] = (uint8_t)s1->s_len;
		cmd[5] = (uint8_t)n1->n_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, s1->s_sym, s1->s_len))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, n1->n_str, n1->n_len))
			return (false);
		c++;
	}

	return (true);
}

bool
filecmp_rcs_admin_locks(struct filecmp_args *fca, struct rcslib_file *rcs,
			size_t n)
{
	struct rcslib_lock *lock1, *lock2, t_lock;
	struct rcsid *i1, *i2;
	struct rcsnum *n1, *n2;
	uint8_t *cmd = fca->fca_cmd;
	size_t len, c = 0, i = 0;
	int rv;
	bool fetched = false;

	lock2 = &t_lock;
	i2 = &lock2->id;
	n2 = &lock2->num;

	while (i < n) {
		if (!fetched) {
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
				return (false);
			if ((cmd[0] == 0) || (cmd[1] == 0))
				return (false);
			if ((len = cmd[0] + cmd[1]) > fca->fca_cmdmax - 2)
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      fca->fca_rvcmd, len)) {
				return (false);
			}

			i2->i_id = (char *)fca->fca_rvcmd;
			i2->i_len = cmd[0];

			if (!rcslib_str2num(&fca->fca_rvcmd[cmd[0]],
					    cmd[1], n2)) {
				return (false);
			}

			fetched = true;
		}

		if (c == rcs->locks.rl_count) {
			len = i2->i_len + n2->n_len + 6;
			if (len > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_LOCKS;
			cmd[3] = UPDATER_UPDATE_REMOVE;
			cmd[4] = (uint8_t)i2->i_len;
			cmd[5] = (uint8_t)n2->n_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, i2->i_id,
				      i2->i_len)) {
				return (false);
			}
			if (!mux_send(fca->fca_mux, MUX_UPDATER, n2->n_str,
				      n2->n_len)) {
				return (false);
			}
			fetched = false;
			i++;
			continue;
		}

		lock1 = &rcs->locks.rl_locks[c];
		rv = rcslib_cmp_lock(lock1, lock2);
		if (rv == 0) {
			fetched = false;
			c++;
			i++;
		} else if (rv > 0) {
			len = i2->i_len + n2->n_len + 6;
			if (len > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_LOCKS;
			cmd[3] = UPDATER_UPDATE_REMOVE;
			cmd[4] = (uint8_t)i2->i_len;
			cmd[5] = (uint8_t)n2->n_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, i2->i_id,
				      i2->i_len)) {
				return (false);
			}
			if (!mux_send(fca->fca_mux, MUX_UPDATER, n2->n_str,
				      n2->n_len)) {
				return (false);
			}
			fetched = false;
			i++;
		} else { /* rv < 0 */
			i1 = &lock1->id;
			n1 = &lock1->num;
			len = i1->i_len + n1->n_len + 6;
			if (len > fca->fca_cmdmax)
				return (false);
			SetWord(cmd, len - 2);
			cmd[2] = UPDATER_UPDATE_RCS_LOCKS;
			cmd[3] = UPDATER_UPDATE_ADD;
			cmd[4] = (uint8_t)i1->i_len;
			cmd[5] = (uint8_t)n1->n_len;
			if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
				return (false);
			if (!mux_send(fca->fca_mux, MUX_UPDATER, i1->i_id,
				      i1->i_len)) {
				return (false);
			}
			if (!mux_send(fca->fca_mux, MUX_UPDATER, n1->n_str,
				      n1->n_len)) {
				return (false);
			}
			c++;
		}
	}
	while (c < rcs->locks.rl_count) {
		lock1 = &rcs->locks.rl_locks[c];
		i1 = &lock1->id;
		n1 = &lock1->num;
		if ((len = i1->i_len + n1->n_len + 6) > fca->fca_cmdmax)
			return (false);
		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE_RCS_LOCKS;
		cmd[3] = UPDATER_UPDATE_ADD;
		cmd[4] = (uint8_t)i1->i_len;
		cmd[5] = (uint8_t)n1->n_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 6))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, i1->i_id, i1->i_len))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, n1->n_str, n1->n_len))
			return (false);
		c++;
	}

	return (true);
}

bool
filecmp_rcs_delta(struct filecmp_args *fca, struct rcslib_file *rcs,
		  uint32_t *ndeltas)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct rcslib_revision *rev;
	struct rcsnum num;
	uint32_t n, i = 0;
	uint8_t *cmd = fca->fca_cmd, *hash = NULL;
	size_t len, c = 0;
	int rv;
	bool fetched = false;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
		return (false);
	if ((n = GetDWord(cmd)) == 0)
		return (false);

	while (i < n) {
		if (!fetched) {
			uint8_t nlen;

			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
				return (false);
			len = GetWord(cmd);
			if ((len < 2) || (len > fca->fca_cmdmax - 2))
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      fca->fca_rvcmd, len)) {
				return (false);
			}
			if (fca->fca_rvcmd[0] != FILECMP_UPDATE_RCS_DELTA)
				return (false);
			if ((nlen = fca->fca_rvcmd[1]) == 0)
				return (false);
			if (len != nlen + hashops->length + 2)
				return (false);

			hash = &fca->fca_rvcmd[nlen + 2];

			if (!rcslib_str2num(&fca->fca_rvcmd[2], nlen, &num))
				return (false);

			fetched = true;
		}

		if (c == rcs->delta.rd_count) {
			if (!filecmp_rcs_delta_remove(fca, &num))
				return (false);
			fetched = false;
			i++;
			continue;
		}

		rev = &rcs->delta.rd_rev[c];
		rv = rcslib_cmp_num(&rev->num, &num);
		if (rv == 0) {
			if (!filecmp_rcs_delta_update(fca, rev, hash))
				return (false);
			fetched = false;
			c++;
			i++;
		} else if (rv > 0) {
			if (!filecmp_rcs_delta_remove(fca, &num))
				return (false);
			fetched = false;
			i++;
		} else { /* rv < 0 */
			if (!filecmp_rcs_delta_add(fca, rev))
				return (false);
			c++;
		}
	}
	while (c < rcs->delta.rd_count) {
		rev = &rcs->delta.rd_rev[c++];
		if (!filecmp_rcs_delta_add(fca, rev))
			return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	*ndeltas = n;

	return (true);
}

bool
filecmp_rcs_delta_add(struct filecmp_args *fca, struct rcslib_revision *rev)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct rcslib_branches *branches = &rev->branches;
	uint8_t *cmd = fca->fca_cmd;
	size_t len, i;

	if (!(*hashops->init)(&fca->fca_hash_ctx))
		return (false);

	len = rev->num.n_len + rev->date.rd_num.n_len + rev->author.i_len;
	len += rev->state.i_len + rev->next.n_len + hashops->length + 11;
	for (i = 0 ; i < branches->rb_count ; i++)
		len += branches->rb_num[i].n_len + 1;
	if (len > fca->fca_cmdmax) {
		(*hashops->destroy)(fca->fca_hash_ctx);
		return (false);
	}

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DELTA;
	cmd[3] = UPDATER_UPDATE_ADD;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
		return (false);

	/* num */
	cmd[0] = (uint8_t)rev->num.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->num.n_str,
		      rev->num.n_len)) {
		return (false);
	}

	/* date */
	cmd[0] = (uint8_t)rev->date.rd_num.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->date.rd_num.n_str,
		      rev->date.rd_num.n_len)) {
		return (false);
	}
	(*hashops->update)(fca->fca_hash_ctx, rev->date.rd_num.n_str,
			   rev->date.rd_num.n_len);

	/* author */
	cmd[0] = (uint8_t)rev->author.i_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->author.i_id,
		      rev->author.i_len)) {
		return (false);
	}
	(*hashops->update)(fca->fca_hash_ctx, rev->author.i_id,
			   rev->author.i_len);

	/* state */
	cmd[0] = (uint8_t)rev->state.i_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (rev->state.i_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->state.i_id,
			      rev->state.i_len)) {
			return (false);
		}
		(*hashops->update)(fca->fca_hash_ctx, rev->state.i_id,
				   rev->state.i_len);
	}

	/* branches */
	SetWord(cmd, branches->rb_count);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 2))
		return (false);
	for (i = 0 ; i < branches->rb_count ; i++) {
		struct rcsnum *num = &branches->rb_num[i];
		cmd[0] = (uint8_t)num->n_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, num->n_str,
			      num->n_len)) {
			return (false);
		}
		(*hashops->update)(fca->fca_hash_ctx, num->n_str, num->n_len);
	}

	/* next */
	cmd[0] = (uint8_t)rev->next.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (rev->next.n_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->next.n_str,
			      rev->next.n_len)) {
			return (false);
		}
		(*hashops->update)(fca->fca_hash_ctx, rev->next.n_str,
				   rev->next.n_len);
	}

	(*hashops->final)(fca->fca_hash_ctx, cmd);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, hashops->length))
		return (false);

	return (true);
}

bool
filecmp_rcs_delta_remove(struct filecmp_args *fca, struct rcsnum *num)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if ((len = num->n_len + 5) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DELTA;
	cmd[3] = UPDATER_UPDATE_REMOVE;
	cmd[4] = (uint8_t)num->n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, num->n_str, num->n_len))
		return (false);

	return (true);
}

bool
filecmp_rcs_delta_update(struct filecmp_args *fca, struct rcslib_revision *rev,
			 uint8_t *hash)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct rcslib_branches *branches = &rev->branches;
	struct rcsnum *num;
	uint8_t *cmd = fca->fca_cmd;
	size_t len, i;

	if (!(*hashops->init)(&fca->fca_hash_ctx))
		return (false);

	/* date */
	(*hashops->update)(fca->fca_hash_ctx, rev->date.rd_num.n_str,
			   rev->date.rd_num.n_len);
	/* author */
	(*hashops->update)(fca->fca_hash_ctx, rev->author.i_id,
			   rev->author.i_len);
	/* state */
	if (rev->state.i_len > 0) {
		(*hashops->update)(fca->fca_hash_ctx, rev->state.i_id,
				   rev->state.i_len);
	}
	/* branches */
	for (i = 0 ; i < branches->rb_count ; i++) {
		num = &branches->rb_num[i];
		(*hashops->update)(fca->fca_hash_ctx, num->n_str, num->n_len);
	}
	/* next */
	if (rev->next.n_len > 0) {
		(*hashops->update)(fca->fca_hash_ctx, rev->next.n_str,
				   rev->next.n_len);
	}

	(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);

	if (memcmp(hash, fca->fca_hash, hashops->length) == 0)
		return (true);

	len = rev->num.n_len + rev->date.rd_num.n_len + rev->author.i_len;
	len += rev->state.i_len + rev->next.n_len + hashops->length + 11;
	for (i = 0 ; i < branches->rb_count ; i++)
		len += branches->rb_num[i].n_len + 1;
	if (len > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DELTA;
	cmd[3] = UPDATER_UPDATE_UPDATE;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
		return (false);

	/* num */
	cmd[0] = (uint8_t)rev->num.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->num.n_str,
		      rev->num.n_len)) {
		return (false);
	}

	/* date */
	cmd[0] = (uint8_t)rev->date.rd_num.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->date.rd_num.n_str,
		      rev->date.rd_num.n_len)) {
		return (false);
	}

	/* author */
	cmd[0] = (uint8_t)rev->author.i_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->author.i_id,
		      rev->author.i_len)) {
		return (false);
	}

	/* state */
	cmd[0] = (uint8_t)rev->state.i_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (rev->state.i_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->state.i_id,
			      rev->state.i_len)) {
			return (false);
		}
	}

	/* branches */
	SetWord(cmd, rev->branches.rb_count);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 2))
		return (false);
	for (i = 0 ; i < branches->rb_count ; i++) {
		num = &branches->rb_num[i];
		cmd[0] = (uint8_t)num->n_len;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, num->n_str,
			      num->n_len)) {
			return (false);
		}
	}

	/* next */
	cmd[0] = (uint8_t)rev->next.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 1))
		return (false);
	if (rev->next.n_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->next.n_str,
			      rev->next.n_len)) {
			return (false);
		}
	}

	if (!mux_send(fca->fca_mux, MUX_UPDATER, fca->fca_hash,
		      hashops->length)) {
		return (false);
	}

	return (true);
}

bool
filecmp_rcs_desc(struct filecmp_args *fca, struct rcslib_file *rcs)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	/* desc */
	if ((len = rcs->desc.s_len + 4) > fca->fca_cmdmax)
		return (false);
	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DESC;
	cmd[3] = (uint8_t)rcs->desc.s_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
		return (false);
	if (rcs->desc.s_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rcs->desc.s_str,
			      rcs->desc.s_len)) {
			return (false);
		}
	}

	return (true);
}

bool
filecmp_rcs_deltatext(struct filecmp_args *fca, struct rcslib_file *rcs,
		      uint32_t ndeltas)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct rcslib_revision *rev;
	struct rcsnum num;
	uint32_t n, i = 0;
	uint8_t *cmd = fca->fca_cmd, *hash = NULL;
	size_t len, c = 0;
	int rv;
	bool fetched = false;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
		return (false);
	if ((n = GetDWord(cmd)) != ndeltas)
		return (false);

	/* deltatext */
	while (i < n) {
		if (!fetched) {
			uint8_t nlen;

			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
				return (false);
			len = GetWord(cmd);
			if ((len < 2) || (len > fca->fca_cmdmax - 2))
				return (false);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN,
				      fca->fca_rvcmd, len)) {
				return (false);
			}
			if (fca->fca_rvcmd[0] != FILECMP_UPDATE_RCS_DELTATEXT)
				return (false);
			if ((nlen = fca->fca_rvcmd[1]) == 0)
				return (false);
			if (len != nlen + hashops->length + 2)
				return (false);

			hash = &fca->fca_rvcmd[nlen + 2];

			if (!rcslib_str2num(&fca->fca_rvcmd[2], nlen, &num))
				return (false);

			fetched = true;
		}

		if (c == rcs->delta.rd_count) {
			if (!filecmp_rcs_deltatext_remove(fca, &num))
				return (false);
			fetched = false;
			i++;
			continue;
		}

		rev = &rcs->delta.rd_rev[c];
		rv = rcslib_cmp_num(&rev->num, &num);
		if (rv == 0) {
			if (!filecmp_rcs_deltatext_update(fca, rev, hash))
				return (false);
			fetched = false;
			c++;
			i++;
		} else if (rv > 0) {
			if (!filecmp_rcs_deltatext_remove(fca, &num))
				return (false);
			fetched = false;
			i++;
		} else { /* rv < 0 */
			if (!filecmp_rcs_deltatext_add(fca, rev))
				return (false);
			c++;
		}
	}
	while (c < rcs->delta.rd_count) {
		rev = &rcs->delta.rd_rev[c++];
		if (!filecmp_rcs_deltatext_add(fca, rev))
			return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filecmp_rcs_deltatext_add(struct filecmp_args *fca,
			  struct rcslib_revision *rev)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if ((len = rev->num.n_len + 5) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DELTATEXT;
	cmd[3] = UPDATER_UPDATE_ADD;
	cmd[4] = (uint8_t)rev->num.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->num.n_str,
		      rev->num.n_len)) {
		return (false);
	}

	if (!(*hashops->init)(&fca->fca_hash_ctx))
		return (false);

	/* log */
	SetDWord(cmd, rev->log.s_len);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4)) {
		(*hashops->destroy)(fca->fca_hash_ctx);
		return (false);
	}
	if (rev->log.s_len > 0) {
		(*hashops->update)(fca->fca_hash_ctx, rev->log.s_str,
				   rev->log.s_len);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->log.s_str,
			      rev->log.s_len)) {
			(*hashops->destroy)(fca->fca_hash_ctx);
			return (false);
		}
	}

	/* text */
	SetDDWord(cmd, rev->text.s_len);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 8)) {
		(*hashops->destroy)(fca->fca_hash_ctx);
		return (false);
	}
	if (rev->text.s_len > 0) {
		(*hashops->update)(fca->fca_hash_ctx, rev->text.s_str,
				   rev->text.s_len);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->text.s_str,
			      rev->text.s_len)) {
			(*hashops->destroy)(fca->fca_hash_ctx);
			return (false);
		}
	}
	(*hashops->final)(fca->fca_hash_ctx, cmd);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, hashops->length))
		return (false);

	return (true);
}

bool
filecmp_rcs_deltatext_remove(struct filecmp_args *fca, struct rcsnum *num)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if ((len = num->n_len + 5) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DELTATEXT;
	cmd[3] = UPDATER_UPDATE_REMOVE;
	cmd[4] = (uint8_t)num->n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, num->n_str, num->n_len))
		return (false);

	return (true);
}

bool
filecmp_rcs_deltatext_update(struct filecmp_args *fca,
			     struct rcslib_revision *rev, uint8_t *hash)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if (!(*hashops->init)(&fca->fca_hash_ctx))
		return (false);

	/* log */
	if (rev->log.s_len > 0) {
		(*hashops->update)(fca->fca_hash_ctx, rev->log.s_str,
				   rev->log.s_len);
	}
	/* text */
	if (rev->text.s_len > 0) {
		(*hashops->update)(fca->fca_hash_ctx, rev->text.s_str,
				   rev->text.s_len);
	}

	(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);

	if (memcmp(hash, fca->fca_hash, hashops->length) == 0)
		return (true);

	if ((len = rev->num.n_len + 5) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_UPDATE_RCS_DELTATEXT;
	cmd[3] = UPDATER_UPDATE_UPDATE;
	cmd[4] = (uint8_t)rev->num.n_len;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->num.n_str,
		      rev->num.n_len)) {
		return (false);
	}

	/* log */
	SetDWord(cmd, rev->log.s_len);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 4))
		return (false);
	if (rev->log.s_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->log.s_str,
			      rev->log.s_len)) {
			return (false);
		}
	}

	/* text */
	SetDDWord(cmd, rev->text.s_len);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 8))
		return (false);
	if (rev->text.s_len > 0) {
		if (!mux_send(fca->fca_mux, MUX_UPDATER, rev->text.s_str,
			      rev->text.s_len)) {
			return (false);
		}
	}

	if (!mux_send(fca->fca_mux, MUX_UPDATER, fca->fca_hash,
		      hashops->length)) {
		return (false);
	}

	return (true);
}

bool
filecmp_rcs_ignore_rcs(struct filecmp_args *fca)
{
	uint32_t n;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	/* head */
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != FILECMP_UPDATE_RCS_HEAD)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);

	/* branch */
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != FILECMP_UPDATE_RCS_BRANCH)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);

	/* access */
	if (!filecmp_rcs_ignore_rcs_list(fca, FILECMP_UPDATE_RCS_ACCESS))
		return (false);

	/* symbols */
	if (!filecmp_rcs_ignore_rcs_list(fca, FILECMP_UPDATE_RCS_SYMBOLS))
		return (false);

	/* locks */
	if (!filecmp_rcs_ignore_rcs_list(fca, FILECMP_UPDATE_RCS_LOCKS))
		return (false);

	/* comment */
	if (!filecmp_rcs_ignore_rcs_string(fca, FILECMP_UPDATE_RCS_COMMENT))
		return (false);

	/* expand */
	if (!filecmp_rcs_ignore_rcs_string(fca, FILECMP_UPDATE_RCS_EXPAND))
		return (false);

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	/* delta */
	if (!filecmp_rcs_ignore_rcs_delta(fca, &n))
		return (false);

	/* deltatext */
	if (!filecmp_rcs_ignore_rcs_deltatext(fca, n))
		return (false);

	return (true);
}

bool
filecmp_rcs_ignore_rcs_delta(struct filecmp_args *fca, uint32_t *ndeltas)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	uint32_t n, i;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
		return (false);
	if ((n = GetDWord(cmd)) == 0)
		return (false);

	for (i = 0 ; i < n ; i++) {
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len < 2) || (len > fca->fca_cmdmax - 2))
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
			return (false);
		if (cmd[0] != FILECMP_UPDATE_RCS_DELTA)
			return (false);
		if (len != cmd[1] + hashops->length + 2)
			return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	*ndeltas = n;

	return (true);
}

bool
filecmp_rcs_ignore_rcs_deltatext(struct filecmp_args *fca, uint32_t ndeltas)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	uint32_t n, i;
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
		return (false);
	if ((n = GetDWord(cmd)) != ndeltas)
		return (false);

	for (i = 0 ; i < n ; i++) {
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
			return (false);
		len = GetWord(cmd);
		if ((len < 2) || (len > fca->fca_cmdmax - 2))
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
			return (false);
		if (cmd[0] != FILECMP_UPDATE_RCS_DELTATEXT)
			return (false);
		if (len != cmd[1] + hashops->length + 2)
			return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	return (true);
}

bool
filecmp_rcs_ignore_rcs_list(struct filecmp_args *fca, uint8_t type)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len, n;

	switch (type) {
	case FILECMP_UPDATE_RCS_LOCKS:
		len = 3;
		break;
	case FILECMP_UPDATE_RCS_SYMBOLS:
		if (fca->fca_proto < CVSYNC_PROTO(0, 24))
			len = 2;
		else
			len = 5;
		break;
	default:
		len = 2;
		break;
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	if (GetWord(cmd) != (uint16_t)len)
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != type)
		return (false);
	if ((type != FILECMP_UPDATE_RCS_SYMBOLS) ||
	    (fca->fca_proto < CVSYNC_PROTO(0, 24))) {
		n = cmd[len - 1];
	} else {
		n = GetDWord(&cmd[1]);
	}
	while (n > 0) {
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
			return (false);
		if ((cmd[0] == 0) || (cmd[1] == 1))
			return (false);
		if ((len = cmd[0] + cmd[1]) > fca->fca_cmdmax - 2)
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
			return (false);
		n--;
	}
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_UPDATE_END)
		return (false);

	return (true);
}

bool
filecmp_rcs_ignore_rcs_string(struct filecmp_args *fca, uint8_t type)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len < 2) || (len > fca->fca_cmdmax - 2))
		return (false);
	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, len))
		return (false);
	if (cmd[0] != type)
		return (false);
	if (len != (size_t)cmd[1] + 2)
		return (false);

	return (true);
}
