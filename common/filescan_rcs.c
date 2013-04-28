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

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "attribute.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "rcslib.h"
#include "refuse.h"
#include "version.h"

#include "filescan.h"
#include "filecmp.h"

bool filescan_rcs_fetch(struct filescan_args *);

bool filescan_rcs_add(struct filescan_args *);
bool filescan_rcs_remove(struct filescan_args *);
bool filescan_rcs_attic(struct filescan_args *);
bool filescan_rcs_setattr(struct filescan_args *);
bool filescan_rcs_setattr_dir(struct filescan_args *);
bool filescan_rcs_setattr_file(struct filescan_args *);
bool filescan_rcs_update(struct filescan_args *);

bool filescan_rcs_update_rcs(struct filescan_args *, struct cvsync_file *);
bool filescan_rcs_update_rcs_admin(struct filescan_args *,
				   struct rcslib_file *);
bool filescan_rcs_update_rcs_delta(struct filescan_args *,
				   struct rcslib_file *);
bool filescan_rcs_update_rcs_deltatext(struct filescan_args *,
				       struct rcslib_file *);
bool filescan_rcs_update_symlink(struct filescan_args *);
bool filescan_rcs_replace(struct filescan_args *);

static const uint8_t _cmde[3] = { 0x00, 0x01, FILECMP_UPDATE_END };

bool
filescan_rcs(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;

	for (;;) {
		if (cvsync_isinterrupted())
			return (false);

		if (!filescan_rcs_fetch(fsa))
			return (false);

		if (cap->ca_tag == FILESCAN_END)
			break;

		if (!refuse_access(fsa->fsa_refuse, cap))
			continue;

		switch (cap->ca_tag) {
		case FILESCAN_ADD:
			if (!filescan_rcs_add(fsa)) {
				logmsg_err("FileScan(RCS): ADD %s",
					   fsa->fsa_path);
				return (false);
			}
			break;
		case FILESCAN_REMOVE:
			if (!filescan_rcs_remove(fsa)) {
				logmsg_err("FileScan(RCS): REMOVE %s",
					   fsa->fsa_path);
				return (false);
			}
			break;
		case FILESCAN_RCS_ATTIC:
			if (!filescan_rcs_attic(fsa)) {
				logmsg_err("FileScan(RCS): ATTIC %s",
					   fsa->fsa_path);
				return (false);
			}
			break;
		case FILESCAN_SETATTR:
			if (!filescan_rcs_setattr(fsa)) {
				logmsg_err("FileScan(RCS): SETATTR %s",
					   fsa->fsa_path);
				return (false);
			}
			break;
		case FILESCAN_UPDATE:
			if (!filescan_rcs_update(fsa)) {
				logmsg_err("FileScan(RCS): UPDATE %s",
					   fsa->fsa_path);
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
filescan_rcs_fetch(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t pathlen, len;

	if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > fsa->fsa_cmdmax - 2))
		return (false);
	if ((cap->ca_tag = cmd[2]) == FILESCAN_END)
		return (len == 1);
	if (len < 2)
		return (false);

	if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd, 3))
		return (false);

	cap->ca_type = cmd[0];
	if ((cap->ca_namelen = GetWord(&cmd[1])) > sizeof(cap->ca_name))
		return (false);

	switch (cap->ca_tag) {
	case FILESCAN_ADD:
	case FILESCAN_REMOVE:
		if (cap->ca_namelen != len - 4)
			return (false);
		if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		break;
	case FILESCAN_RCS_ATTIC:
		if ((cap->ca_type != FILETYPE_RCS) &&
		    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
			return (false);
		}
		if (cap->ca_namelen != len - RCS_ATTRLEN_RCS - 4)
			return (false);
		if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!IS_FILE_RCS(cap->ca_name, cap->ca_namelen))
			return (false);
		if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd,
			      RCS_ATTRLEN_RCS)) {
			return (false);
		}
		if (!attr_rcs_decode_rcs(cmd, RCS_ATTRLEN_RCS, cap))
			return (false);
		break;
	case FILESCAN_UPDATE:
		if (cap->ca_type == FILETYPE_DIR)
			return (false);
		if (cap->ca_type == FILETYPE_SYMLINK) {
			if (len != cap->ca_namelen + 4)
				return (false);
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			break;
		}
		/* FALLTHROUGH */
	case FILESCAN_SETATTR:
		switch (cap->ca_type) {
		case FILETYPE_DIR:
			if (cap->ca_namelen != len - RCS_ATTRLEN_DIR - 4)
				return (false);
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd,
				      RCS_ATTRLEN_DIR)) {
				return (false);
			}
			if (!attr_rcs_decode_dir(cmd, RCS_ATTRLEN_DIR, cap))
				return (false);
			break;
		case FILETYPE_FILE:
			if (cap->ca_namelen != len - RCS_ATTRLEN_FILE - 4)
				return (false);
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd,
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
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN,
				      cap->ca_name, cap->ca_namelen)) {
				return (false);
			}
			if (!IS_FILE_RCS(cap->ca_name, cap->ca_namelen))
				return (false);
			if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd,
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

	if ((pathlen = fsa->fsa_pathlen + cap->ca_namelen) >= fsa->fsa_pathmax)
		return (false);

	(void)memcpy(fsa->fsa_rpath, cap->ca_name, cap->ca_namelen);
	fsa->fsa_rpath[cap->ca_namelen] = '\0';
	if (cap->ca_type == FILETYPE_RCS_ATTIC) {
		if (!cvsync_rcs_insert_attic(fsa->fsa_path, pathlen,
					     fsa->fsa_pathmax)) {
			return (false);
		}
	}

	return (true);
}

