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

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdlib.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_unistd.h"

#include "attribute.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "list.h"
#include "logmsg.h"
#include "mdirent.h"
#include "scanfile.h"

struct scanfile_rcs_args {
	struct scanfile_args	sra_scanfile;
	char			sra_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*sra_rpath;
	char			sra_rprefix[PATH_MAX];
	size_t			sra_pathlen, sra_pathmax, sra_rprefixlen;
	struct mdirent_args	*sra_mdirent_args;
	mode_t			sra_umask;

	uint8_t			sra_aux[CVSYNC_MAXAUXLEN];
	size_t			sra_auxmax;
};

struct scanfile_rcs_args *scanfile_rcs_init(struct scanfile_create_args *);
void scanfile_rcs_destroy(struct scanfile_rcs_args *);
bool scanfile_rcs_dir(struct scanfile_rcs_args *, struct mdirent_rcs *);
bool scanfile_rcs_file(struct scanfile_rcs_args *, struct mdirent_rcs *);
bool scanfile_rcs_symlink(struct scanfile_rcs_args *, struct mdirent_rcs *);

struct mDIR *scanfile_rcs_opendir(struct scanfile_rcs_args *, size_t);

struct scanfile_rcs_args *
scanfile_rcs_init(struct scanfile_create_args *sca)
{
	struct scanfile_rcs_args *sra;
	struct stat st;

	if ((sra = malloc(sizeof(*sra))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	scanfile_init(&sra->sra_scanfile);
	sra->sra_scanfile.sa_scanfile_name = sca->sca_name;
	if (!scanfile_create_tmpfile(&sra->sra_scanfile, sca->sca_mode)) {
		free(sra);
		return (false);
	}

	if (stat(sca->sca_prefix, &st) == -1) {
		logmsg_err("%s: %s", sca->sca_prefix, strerror(errno));
		scanfile_remove_tmpfile(&sra->sra_scanfile);
		free(sra);
		return (NULL);
	}
	if (!S_ISDIR(st.st_mode)) {
		logmsg_err("%s: %s", sca->sca_prefix, strerror(ENOTDIR));
		scanfile_remove_tmpfile(&sra->sra_scanfile);
		free(sra);
		return (NULL);
	}
	if (realpath(sca->sca_prefix, sra->sra_path) == NULL) {
		logmsg_err("%s: %s", sca->sca_prefix, strerror(errno));
		scanfile_remove_tmpfile(&sra->sra_scanfile);
		free(sra);
		return (NULL);
	}

	sra->sra_pathmax = sizeof(sra->sra_path);
	sra->sra_pathlen = strlen(sra->sra_path) + 1;
	if (sra->sra_pathlen >= sra->sra_pathmax) {
		logmsg_err("%s: %s", sca->sca_prefix, strerror(ENAMETOOLONG));
		scanfile_remove_tmpfile(&sra->sra_scanfile);
		free(sra);
		return (NULL);
	}
	if ((sra->sra_rprefixlen = sca->sca_rprefixlen) >= sra->sra_pathmax) {
		logmsg_err("%.*s: %s", sca->sca_rprefixlen, sca->sca_rprefix,
			   strerror(ENAMETOOLONG));
		scanfile_remove_tmpfile(&sra->sra_scanfile);
		free(sra);
		return (NULL);
	}
	sra->sra_path[sra->sra_pathlen - 1] = '/';
	sra->sra_path[sra->sra_pathlen] = '\0';
	sra->sra_rpath = &sra->sra_path[sra->sra_pathlen];
	if (sra->sra_rprefixlen > 0) {
		(void)memcpy(sra->sra_rprefix, sca->sca_rprefix,
			     sca->sca_rprefixlen);
	}
	sra->sra_mdirent_args = sca->sca_mdirent_args;
	sra->sra_umask = sca->sca_umask;

	sra->sra_auxmax = sizeof(sra->sra_aux);

	return (sra);
}

void
scanfile_rcs_destroy(struct scanfile_rcs_args *sra)
{
	if (strlen(sra->sra_scanfile.sa_tmp_name) != 0)
		scanfile_remove_tmpfile(&sra->sra_scanfile);
	free(sra);
}

bool
scanfile_rcs(struct scanfile_create_args *sca)
{
	struct scanfile_rcs_args *sra;
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp, *entries;
	struct list *lp;
	size_t len;

	if ((sra = scanfile_rcs_init(sca)) == NULL)
		return (false);
	sra->sra_scanfile.sa_changed = true;

	if ((lp = list_init()) == NULL) {
		scanfile_rcs_destroy(sra);
		return (false);
	}
	list_set_destructor(lp, mclosedir);

	if ((mdirp = scanfile_rcs_opendir(sra, sra->sra_pathlen)) == NULL) {
		list_destroy(lp);
		scanfile_rcs_destroy(sra);
		return (false);
	}
	mdirp->m_parent_pathlen = sra->sra_pathlen;

	if (!list_insert_tail(lp, mdirp)) {
		mclosedir(mdirp);
		list_destroy(lp);
		scanfile_rcs_destroy(sra);
		return (false);
	}

	do {
		if ((mdirp = list_remove_tail(lp)) == NULL) {
			list_destroy(lp);
			scanfile_rcs_destroy(sra);
			return (false);
		}

		while (mdirp->m_offset < mdirp->m_nentries) {
			if (cvsync_isinterrupted()) {
				mclosedir(mdirp);
				list_destroy(lp);
				scanfile_rcs_destroy(sra);
				return (false);
			}

			entries = mdirp->m_entries;
			mdp = &entries[mdirp->m_offset++];
			if (mdp->md_dead)
				continue;

			switch (mdp->md_stat.st_mode & S_IFMT) {
			case S_IFDIR:
				if (!scanfile_rcs_dir(sra, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					scanfile_rcs_destroy(sra);
					return (false);
				}

				len = sra->sra_pathlen + mdp->md_namelen + 1;
				if (len >= sra->sra_pathmax) {
					mclosedir(mdirp);
					list_destroy(lp);
					scanfile_rcs_destroy(sra);
					return (false);
				}
				(void)memcpy(&sra->sra_path[sra->sra_pathlen],
					     mdp->md_name, mdp->md_namelen);
				sra->sra_path[len - 1] = '/';
				sra->sra_path[len] = '\0';

				if (!list_insert_tail(lp, mdirp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					scanfile_rcs_destroy(sra);
					return (false);
				}

				mdirp = scanfile_rcs_opendir(sra, len);
				if (mdirp == NULL) {
					list_destroy(lp);
					scanfile_rcs_destroy(sra);
					return (false);
				}
				mdirp->m_parent = mdp;
				mdirp->m_parent_pathlen = sra->sra_pathlen;

				sra->sra_pathlen = len;

				break;
			case S_IFREG:
				if (!scanfile_rcs_file(sra, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					scanfile_rcs_destroy(sra);
					return (false);
				}
				break;
			case S_IFLNK:
				if (!scanfile_rcs_symlink(sra, mdp)) {
					mclosedir(mdirp);
					list_destroy(lp);
					scanfile_rcs_destroy(sra);
					return (false);
				}
				break;
			default:
				mclosedir(mdirp);
				list_destroy(lp);
				scanfile_rcs_destroy(sra);
				return (false);
			}
		}

		sra->sra_pathlen = mdirp->m_parent_pathlen;
		sra->sra_path[sra->sra_pathlen] = '\0';

		mclosedir(mdirp);
	} while (!list_isempty(lp));

	list_destroy(lp);

	if (!scanfile_rename(&sra->sra_scanfile)) {
		scanfile_rcs_destroy(sra);
		return (false);
	}

	scanfile_rcs_destroy(sra);

	return (true);
}

bool
scanfile_rcs_dir(struct scanfile_rcs_args *sra, struct mdirent_rcs *mdp)
{
	struct scanfile_args *sa = &sra->sra_scanfile;
	struct scanfile_attr *attr = &sa->sa_attr;
	uint16_t mode = RCS_MODE(mdp->md_stat.st_mode, sra->sra_umask);
	size_t rpathlen, namelen, auxlen;

	rpathlen = sra->sra_pathlen - (size_t)(sra->sra_rpath - sra->sra_path);
	if ((namelen = rpathlen + mdp->md_namelen) >= sra->sra_pathmax)
		return (false);
	(void)memcpy(&sra->sra_rpath[rpathlen], mdp->md_name, mdp->md_namelen);

	if ((auxlen = attr_rcs_encode_dir(sra->sra_aux, sra->sra_auxmax,
					  mode)) == 0) {
		return (false);
	}

	attr->a_type = FILETYPE_DIR;
	attr->a_name = sra->sra_rpath;
	attr->a_namelen = namelen;
	attr->a_aux = sra->sra_aux;
	attr->a_auxlen = auxlen;

	if (!scanfile_write_attr(sa, attr))
		return (false);

	return (true);
}

bool
scanfile_rcs_file(struct scanfile_rcs_args *sra, struct mdirent_rcs *mdp)
{
	struct scanfile_args *sa = &sra->sra_scanfile;
	struct scanfile_attr *attr = &sa->sa_attr;
	struct stat *st = &mdp->md_stat;
	uint16_t mode = RCS_MODE(st->st_mode, sra->sra_umask);
	size_t rpathlen, namelen, auxlen;

	rpathlen = sra->sra_pathlen - (size_t)(sra->sra_rpath - sra->sra_path);
	if ((namelen = rpathlen + mdp->md_namelen) >= sra->sra_pathmax)
		return (false);
	(void)memcpy(&sra->sra_rpath[rpathlen], mdp->md_name, mdp->md_namelen);
	if (IS_FILE_RCS(mdp->md_name, mdp->md_namelen)) {
		if (mdp->md_attic)
			attr->a_type = FILETYPE_RCS_ATTIC;
		else
			attr->a_type = FILETYPE_RCS;
		if ((auxlen = attr_rcs_encode_rcs(sra->sra_aux,
						  sra->sra_auxmax,
						  st->st_mtime, mode)) == 0) {
			return (false);
		}
	} else {
		attr->a_type = FILETYPE_FILE;
		if ((auxlen = attr_rcs_encode_file(sra->sra_aux,
						   sra->sra_auxmax,
						   st->st_mtime, st->st_size,
						   mode)) == 0) {
			return (false);
		}
	}
	attr->a_name = sra->sra_rpath;
	attr->a_namelen = namelen;
	attr->a_aux = sra->sra_aux;
	attr->a_auxlen = auxlen;

	if (!scanfile_write_attr(sa, attr))
		return (false);

	return (true);
}

bool
scanfile_rcs_symlink(struct scanfile_rcs_args *sra, struct mdirent_rcs *mdp)
{
	struct scanfile_args *sa = &sra->sra_scanfile;
	struct scanfile_attr *attr = &sa->sa_attr;
	size_t rpathlen, namelen;
	ssize_t auxlen;

	rpathlen = sra->sra_pathlen - (size_t)(sra->sra_rpath - sra->sra_path);
	if ((namelen = rpathlen + mdp->md_namelen) >= sra->sra_pathmax)
		return (false);
	(void)memcpy(&sra->sra_rpath[rpathlen], mdp->md_name, mdp->md_namelen);
	sra->sra_rpath[rpathlen + mdp->md_namelen] = '\0';
	if ((auxlen = readlink(sra->sra_path, (char *)sra->sra_aux,
			       sra->sra_auxmax)) == -1) {
		logmsg_err("%s: %s", sra->sra_path, strerror(errno));
		return (false);
	}
	attr->a_type = FILETYPE_SYMLINK;
	attr->a_name = sra->sra_rpath;
	attr->a_namelen = namelen;
	attr->a_aux = sra->sra_aux;
	attr->a_auxlen = (size_t)auxlen;

	if (!scanfile_write_attr(sa, attr))
		return (false);

	return (true);
}

struct mDIR *
scanfile_rcs_opendir(struct scanfile_rcs_args *sra, size_t pathlen)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp;
	size_t rpathlen, len;
	int rv;

	rpathlen = pathlen - (size_t)(sra->sra_rpath - sra->sra_path);
	if (rpathlen >= sra->sra_rprefixlen) {
		if ((mdirp = mopendir_rcs(sra->sra_path, pathlen,
					  sra->sra_pathmax,
					  sra->sra_mdirent_args)) == NULL) {
			return (NULL);
		}

		return (mdirp);
	}

	if ((mdp = malloc(sizeof(*mdp))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	for (len = rpathlen ; len < sra->sra_rprefixlen ; len++) {
		if (sra->sra_rprefix[len] == '/')
			break;
	}
	if ((len -= rpathlen) >= sizeof(mdp->md_name)) {
		free(mdp);
		return (NULL);
	}
	(void)memcpy(mdp->md_name, &sra->sra_rprefix[rpathlen], len);
	mdp->md_namelen = len;
	mdp->md_attic = false;
	mdp->md_dead = false;

	if ((len = pathlen + mdp->md_namelen) >= sra->sra_pathmax) {
		free(mdp);
		return (NULL);
	}
	(void)memcpy(&sra->sra_path[pathlen], &sra->sra_rprefix[rpathlen],
		     mdp->md_namelen);
	sra->sra_path[len] = '\0';

	if (sra->sra_mdirent_args->mda_symfollow)
		rv = stat(sra->sra_path, &mdp->md_stat);
	else
		rv = lstat(sra->sra_path, &mdp->md_stat);
	if (rv == -1) {
		free(mdp);
		if (errno != ENOENT) {
			logmsg_err("%s: %s", sra->sra_path, strerror(errno));
			return (NULL);
		}
		mdp = NULL;
	} else {
		if (!S_ISDIR(mdp->md_stat.st_mode)) {
			logmsg_err("%s: %s", sra->sra_path, strerror(ENOTDIR));
			free(mdp);
			return (NULL);
		}
	}

	if ((mdirp = malloc(sizeof(*mdirp))) == NULL) {
		logmsg_err("%s", strerror(errno));
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
