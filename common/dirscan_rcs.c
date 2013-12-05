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

#include <stdlib.h>

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
#include "filetypes.h"
#include "list.h"
#include "mdirent.h"
#include "mux.h"

#include "dirscan.h"
#include "dircmp.h"

bool dirscan_rcs_down(struct dirscan_args *, struct mdirent_rcs *);
bool dirscan_rcs_up(struct dirscan_args *);
bool dirscan_rcs_file(struct dirscan_args *, struct mdirent_rcs *);
bool dirscan_rcs_symlink(struct dirscan_args *, struct mdirent_rcs *);

struct mDIR *dirscan_rcs_opendir(struct dirscan_args *, size_t);

bool
dirscan_rcs(struct dirscan_args *dsa)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp = NULL, *entries;
	struct collection *cl = dsa->dsa_collection;
	struct list *lp;
	size_t len;

	if (cl->cl_scanfile != NULL)
		return (dirscan_rcs_scanfile(dsa));

	if ((lp = list_init()) == NULL)
		return (false);
	list_set_destructor(lp, mclosedir);

	if ((mdirp = dirscan_rcs_opendir(dsa, dsa->dsa_pathlen)) == NULL) {
		list_destroy(lp);
		return (false);
	}
	mdirp->m_parent_pathlen = dsa->dsa_pathlen;

	if (!list_insert_tail(lp, mdirp)) {
		mclosedir(mdirp);
		list_destroy(lp);
		return (false);
	}

	do {
		if (cvsync_isinterrupted()) {
			list_destroy(lp);
			return (false);
		}

		if ((mdirp = list_remove_tail(lp)) == NULL) {
			list_destroy(lp);
			return (false);
		}

		while (mdirp->m_offset < mdirp->m_nentries) {
			entries = mdirp->m_entries;
			mdp = &entries[mdirp->m_offset++];
			if (mdp->md_dead)
				continue;

			switch (mdp->md_stat.st_mode & S_IFMT) {
			case S_IFDIR:
				if (!dirscan_rcs_down(dsa, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}

				len = dsa->dsa_pathlen + mdp->md_namelen + 1;
				if (len >= dsa->dsa_pathmax) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}
				(void)memcpy(&dsa->dsa_path[dsa->dsa_pathlen],
					     mdp->md_name, mdp->md_namelen);
				dsa->dsa_path[len - 1] = '/';
				dsa->dsa_path[len] = '\0';

				if (!list_insert_tail(lp, mdirp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}

				mdirp = dirscan_rcs_opendir(dsa, len);
				if (mdirp == NULL) {
					list_destroy(lp);
					return (false);
				}
				mdirp->m_parent = mdp;
				mdirp->m_parent_pathlen = dsa->dsa_pathlen;

				dsa->dsa_pathlen = len;

				break;
			case S_IFREG:
				if (!dirscan_rcs_file(dsa, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}
				break;
			case S_IFLNK:
				if (!dirscan_rcs_symlink(dsa, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}
				break;
			default:
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
		}

		if (mdirp->m_parent != NULL) {
			if (!dirscan_rcs_up(dsa)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
		}

		dsa->dsa_pathlen = mdirp->m_parent_pathlen;

		mclosedir(mdirp);
	} while (!list_isempty(lp));

	list_destroy(lp);

	return (true);
}

bool
dirscan_rcs_down(struct dirscan_args *dsa, struct mdirent_rcs *mdp)
{
	struct collection *cl = dsa->dsa_collection;
	uint16_t mode = RCS_MODE(mdp->md_stat.st_mode, cl->cl_umask);
	uint8_t *cmd = dsa->dsa_cmd;
	size_t base, len;

	if (mdp->md_namelen > dsa->dsa_namemax)
		return (false);
	if ((base = mdp->md_namelen + 4) > dsa->dsa_cmdmax)
		return (false);

	cmd[2] = DIRCMP_DOWN;
	cmd[3] = (uint8_t)mdp->md_namelen;
	if ((len = attr_rcs_encode_dir(&cmd[4], dsa->dsa_cmdmax - base,
				       mode)) == 0) {
		return (false);
	}
	SetWord(cmd, len + base - 2);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, cmd, 4))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, mdp->md_name, mdp->md_namelen))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, &cmd[4], len))
		return (false);

	return (true);
}

bool
dirscan_rcs_up(struct dirscan_args *dsa)
{
	static const uint8_t _cmd[3] = { 0x00, 0x01, DIRCMP_UP };

	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, _cmd, sizeof(_cmd)))
		return (false);

	return (true);
}