bool
filescan_rcs_add(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len;

	if ((len = cap->ca_namelen + 6) > fsa->fsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_ADD;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	return (true);
}

bool
filescan_rcs_remove(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len;

	if ((len = cap->ca_namelen + 6) > fsa->fsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_REMOVE;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	return (true);
}

bool
filescan_rcs_attic(struct filescan_args *fsa)
{
	struct cvsync_file *cfp;
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint16_t mode;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t pathlen, base, len;

	switch (cap->ca_type) {
	case FILETYPE_RCS:
		pathlen = fsa->fsa_pathlen + cap->ca_namelen;
		if (!cvsync_rcs_insert_attic(fsa->fsa_path, pathlen,
					     fsa->fsa_pathmax)) {
			return (false);
		}
		break;
	case FILETYPE_RCS_ATTIC:
		pathlen = fsa->fsa_pathlen + cap->ca_namelen + 6;
		if (!cvsync_rcs_remove_attic(fsa->fsa_path, pathlen))
			return (false);
		break;
	default:
		return (false);
	}

	if ((cfp = cvsync_fopen(fsa->fsa_path)) == NULL) {
		switch (cap->ca_type) {
		case FILETYPE_RCS:
			pathlen = fsa->fsa_pathlen + cap->ca_namelen + 6;
			if (!cvsync_rcs_remove_attic(fsa->fsa_path, pathlen))
				return (false);
			break;
		case FILETYPE_RCS_ATTIC:
			pathlen = fsa->fsa_pathlen + cap->ca_namelen;
			if (!cvsync_rcs_insert_attic(fsa->fsa_path, pathlen,
						     fsa->fsa_pathmax)) {
				return (false);
			}
			break;
		default:
			return (false);
		}
		if (!filescan_rcs_remove(fsa))
			return (false);
		return (filescan_rcs_add(fsa));
	}
	if (!cvsync_mmap(cfp, (off_t)0, cfp->cf_size)) {
		cvsync_fclose(cfp);
		return (false);
	}
	mode = RCS_MODE(cfp->cf_mode, fsa->fsa_umask);

	if ((base = cap->ca_namelen + 6) > fsa->fsa_cmdmax) {
		cvsync_fclose(cfp);
		return (false);
	}

	cmd[2] = FILECMP_RCS_ATTIC;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if ((len = attr_rcs_encode_rcs(&cmd[6], fsa->fsa_cmdmax - base,
				       cfp->cf_mtime, mode)) == 0) {
		cvsync_fclose(cfp);
		return (false);
	}
	SetWord(cmd, len + base - 2);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, &cmd[6], len)) {
		cvsync_fclose(cfp);
		return (false);
	}

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if (fsa->fsa_proto < CVSYNC_PROTO(0, 24)) {
			if (!filescan_generic_update(fsa, cfp)) {
				cvsync_fclose(cfp);
				return (false);
			}
		} else {
			if (!filescan_rdiff_update(fsa, cfp)) {
				cvsync_fclose(cfp);
				return (false);
			}
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if (!filescan_rcs_update_rcs(fsa, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

	if (!cvsync_fclose(cfp))
		return (false);

	return (true);
}

bool
filescan_rcs_setattr(struct filescan_args *fsa)
{
	switch (fsa->fsa_attr.ca_type) {
	case FILETYPE_DIR:
		return (filescan_rcs_setattr_dir(fsa));
	case FILETYPE_FILE:
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		return (filescan_rcs_setattr_file(fsa));
	default:
		break;
	}

	return (false);
}

bool
filescan_rcs_setattr_dir(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t base, len;

	if (lstat(fsa->fsa_path, &st) == -1) {
		if (errno != ENOENT)
			return (false);
		st.st_mode = S_IFDIR;
	}
	if (!S_ISDIR(st.st_mode))
		return (filescan_rcs_remove(fsa));

	if ((mode = RCS_MODE(st.st_mode, fsa->fsa_umask)) == cap->ca_mode)
		return (true);

	if ((base = cap->ca_namelen + 6) > fsa->fsa_cmdmax)
		return (false);

	if ((len = attr_rcs_encode_dir(&cmd[6], fsa->fsa_cmdmax - base,
				       mode)) == 0) {
		return (false);
	}

	SetWord(cmd, len + base - 2);
	cmd[2] = FILECMP_SETATTR;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, &cmd[6], len))
		return (false);

	return (true);
}

bool
filescan_rcs_setattr_file(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	struct stat st;
	uint16_t mode;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t base, len;

	if (lstat(fsa->fsa_path, &st) == -1)
		return (filescan_rcs_replace(fsa));

	if ((mode = RCS_MODE(st.st_mode, fsa->fsa_umask)) == cap->ca_mode)
		return (true);

	if ((base = cap->ca_namelen + 6) > fsa->fsa_cmdmax)
		return (false);
	len = fsa->fsa_cmdmax - base;

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if ((len = attr_rcs_encode_file(&cmd[6], len, st.st_mtime,
						st.st_size, mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((len = attr_rcs_encode_rcs(&cmd[6], len, st.st_mtime,
					       mode)) == 0) {
			return (false);
		}
		break;
	default:
		return (false);
	}

	SetWord(cmd, len + base - 2);
	cmd[2] = FILECMP_SETATTR;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, &cmd[6], len))
		return (false);

	return (true);
}

bool
filescan_rcs_update(struct filescan_args *fsa)
{
	struct cvsync_file *cfp;
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint16_t mode;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t base, len;

	if (cap->ca_type == FILETYPE_SYMLINK)
		return (filescan_rcs_update_symlink(fsa));

	if ((cfp = cvsync_fopen(fsa->fsa_path)) == NULL)
		return (filescan_rcs_replace(fsa));
	if (!cvsync_mmap(cfp, (off_t)0, cfp->cf_size)) {
		cvsync_fclose(cfp);
		return (false);
	}
	mode = RCS_MODE(cfp->cf_mode, fsa->fsa_umask);

	if ((base = cap->ca_namelen + 6) > fsa->fsa_cmdmax) {
		cvsync_fclose(cfp);
		return (false);
	}
	len = fsa->fsa_cmdmax - base;

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
	}

	SetWord(cmd, len + base - 2);
	cmd[2] = FILECMP_UPDATE;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, &cmd[6], len)) {
		cvsync_fclose(cfp);
		return (false);
	}

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if (fsa->fsa_proto < CVSYNC_PROTO(0, 24)) {
			if (!filescan_generic_update(fsa, cfp)) {
				cvsync_fclose(cfp);
				return (false);
			}
		} else {
			if (!filescan_rdiff_update(fsa, cfp)) {
				cvsync_fclose(cfp);
				return (false);
			}
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if (!filescan_rcs_update_rcs(fsa, cfp)) {
			cvsync_fclose(cfp);
			return (false);
		}
		break;
	default:
		cvsync_fclose(cfp);
		return (false);
	}

	if (!cvsync_fclose(cfp))
		return (false);

	return (true);
}

