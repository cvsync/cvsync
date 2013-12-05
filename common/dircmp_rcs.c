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

#include "dircmp.h"
#include "filescan.h"

bool dircmp_rcs_fetch(struct dircmp_args *);

bool dircmp_rcs_add(struct dircmp_args *, struct mdirent_rcs *);
bool dircmp_rcs_add_dir(struct dircmp_args *, struct mdirent_rcs *);
bool dircmp_rcs_add_file(struct dircmp_args *, struct mdirent_rcs *);
bool dircmp_rcs_add_symlink(struct dircmp_args *, struct mdirent_rcs *);
bool dircmp_rcs_remove(struct dircmp_args *);
bool dircmp_rcs_remove_dir(struct dircmp_args *);
bool dircmp_rcs_remove_file(struct dircmp_args *);
bool dircmp_rcs_replace(struct dircmp_args *, struct mdirent_rcs *);
bool dircmp_rcs_update(struct dircmp_args *, struct mdirent_rcs *);

struct mDIR *dircmp_rcs_opendir(struct dircmp_args *, size_t);

bool
dircmp_rcs(struct dircmp_args *dca)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp, *entries;
	struct cvsync_attr *cap = &dca->dca_attr;
	struct list *lp;
	int rv;
	bool fetched;

	if (dca->dca_collection->cl_scanfile != NULL)
		return (dircmp_rcs_scanfile(dca));

	if ((lp = list_init()) == NULL)
		return (false);
	list_set_destructor(lp, mclosedir);

	if ((mdirp = dircmp_rcs_opendir(dca, dca->dca_pathlen)) == NULL) {
		list_destroy(lp);
		return (false);
	}
	mdirp->m_parent_pathlen = dca->dca_pathlen;

	fetched = false;

	for (;;) {
		if (!fetched) {
			if (!dircmp_rcs_fetch(dca)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
			fetched = true;
		}

		if (cap->ca_tag == DIRCMP_END)
			break;

		if (cap->ca_tag == DIRCMP_UP) {
			while (mdirp->m_offset < mdirp->m_nentries) {
				entries = mdirp->m_entries;
				mdp = &entries[mdirp->m_offset++];
				if (!dircmp_access(dca, mdp))
					continue;
				if (!dircmp_rcs_add(dca, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}
			}
			dca->dca_pathlen = mdirp->m_parent_pathlen;
			dca->dca_path[dca->dca_pathlen] = '\0';
			mclosedir(mdirp);

			if ((mdirp = list_remove_tail(lp)) == NULL) {
				list_destroy(lp);
				return (false);
			}
			mdirp->m_offset++;

			fetched = false;
			continue;
		}

		if (mdirp->m_offset == mdirp->m_nentries) {
			if (!dircmp_rcs_remove(dca)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}

			fetched = false;
			continue;
		}

		entries = mdirp->m_entries;
		mdp = &entries[mdirp->m_offset];
		if (!dircmp_access(dca, mdp)) {
			mdirp->m_offset++;
			continue;
		}

		if (mdp->md_namelen == cap->ca_namelen) {
			rv = memcmp(mdp->md_name, cap->ca_name,
				    cap->ca_namelen);
		} else {
			if (mdp->md_namelen < cap->ca_namelen) {
				rv = memcmp(mdp->md_name, cap->ca_name,
					    mdp->md_namelen);
			} else {
				rv = memcmp(mdp->md_name, cap->ca_name,
					    cap->ca_namelen);
			}
			if (rv == 0) {
				if (mdp->md_namelen < cap->ca_namelen)
					rv = -1;
				else
					rv = 1;
			}
		}
		if (rv == 0) {
			if (!dircmp_rcs_update(dca, mdp)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
			if (cap->ca_tag != DIRCMP_DOWN) {
				mdirp->m_offset++;
			} else {
				size_t len;

				if (!list_insert_tail(lp, mdirp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}

				len = dca->dca_pathlen + mdp->md_namelen + 1;
				if (len >= dca->dca_pathmax) {
					list_destroy(lp);
					return (false);
				}
				(void)memcpy(&dca->dca_path[dca->dca_pathlen],
					     mdp->md_name, mdp->md_namelen);
				dca->dca_path[len - 1] = '/';
				dca->dca_path[len] = '\0';

				mdirp = dircmp_rcs_opendir(dca, len);
				if (mdirp == NULL) {
					list_destroy(lp);
					return (false);
				}
				mdirp->m_parent_pathlen = dca->dca_pathlen;
				dca->dca_pathlen = len;
			}
			fetched = false;
		} else if (rv < 0) {
			if (!dircmp_rcs_add(dca, mdp)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
			mdirp->m_offset++;
		} else { /* rv > 0 */
			if (!dircmp_rcs_remove(dca)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
			fetched = false;
		}
	}

	for (;;) {
		while (mdirp->m_offset < mdirp->m_nentries) {
			entries = mdirp->m_entries;
			mdp = &entries[mdirp->m_offset++];
			if (!dircmp_access(dca, mdp))
				continue;

			if (!dircmp_rcs_add(dca, mdp)) {
				mclosedir(mdirp);
				list_destroy(lp);
				return (false);
			}
		}
		mclosedir(mdirp);

		if (list_isempty(lp))
			break;

		if ((mdirp = list_remove_tail(lp)) == NULL) {
			list_destroy(lp);
			return (false);
		}
	}

	list_destroy(lp);

	return (true);
}

bool
dircmp_rcs_fetch(struct dircmp_args *dca)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	uint8_t *cmd = dca->dca_cmd;
	size_t len;

	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > dca->dca_cmdmax - 2))
		return (false);
	if ((cap->ca_tag = cmd[2]) == DIRCMP_END)
		return (len == 1);
	if (cap->ca_tag == DIRCMP_UP) {
		cap->ca_type = FILETYPE_DIR;
		return (len == 1);
	}
	if (len < 2)
		return (false);

	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd, 1))
		return (false);
	cap->ca_namelen = cmd[0];

	switch (cap->ca_tag) {
	case DIRCMP_DOWN:
		cap->ca_type = FILETYPE_DIR;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name)) ||
		    (cap->ca_namelen != len - RCS_ATTRLEN_DIR - 2)) {
			return (false);
		}
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!cvsync_rcs_filename((char *)cap->ca_name,
					 cap->ca_namelen)) {
			return (false);
		}
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd,
			      RCS_ATTRLEN_DIR)) {
			return (false);
		}
		if (!attr_rcs_decode_dir(cmd, RCS_ATTRLEN_DIR, cap))
			return (false);
		break;
	case DIRCMP_FILE:
		cap->ca_type = FILETYPE_FILE;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name)) ||
		    (cap->ca_namelen != len - RCS_ATTRLEN_FILE - 2)) {
			return (false);
		}
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!cvsync_rcs_filename((char *)cap->ca_name,
					 cap->ca_namelen)) {
			return (false);
		}
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd,
			      RCS_ATTRLEN_FILE)) {
			return (false);
		}
		if (!attr_rcs_decode_file(cmd, RCS_ATTRLEN_FILE, cap))
			return (false);
		break;
	case DIRCMP_RCS:
	case DIRCMP_RCS_ATTIC:
		if (cap->ca_tag == DIRCMP_RCS)
			cap->ca_type = FILETYPE_RCS;
		else
			cap->ca_type = FILETYPE_RCS_ATTIC;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name)) ||
		    (cap->ca_namelen != len - RCS_ATTRLEN_RCS - 2)) {
			return (false);
		}
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!cvsync_rcs_filename((char *)cap->ca_name,
					 cap->ca_namelen)) {
			return (false);
		}
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd,
			      RCS_ATTRLEN_RCS)) {
			return (false);
		}
		if (!attr_rcs_decode_rcs(cmd, RCS_ATTRLEN_RCS, cap))
			return (false);
		break;
	case DIRCMP_SYMLINK:
		cap->ca_type = FILETYPE_SYMLINK;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name))) {
			return (false);
		}
		if (len <= cap->ca_namelen + 2)
			return (false);
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_name,
			      cap->ca_namelen)) {
			return (false);
		}
		if (!cvsync_rcs_filename((char *)cap->ca_name,
					 cap->ca_namelen)) {
			return (false);
		}
		cap->ca_auxlen = len - cap->ca_namelen - 2;
		if (cap->ca_auxlen > sizeof(cap->ca_aux))
			return (false);
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_aux,
			      cap->ca_auxlen)) {
			return (false);
		}
		break;
	default:
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_add(struct dircmp_args *dca, struct mdirent_rcs *mdp)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *entries;
	struct list *lp;
	struct stat *st = &mdp->md_stat;
	size_t len;

	if (S_ISREG(st->st_mode))
		return (dircmp_rcs_add_file(dca, mdp));
	if (S_ISLNK(st->st_mode))
		return (dircmp_rcs_add_symlink(dca, mdp));

	if (!S_ISDIR(st->st_mode))
		return (false);
	if (!dircmp_rcs_add_dir(dca, mdp))
		return (false);

	if ((lp = list_init()) == NULL)
		return (false);
	list_set_destructor(lp, mclosedir);

	len = dca->dca_pathlen + mdp->md_namelen + 1;
	if (len >= dca->dca_pathmax) {
		list_destroy(lp);
		return (false);
	}

	(void)memcpy(&dca->dca_path[dca->dca_pathlen], mdp->md_name,
		     mdp->md_namelen);
	dca->dca_path[len - 1] = '/';
	dca->dca_path[len] = '\0';

	if ((mdirp = dircmp_rcs_opendir(dca, len)) == NULL) {
		list_destroy(lp);
		return (false);
	}
	mdirp->m_parent_pathlen = dca->dca_pathlen;
	dca->dca_pathlen = len;

	if (!list_insert_tail(lp, mdirp)) {
		mclosedir(mdirp);
		list_destroy(lp);
		return (false);
	}

	do {
		if ((mdirp = list_remove_tail(lp)) == NULL) {
			list_destroy(lp);
			return (false);
		}

		while (mdirp->m_offset < mdirp->m_nentries) {
			entries = mdirp->m_entries;
			mdp = &entries[mdirp->m_offset++];
			if (!dircmp_access(dca, mdp))
				continue;

			switch (mdp->md_stat.st_mode & S_IFMT) {
			case S_IFDIR:
				if (!dircmp_rcs_add_dir(dca, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}

				len = dca->dca_pathlen + mdp->md_namelen + 1;
				if (len >= dca->dca_pathmax) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}
				(void)memcpy(&dca->dca_path[dca->dca_pathlen],
					     mdp->md_name, mdp->md_namelen);
				dca->dca_path[len - 1] = '/';
				dca->dca_path[len] = '\0';

				if (!list_insert_tail(lp, mdirp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}

				mdirp = dircmp_rcs_opendir(dca, len);
				if (mdirp == NULL) {
					list_destroy(lp);
					return (false);
				}
				mdirp->m_parent_pathlen = dca->dca_pathlen;
				dca->dca_pathlen = len;

				break;
			case S_IFREG:
				if (!dircmp_rcs_add_file(dca, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					return (false);
				}
				break;
			case S_IFLNK:
				if (!dircmp_rcs_add_symlink(dca, mdp)) {
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

		dca->dca_pathlen = mdirp->m_parent_pathlen;
		dca->dca_path[dca->dca_pathlen] = '\0';

		mclosedir(mdirp);
	} while (!list_isempty(lp));

	list_destroy(lp);

	return (true);
}

bool
dircmp_rcs_add_dir(struct dircmp_args *dca, struct mdirent_rcs *mdp)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len, rlen;

	if ((len = dca->dca_pathlen + mdp->md_namelen) >= dca->dca_pathmax)
		return (false);
	rlen = dca->dca_pathlen - dca->dca_rpathlen;
	if ((len = rlen + mdp->md_namelen + 6) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_ADD;
	cmd[3] = FILETYPE_DIR;
	SetWord(&cmd[4], rlen + mdp->md_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_rpath, rlen))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, mdp->md_name,
		      mdp->md_namelen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_add_file(struct dircmp_args *dca, struct mdirent_rcs *mdp)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len, rlen;

	if ((len = dca->dca_pathlen + mdp->md_namelen) >= dca->dca_pathmax)
		return (false);
	rlen = dca->dca_pathlen - dca->dca_rpathlen;
	if ((len = rlen + mdp->md_namelen + 6) > dca->dca_cmdmax)
		return (false);

	if (IS_FILE_RCS(mdp->md_name, mdp->md_namelen)) {
		if (mdp->md_attic)
			cmd[3] = FILETYPE_RCS_ATTIC;
		else
			cmd[3] = FILETYPE_RCS;
	} else {
		cmd[3] = FILETYPE_FILE;
	}

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_ADD;
	SetWord(&cmd[4], rlen + mdp->md_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_rpath, rlen))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, mdp->md_name,
		      mdp->md_namelen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_add_symlink(struct dircmp_args *dca, struct mdirent_rcs *mdp)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len, rlen;

	if ((len = dca->dca_pathlen + mdp->md_namelen) >= dca->dca_pathmax)
		return (false);
	rlen = dca->dca_pathlen - dca->dca_rpathlen;
	if ((len = rlen + mdp->md_namelen + 6) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_ADD;
	cmd[3] = FILETYPE_SYMLINK;
	SetWord(&cmd[4], rlen + mdp->md_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_rpath, rlen))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, mdp->md_name,
		      mdp->md_namelen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_remove(struct dircmp_args *dca)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	size_t len, *plen_stack, c, n;

	if (cap->ca_type != FILETYPE_DIR)
		return (dircmp_rcs_remove_file(dca));

	n = 4;
	if ((plen_stack = malloc(n * sizeof(*plen_stack))) == NULL)
		return (false);
	plen_stack[0] = dca->dca_pathlen;
	c = 1;

	len = dca->dca_pathlen + cap->ca_namelen + 1;
	if (len >= dca->dca_pathmax) {
		free(plen_stack);
		return (false);
	}

	(void)memcpy(&dca->dca_path[dca->dca_pathlen], cap->ca_name,
		     cap->ca_namelen);
	dca->dca_path[len - 1] = '/';
	dca->dca_path[len] = '\0';
	dca->dca_pathlen = len;

	do {
		if (!dircmp_rcs_fetch(dca)) {
			free(plen_stack);
			return (false);
		}

		switch (cap->ca_tag) {
		case DIRCMP_END:
			free(plen_stack);
			return (false);
		case DIRCMP_DOWN:
			if (c == n) {
				size_t *newp, old = n, new = old * 2;

				newp = malloc(new * sizeof(*newp));
				if (newp == NULL) {
					free(plen_stack);
					return (false);
				}
				(void)memcpy(newp, plen_stack,
					     old * sizeof(*newp));
				(void)memset(&newp[old], 0,
					     (new - old) * sizeof(*newp));

				free(plen_stack);
				plen_stack = newp;
				n = new;
			}
			plen_stack[c++] = dca->dca_pathlen;

			len = dca->dca_pathlen + cap->ca_namelen + 1;
			if (len >= dca->dca_pathmax) {
				free(plen_stack);
				return (false);
			}

			(void)memcpy(&dca->dca_path[dca->dca_pathlen],
				     cap->ca_name, cap->ca_namelen);
			dca->dca_path[len - 1] = '/';
			dca->dca_path[len] = '\0';
			dca->dca_pathlen = len;

			break;
		case DIRCMP_UP:
			dca->dca_path[dca->dca_pathlen - 1] = '\0';

			if (!dircmp_rcs_remove_dir(dca)) {
				free(plen_stack);
				return (false);
			}

			dca->dca_pathlen = plen_stack[--c];
			dca->dca_path[dca->dca_pathlen] = '\0';

			break;
		case DIRCMP_FILE:
		case DIRCMP_RCS:
		case DIRCMP_RCS_ATTIC:
		case DIRCMP_SYMLINK:
			if (!dircmp_rcs_remove_file(dca)) {
				free(plen_stack);
				return (false);
			}
			break;
		default:
			free(plen_stack);
			return (false);
		}
	} while (c > 0);

	free(plen_stack);

	return (true);
}

bool
dircmp_rcs_remove_dir(struct dircmp_args *dca)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len, rlen;

	rlen = dca->dca_pathlen - dca->dca_rpathlen - 1;
	if ((len = rlen + 6) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_REMOVE;
	cmd[3] = FILETYPE_DIR;
	SetWord(&cmd[4], rlen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_rpath, rlen))
		return (false);

	return (true);
}

bool
dircmp_rcs_remove_file(struct dircmp_args *dca)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	uint8_t *cmd = dca->dca_cmd;
	size_t len, rlen;

	if ((len = dca->dca_pathlen + cap->ca_namelen) >= dca->dca_pathmax)
		return (false);
	rlen = dca->dca_pathlen - dca->dca_rpathlen;
	if ((len = rlen + cap->ca_namelen + 6) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_REMOVE;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], rlen + cap->ca_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_rpath, rlen))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_replace(struct dircmp_args *dca, struct mdirent_rcs *mdp)
{
	switch (mdp->md_stat.st_mode & S_IFMT) {
	case S_IFDIR:
	case S_IFREG:
	case S_IFLNK:
		if (!dircmp_rcs_remove(dca))
			return (false);
		if (!dircmp_rcs_add(dca, mdp))
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_update(struct dircmp_args *dca, struct mdirent_rcs *mdp)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	struct stat *st = &mdp->md_stat;
	uint16_t mode = RCS_MODE(st->st_mode, dca->dca_collection->cl_umask);
	uint8_t *cmd = dca->dca_cmd;
	size_t base, len, rlen;
	int wn;

	if ((len = dca->dca_pathlen + mdp->md_namelen) >= dca->dca_pathmax)
		return (false);
	rlen = dca->dca_pathlen - dca->dca_rpathlen;
	if ((base = rlen + mdp->md_namelen + 6) > dca->dca_cmdmax)
		return (false);

	len = dca->dca_cmdmax - base;

	switch (st->st_mode & S_IFMT) {
	case S_IFDIR:
		if (cap->ca_type != FILETYPE_DIR)
			return (dircmp_rcs_replace(dca, mdp));
		if (mode == cap->ca_mode)
			return (true);
		cmd[2] = FILESCAN_SETATTR;
		cmd[3] = FILETYPE_DIR;
		if ((len = attr_rcs_encode_dir(&cmd[6], len, mode)) == 0)
			return (false);
		break;
	case S_IFREG:
		if (IS_FILE_RCS(mdp->md_name, mdp->md_namelen)) {
			if ((cap->ca_type != FILETYPE_RCS) &&
			    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
				return (dircmp_rcs_replace(dca, mdp));
			}
			if (mdp->md_attic)
				cmd[3] = FILETYPE_RCS_ATTIC;
			else
				cmd[3] = FILETYPE_RCS;
			if ((cmd[3] == cap->ca_type) &&
			    ((int64_t)st->st_mtime == cap->ca_mtime) &&
			    (mode == cap->ca_mode)) {
				return (true);
			}
			if (cap->ca_type != cmd[3]) {
				cmd[2] = FILESCAN_RCS_ATTIC;
			} else {
				if ((int64_t)st->st_mtime != cap->ca_mtime)
					cmd[2] = FILESCAN_UPDATE;
				else
					cmd[2] = FILESCAN_SETATTR;
			}
			if ((len = attr_rcs_encode_rcs(&cmd[6], len,
						       st->st_mtime,
						       mode)) == 0) {
				return (false);
			}
		} else {
			if (cap->ca_type != FILETYPE_FILE)
				return (dircmp_rcs_replace(dca, mdp));
			if (((int64_t)st->st_mtime == cap->ca_mtime) &&
			    ((uint64_t)st->st_size == cap->ca_size) &&
			    (mode == cap->ca_mode)) {
				return (true);
			}
			if (((int64_t)st->st_mtime != cap->ca_mtime) ||
			    ((uint64_t)st->st_size != cap->ca_size)) {
				if (st->st_size == 0)
					return (dircmp_rcs_replace(dca, mdp));

				cmd[2] = FILESCAN_UPDATE;
			} else {
				cmd[2] = FILESCAN_SETATTR;
			}
			cmd[3] = FILETYPE_FILE;
			if ((len = attr_rcs_encode_file(&cmd[6], len,
							st->st_mtime,
							st->st_size,
							mode)) == 0) {
				return (false);
			}
		}
		break;
	case S_IFLNK:
		if (cap->ca_type != FILETYPE_SYMLINK)
			return (dircmp_rcs_replace(dca, mdp));
		(void)memcpy(&dca->dca_path[dca->dca_pathlen], mdp->md_name,
			     mdp->md_namelen);
		dca->dca_path[dca->dca_pathlen + mdp->md_namelen] = '\0';
		if ((wn = readlink(dca->dca_path, dca->dca_symlink,
				   dca->dca_pathmax)) == -1) {
			return (false);
		}
		len = (size_t)wn;
		if ((len == cap->ca_auxlen) &&
		    (memcmp(cap->ca_aux, dca->dca_symlink, len) == 0)) {
			return (true);
		}
		cmd[2] = FILESCAN_UPDATE;
		cmd[3] = FILETYPE_SYMLINK;
		len = 0;
		break;
	default:
		return (false);
	}

	SetWord(cmd, len + base - 2);
	SetWord(&cmd[4], rlen + mdp->md_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_rpath, rlen))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, mdp->md_name,
		      mdp->md_namelen)) {
		return (false);
	}
	if (len > 0) {
		if (!mux_send(dca->dca_mux, MUX_FILESCAN, &cmd[6], len))
			return (false);
	}

	return (true);
}