bool
dirscan_rcs_file(struct dirscan_args *dsa, struct mdirent_rcs *mdp)
{
	struct stat *st = &mdp->md_stat;
	uint16_t mode;
	uint8_t *cmd = dsa->dsa_cmd;
	size_t base, len;

	if (mdp->md_namelen > dsa->dsa_namemax)
		return (false);
	if ((base = mdp->md_namelen + 4) > dsa->dsa_cmdmax)
		return (false);

	mode = RCS_MODE(st->st_mode, dsa->dsa_collection->cl_umask);
	len = dsa->dsa_cmdmax - base;

	if (IS_FILE_RCS(mdp->md_name, mdp->md_namelen)) {
		if (mdp->md_attic)
			cmd[2] = DIRCMP_RCS_ATTIC;
		else
			cmd[2] = DIRCMP_RCS;
		len = attr_rcs_encode_rcs(&cmd[4], len, st->st_mtime, mode);
	} else {
		cmd[2] = DIRCMP_FILE;
		len = attr_rcs_encode_file(&cmd[4], len, st->st_mtime,
					   st->st_size, mode);
	}
	if (len == 0)
		return (false);

	SetWord(cmd, len + base - 2);
	cmd[3] = (uint8_t)mdp->md_namelen;
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, cmd, 4))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, mdp->md_name, mdp->md_namelen))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, &cmd[4], len))
		return (false);

	return (true);
}

bool
dirscan_rcs_symlink(struct dirscan_args *dsa, struct mdirent_rcs *mdp)
{
	uint8_t *cmd = dsa->dsa_cmd;
	size_t len;
	ssize_t auxlen;

	if (dsa->dsa_pathlen + mdp->md_namelen + 1 > dsa->dsa_pathmax)
		return (false);
	(void)memcpy(&dsa->dsa_path[dsa->dsa_pathlen], mdp->md_name,
		     mdp->md_namelen);
	dsa->dsa_path[dsa->dsa_pathlen + mdp->md_namelen] = '\0';

	if ((auxlen = readlink(dsa->dsa_path, dsa->dsa_symlink,
			       dsa->dsa_pathmax)) == -1) {
		return (false);
	}

	if ((len = mdp->md_namelen + (size_t)auxlen + 4) > dsa->dsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = DIRCMP_SYMLINK;
	cmd[3] = (uint8_t)mdp->md_namelen;
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, cmd, 4))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, mdp->md_name, mdp->md_namelen))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, dsa->dsa_symlink,
		      (size_t)auxlen)) {
		return (false);
	}

	return (true);
}

struct mDIR *
dirscan_rcs_opendir(struct dirscan_args *dsa, size_t pathlen)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp;
	struct mdirent_args mda;
	struct collection *cl = dsa->dsa_collection;
	size_t rpathlen, len;

	mda.mda_errormode = cl->cl_errormode;
	mda.mda_symfollow = false;
	mda.mda_remove = true;

	rpathlen = pathlen - (size_t)(dsa->dsa_rpath - dsa->dsa_path);
	if (rpathlen >= cl->cl_rprefixlen) {
		if ((mdirp = mopendir_rcs(dsa->dsa_path, pathlen,
					  dsa->dsa_pathmax, &mda)) == NULL) {
			return (NULL);
		}

		return (mdirp);
	}

	if ((mdp = malloc(sizeof(*mdp))) == NULL)
		return (NULL);

	for (len = rpathlen ; len < cl->cl_rprefixlen ; len++) {
		if (cl->cl_rprefix[len] == '/')
			break;
	}
	if ((len -= rpathlen) >= sizeof(mdp->md_name)) {
		free(mdp);
		return (NULL);
	}
	(void)memcpy(mdp->md_name, &cl->cl_rprefix[rpathlen], len);
	mdp->md_namelen = len;
	mdp->md_attic = false;
	mdp->md_dead = false;

	len = cl->cl_prefixlen + rpathlen + mdp->md_namelen;
	if (len >= dsa->dsa_pathmax) {
		free(mdp);
		return (NULL);
	}
	(void)memcpy(&dsa->dsa_path[cl->cl_prefixlen], cl->cl_rprefix,
		     rpathlen + mdp->md_namelen);
	dsa->dsa_path[len] = '\0';

	if (lstat(dsa->dsa_path, &mdp->md_stat) == -1) {
		free(mdp);
		mdp = NULL;
	} else {
		if (!S_ISDIR(mdp->md_stat.st_mode)) {
			free(mdp);
			return (NULL);
		}
	}

	if ((mdirp = malloc(sizeof(*mdirp))) == NULL) {
		if (mdp != NULL)
			free(mdp);
		return (NULL);
	}
	mdirp->m_entries = mdp;
	mdirp->m_nentries = (mdp != NULL) ? 1 : 0;
	mdirp->m_offset = 0;
	mdirp->m_parent = NULL;
	mdirp->m_parent_pathlen = 0;

	return (mdirp);
}