bool
filescan_rcs_update_rcs(struct filescan_args *fsa, struct cvsync_file *cfp)
{
	static const uint8_t _cmds[3] = { 0x00, 0x01, FILECMP_UPDATE_RCS };
	struct rcslib_file *rcs;
	struct cvsync_attr *cap = &fsa->fsa_attr;

	if ((cap->ca_type != FILETYPE_RCS) &&
	    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
		return (false);
	}

	if ((rcs = rcslib_init(cfp->cf_addr, cfp->cf_size)) == NULL) {
		if (fsa->fsa_proto < CVSYNC_PROTO(0, 24))
			return (filescan_generic_update(fsa, cfp));
		else
			return (filescan_rdiff_update(fsa, cfp));
	}
	if ((fsa->fsa_proto < CVSYNC_PROTO(0, 24)) &&
	    (rcs->symbols.rs_count > UINT8_MAX)) {
		rcslib_destroy(rcs);
		return (filescan_generic_update(fsa, cfp));
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmds, sizeof(_cmds))) {
		rcslib_destroy(rcs);
		return (false);
	}

	if (!filescan_rcs_update_rcs_admin(fsa, rcs)) {
		rcslib_destroy(rcs);
		return (false);
	}
	if (!filescan_rcs_update_rcs_delta(fsa, rcs)) {
		rcslib_destroy(rcs);
		return (false);
	}
	if (!filescan_rcs_update_rcs_deltatext(fsa, rcs)) {
		rcslib_destroy(rcs);
		return (false);
	}

	rcslib_destroy(rcs);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filescan_rcs_update_rcs_admin(struct filescan_args *fsa,
			      struct rcslib_file *rcs)
{
	struct rcsid *id;
	struct rcsnum *num;
	struct rcssym *sym;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len, i;