struct mDIR *
dircmp_rcs_opendir(struct dircmp_args *dca, size_t pathlen)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp;
	struct mdirent_args mda;
	struct collection *cl = dca->dca_collection;
	size_t rpathlen, len;
	int rv;

	mda.mda_errormode = cl->cl_errormode;
	mda.mda_symfollow = cl->cl_symfollow;
	mda.mda_remove = false;

	rpathlen = pathlen - (size_t)(dca->dca_rpath - dca->dca_path);
	if (rpathlen >= cl->cl_rprefixlen) {
		if ((mdirp = mopendir_rcs(dca->dca_path, pathlen,
					  dca->dca_pathmax, &mda)) == NULL) {
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
	if (len >= dca->dca_pathmax) {
		free(mdp);
		return (NULL);
	}
	(void)memcpy(&dca->dca_path[cl->cl_prefixlen], cl->cl_rprefix,
		     rpathlen + mdp->md_namelen);
	dca->dca_path[len] = '\0';

	if (cl->cl_symfollow)
		rv = stat(dca->dca_path, &mdp->md_stat);
	else
		rv = lstat(dca->dca_path, &mdp->md_stat);
	if (rv == -1) {
		free(mdp);
		return (NULL);
	}
	if (!S_ISDIR(mdp->md_stat.st_mode)) {
		free(mdp);
		return (NULL);
	}

	if ((mdirp = malloc(sizeof(*mdirp))) == NULL) {
		free(mdp);
		return (NULL);
	}
	mdirp->m_entries = mdp;
	mdirp->m_nentries = 1;
	mdirp->m_offset = 0;
	mdirp->m_parent = NULL;
	mdirp->m_parent_pathlen = 0;

	return (mdirp);
}