	/* head */
	if ((len = rcs->head.n_len + 4) > fsa->fsa_cmdmax)
		return (false);
	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_UPDATE_RCS_HEAD;
	cmd[3] = rcs->head.n_len;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);
	if (rcs->head.n_len > 0) {
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, rcs->head.n_str,
			      rcs->head.n_len)) {
			return (false);
		}
	}

	/* branch */
	if ((len = rcs->branch.n_len + 4) > fsa->fsa_cmdmax)
		return (false);
	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_UPDATE_RCS_BRANCH;
	cmd[3] = rcs->branch.n_len;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);
	if (rcs->branch.n_len > 0) {
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, rcs->branch.n_str,
			      rcs->branch.n_len)) {
			return (false);
		}
	}

	/* access */
	SetWord(cmd, 2);
	cmd[2] = FILECMP_UPDATE_RCS_ACCESS;
	cmd[3] = rcs->access.ra_count;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);
	if (rcs->access.ra_count > 0) {
		for (i = 0 ; i < rcs->access.ra_count ; i++) {
			id = &rcs->access.ra_id[i];
			if ((len = id->i_len + 1) > fsa->fsa_cmdmax)
				return (false);
			cmd[0] = id->i_len;
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 1))
				return (false);
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, id->i_id,
				      id->i_len)) {
				return (false);
			}
		}
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	/* symbols */
	if (fsa->fsa_proto < CVSYNC_PROTO(0, 24)) {
		SetWord(cmd, 2);
		cmd[2] = FILECMP_UPDATE_RCS_SYMBOLS;
		cmd[3] = rcs->symbols.rs_count;
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
			return (false);
	} else {
		SetWord(cmd, 5);
		cmd[2] = FILECMP_UPDATE_RCS_SYMBOLS;
		SetDWord(&cmd[3], rcs->symbols.rs_count);
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 7))
			return (false);
	}
	if (rcs->symbols.rs_count > 0) {
		for (i = 0 ; i < rcs->symbols.rs_count ; i++) {
			struct rcslib_symbol *symbol;

			symbol = &rcs->symbols.rs_symbols[i];
			num = &symbol->num;
			sym = &symbol->sym;

			len = num->n_len + sym->s_len + 2;
			if (len > fsa->fsa_cmdmax)
				return (false);
			cmd[0] = sym->s_len;
			cmd[1] = num->n_len;
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 2))
				return (false);
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, sym->s_sym,
				      sym->s_len)) {
				return (false);
			}
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, num->n_str,
				      num->n_len)) {
				return (false);
			}
		}
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	/* locks */
	SetWord(cmd, 3);
	cmd[2] = FILECMP_UPDATE_RCS_LOCKS;
	cmd[3] = (rcs->locks.rl_strict != 0) ? 1 : 0;
	cmd[4] = rcs->locks.rl_count;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 5))
		return (false);
	if (rcs->locks.rl_count > 0) {
		for (i = 0 ; i < rcs->locks.rl_count ; i++) {
			struct rcslib_lock *lock = &rcs->locks.rl_locks[i];
			id = &lock->id;
			num = &lock->num;
			len = id->i_len + num->n_len + 2;
			if (len > fsa->fsa_cmdmax)
				return (false);
			cmd[0] = id->i_len;
			cmd[1] = num->n_len;
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 2))
				return (false);
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, id->i_id,
				      id->i_len)) {
				return (false);
			}
			if (!mux_send(fsa->fsa_mux, MUX_FILECMP, num->n_str,
				      num->n_len)) {
				return (false);
			}
		}
	}
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	/* comment */
	if ((len = rcs->comment.s_len + 4) > fsa->fsa_cmdmax)
		return (false);
	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_UPDATE_RCS_COMMENT;
	cmd[3] = rcs->comment.s_len;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);
	if (rcs->comment.s_len > 0) {
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, rcs->comment.s_str,
			      rcs->comment.s_len)) {
			return (false);
		}
	}

	/* expand */
	if ((len = rcs->expand.s_len + 4) > fsa->fsa_cmdmax)
		return (false);
	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_UPDATE_RCS_EXPAND;
	cmd[3] = rcs->expand.s_len;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);
	if (rcs->expand.s_len > 0) {
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, rcs->expand.s_str,
			      rcs->expand.s_len)) {
			return (false);
		}
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filescan_rcs_update_rcs_delta(struct filescan_args *fsa,
			      struct rcslib_file *rcs)
{
	const struct hash_args *hashops = fsa->fsa_hash_ops;
	struct rcslib_revision *rev;
	struct rcsnum *num;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len, i, j;

	SetDWord(cmd, rcs->delta.rd_count);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);

	/* delta */
	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		rev = &rcs->delta.rd_rev[i];

		len = rev->num.n_len + hashops->length + 4;
		if (len > fsa->fsa_cmdmax)
			return (false);

		/* num */
		SetWord(cmd, len - 2);
		cmd[2] = FILECMP_UPDATE_RCS_DELTA;
		cmd[3] = rev->num.n_len;
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
			return (false);
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, rev->num.n_str,
			      rev->num.n_len)) {
			return (false);
		}

		if (!(*hashops->init)(&fsa->fsa_hash_ctx))
			return (false);

		/* date */
		(*hashops->update)(fsa->fsa_hash_ctx, rev->date.rd_num.n_str,
				   rev->date.rd_num.n_len);
		/* author */
		(*hashops->update)(fsa->fsa_hash_ctx, rev->author.i_id,
				   rev->author.i_len);
		/* state */
		if (rev->state.i_len > 0) {
			(*hashops->update)(fsa->fsa_hash_ctx, rev->state.i_id,
					   rev->state.i_len);
		}
		/* branches */
		for (j = 0 ; j < rev->branches.rb_count ; j++) {
			num = &rev->branches.rb_num[j];
			(*hashops->update)(fsa->fsa_hash_ctx, num->n_str,
					   num->n_len);
		}
		/* next */
		if (rev->next.n_len > 0) {
			(*hashops->update)(fsa->fsa_hash_ctx, rev->next.n_str,
					   rev->next.n_len);
		}

		(*hashops->final)(fsa->fsa_hash_ctx, cmd);

		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, hashops->length))
			return (false);
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filescan_rcs_update_rcs_deltatext(struct filescan_args *fsa,
				  struct rcslib_file *rcs)
{
	const struct hash_args *hashops = fsa->fsa_hash_ops;
	struct rcslib_revision *rev;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len, i;

	SetDWord(cmd, rcs->delta.rd_count);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
		return (false);

	/* deltatext */
	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		rev = &rcs->delta.rd_rev[i];

		len = rev->num.n_len + hashops->length + 4;
		if (len > fsa->fsa_cmdmax)
			return (false);

		/* num */
		SetWord(cmd, len - 2);
		cmd[2] = FILECMP_UPDATE_RCS_DELTATEXT;
		cmd[3] = rev->num.n_len;
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 4))
			return (false);
		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, rev->num.n_str,
			      rev->num.n_len)) {
			return (false);
		}

		if (!(*hashops->init)(&fsa->fsa_hash_ctx))
			return (false);

		/* log */
		if (rev->log.s_len > 0) {
			(*hashops->update)(fsa->fsa_hash_ctx, rev->log.s_str,
					   rev->log.s_len);
		}
		/* text */
		if (rev->text.s_len > 0) {
			(*hashops->update)(fsa->fsa_hash_ctx, rev->text.s_str,
					   rev->text.s_len);
		}

		(*hashops->final)(fsa->fsa_hash_ctx, cmd);

		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, hashops->length))
			return (false);
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filescan_rcs_update_symlink(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len;

	if ((len = cap->ca_namelen + 6) >= fsa->fsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_UPDATE;
	cmd[3] = FILETYPE_SYMLINK;
	SetWord(&cmd[4], cap->ca_namelen);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 6))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	return (true);
}

bool
filescan_rcs_replace(struct filescan_args *fsa)
{
	struct cvsync_attr *cap = &fsa->fsa_attr;
	struct stat st;
	size_t pathlen;

	if ((lstat(fsa->fsa_path, &st) != -1) || (errno != ENOENT))
		return (false);

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if (!filescan_rcs_remove(fsa))
			return (false);
		if (!filescan_rcs_add(fsa))
			return (false);
		break;
	case FILETYPE_RCS:
		pathlen = fsa->fsa_pathlen + cap->ca_namelen;
		if (!cvsync_rcs_insert_attic(fsa->fsa_path, pathlen,
					     fsa->fsa_pathmax)) {
			return (false);
		}
		if (lstat(fsa->fsa_path, &st) == 0) {
			pathlen += 6;
			if (!cvsync_rcs_remove_attic(fsa->fsa_path, pathlen))
				return (false);
			if (!filescan_rcs_attic(fsa))
				return (false);
		} else {
			if (errno != ENOENT)
				return (false);
			if (!filescan_rcs_remove(fsa))
				return (false);
			pathlen += 6;
			if (!cvsync_rcs_remove_attic(fsa->fsa_path, pathlen))
				return (false);
			if (!filescan_rcs_add(fsa))
				return (false);
		}
		break;
	case FILETYPE_RCS_ATTIC:
		pathlen = fsa->fsa_pathlen + cap->ca_namelen + 6;
		if (!cvsync_rcs_remove_attic(fsa->fsa_path, pathlen))
			return (false);
		if (lstat(fsa->fsa_path, &st) == 0) {
			pathlen -= 6;
			if (!cvsync_rcs_insert_attic(fsa->fsa_path, pathlen,
						     fsa->fsa_pathmax)) {
				return (false);
			}
			if (!filescan_rcs_attic(fsa))
				return (false);
		} else {
			if (errno != ENOENT)
				return (false);
			if (!filescan_rcs_remove(fsa))
				return (false);
			pathlen -= 6;
			if (!cvsync_rcs_insert_attic(fsa->fsa_path, pathlen,
						     fsa->fsa_pathmax)) {
				return (false);
			}
			if (!filescan_rcs_add(fsa))
				return (false);
		}
		break;
	default:
		return (false);
	}

	return (true);
}
