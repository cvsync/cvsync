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

#include <stdlib.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_dirent.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "attribute.h"
#include "cvsync.h"
#include "filetypes.h"
#include "logmsg.h"
#include "mdirent.h"

extern pthread_mutex_t mdirent_mtx;

bool mopendir_rcs_unlink(char *, size_t, size_t, struct mdirent_rcs *);

struct mDIR *
mopendir_rcs(char *path, size_t pathlen, size_t pathmax,
	     struct mdirent_args *aux)
{
	struct mDIR *mdirp;
	struct mdirent_rcs *mdp = NULL;
	DIR *dirp;
	struct dirent *dp;
	char *rpath = &path[pathlen];
	size_t rpathmax = pathmax - pathlen, namelen, max = 0, n = 0, sv_n;
	int errormode = aux->mda_errormode;
	bool have_attic = false;

	if ((dirp = opendir(path)) == NULL) {
		if (errno != EACCES) {
			logmsg_err("%s: %s", path, strerror(errno));
			return (NULL);
		}
		goto done;
	}

	if (pthread_mutex_lock(&mdirent_mtx) != 0) {
		closedir(dirp);
		return (NULL);
	}

	for (;;) {
		errno = 0;
		if ((dp = readdir(dirp)) == NULL) {
			if (errno == 0)
				break;
			logmsg_err("%s: %s", path, strerror(errno));
			pthread_mutex_unlock(&mdirent_mtx);
			closedir(dirp);
			return (NULL);
		}
		namelen = DIRENT_NAMLEN(dp);
		if ((namelen == 0) || (namelen > CVSYNC_NAME_MAX) ||
		    (rpathmax < namelen)) {
			pthread_mutex_unlock(&mdirent_mtx);
			closedir(dirp);
			return (NULL);
		}
		if (IS_DIR_CURRENT(dp->d_name, namelen) ||
		    IS_DIR_PARENT(dp->d_name, namelen)) {
			continue;
		}
		if (IS_DIR_ATTIC(dp->d_name, namelen))
			have_attic = true;
		else
			max++;
	}

	if (pthread_mutex_unlock(&mdirent_mtx) != 0) {
		closedir(dirp);
		return (NULL);
	}

	rewinddir(dirp);

	if (max > 0) {
		if ((mdp = malloc(max * sizeof(*mdp))) == NULL) {
			logmsg_err("%s", strerror(errno));
			closedir(dirp);
			return (NULL);
		}

		if (pthread_mutex_lock(&mdirent_mtx) != 0) {
			free(mdp);
			closedir(dirp);
			return (NULL);
		}

		n = 0;
		while (n < max) {
			errno = 0;
			if ((dp = readdir(dirp)) == NULL) {
				if (errno == 0)
					break;
				logmsg_err("%s: %s", path, strerror(errno));
				pthread_mutex_unlock(&mdirent_mtx);
				free(mdp);
				closedir(dirp);
				return (NULL);
			}
			namelen = DIRENT_NAMLEN(dp);
			if (IS_DIR_ATTIC(dp->d_name, namelen) ||
			    IS_DIR_CURRENT(dp->d_name, namelen) ||
			    IS_DIR_PARENT(dp->d_name, namelen) ||
			    IS_FILE_CVSLOCK(dp->d_name, namelen)) {
				continue;
			}
			(void)memcpy(rpath, dp->d_name, namelen);
			rpath[namelen] = '\0';
			if (aux->mda_remove &&
			    IS_FILE_TMPFILE(dp->d_name, namelen)) {
				if (unlink(path) == 0)
					continue;
				if (errno == ENOENT)
					continue;
				logmsg_err("%s: %s", path, strerror(errno));
				pthread_mutex_unlock(&mdirent_mtx);
				free(mdp);
				closedir(dirp);
				return (NULL);
			}
			if (lstat(path, &mdp[n].md_stat) == -1) {
				logmsg_err("%s: %s", path, strerror(errno));
				pthread_mutex_unlock(&mdirent_mtx);
				free(mdp);
				closedir(dirp);
				return (NULL);
			}
			if (!S_ISDIR(mdp[n].md_stat.st_mode) &&
			    !S_ISREG(mdp[n].md_stat.st_mode) &&
			    !S_ISLNK(mdp[n].md_stat.st_mode)) {
				pthread_mutex_unlock(&mdirent_mtx);
				free(mdp);
				closedir(dirp);
				return (NULL);
			}
			if (aux->mda_symfollow) {
				if (stat(path, &mdp[n].md_stat) == -1) {
					if (errno == ENOENT)
						continue;
					logmsg_err("%s: %s", path,
						   strerror(errno));
					pthread_mutex_unlock(&mdirent_mtx);
					free(mdp);
					closedir(dirp);
					return (NULL);
				}
				if (!S_ISDIR(mdp[n].md_stat.st_mode) &&
				    !S_ISREG(mdp[n].md_stat.st_mode)) {
					continue;
				}
			}
			if (RCS_MODE(mdp[n].md_stat.st_mode, 0) == 0)
				continue;
			(void)memcpy(mdp[n].md_name, dp->d_name, namelen);
			mdp[n].md_namelen = namelen;
			mdp[n].md_attic = false;
			mdp[n].md_dead = false;
			n++;
		}

		if (pthread_mutex_unlock(&mdirent_mtx) != 0) {
			free(mdp);
			closedir(dirp);
			return (NULL);
		}
	}

	if (closedir(dirp) == -1) {
		logmsg_err("%s: %s", path, strerror(errno));
		if (mdp != NULL)
			free(mdp);
		return (NULL);
	}

	if (have_attic) {
		if (rpathmax <= 6) {
			if (mdp != NULL)
				free(mdp);
			return (NULL);
		}
		(void)memcpy(rpath, "Attic/", 6);
		rpath += 6;
		rpathmax -= 6;
		rpath[0] = '\0';

		if ((dirp = opendir(path)) == NULL) {
			logmsg_err("%s: %s", path, strerror(errno));
			if (mdp != NULL)
				free(mdp);
			return (NULL);
		}

		if (pthread_mutex_lock(&mdirent_mtx) != 0) {
			if (mdp != NULL)
				free(mdp);
			closedir(dirp);
			return (NULL);
		}

		sv_n = n;
		n = 0;
		for (;;) {
			errno = 0;
			if ((dp = readdir(dirp)) == NULL) {
				if (errno == 0)
					break;
				logmsg_err("%s: %s", path, strerror(errno));
				pthread_mutex_unlock(&mdirent_mtx);
				if (mdp != NULL)
					free(mdp);
				closedir(dirp);
				return (NULL);
			}
			namelen = DIRENT_NAMLEN(dp);
			if ((namelen == 0) || (namelen > CVSYNC_NAME_MAX) ||
			    (rpathmax < namelen)) {
				pthread_mutex_unlock(&mdirent_mtx);
				if (mdp != NULL)
					free(mdp);
				closedir(dirp);
				return (NULL);
			}
			if (IS_DIR_CURRENT(dp->d_name, namelen) ||
			    IS_DIR_PARENT(dp->d_name, namelen) ||
			    IS_FILE_TMPFILE(dp->d_name, namelen)) {
				continue;
			}
			if (!IS_FILE_RCS(dp->d_name, namelen)) {
				logmsg_err("Found an invalid file in Attic: "
					   "%s%.*s", path, namelen,
					   dp->d_name);
				if (errormode == CVSYNC_ERRORMODE_ABORT) {
					pthread_mutex_unlock(&mdirent_mtx);
					if (mdp != NULL)
						free(mdp);
					closedir(dirp);
					return (NULL);
				}
				continue;
			}
			n++;
		}

		if (pthread_mutex_unlock(&mdirent_mtx) != 0) {
			if (mdp != NULL)
				free(mdp);
			closedir(dirp);
			return (NULL);
		}

		if (n == 0) {
			n = sv_n;
		} else {
			struct mdirent_rcs *newp;

			rewinddir(dirp);

			max += n;
			if ((newp = malloc(max * sizeof(*newp))) == NULL) {
				logmsg_err("%s", strerror(errno));
				if (mdp != NULL)
					free(mdp);
				closedir(dirp);
				return (NULL);
			}
			n = sv_n;
			if (mdp != NULL) {
				if (n > 0) {
					(void)memcpy(newp, mdp,
						     n * sizeof(*newp));
				}
				free(mdp);
			}
			mdp = newp;

			if (pthread_mutex_lock(&mdirent_mtx) != 0) {
				free(mdp);
				closedir(dirp);
				return (NULL);
			}

			while (n < max) {
				errno = 0;
				if ((dp = readdir(dirp)) == NULL) {
					if (errno == 0)
						break;
					logmsg_err("%s: %s", path,
						   strerror(errno));
					pthread_mutex_unlock(&mdirent_mtx);
					free(mdp);
					closedir(dirp);
					return (NULL);
				}
				namelen = DIRENT_NAMLEN(dp);
				if (IS_DIR_CURRENT(dp->d_name, namelen) ||
				    IS_DIR_PARENT(dp->d_name, namelen)) {
					continue;
				}
				(void)memcpy(rpath, dp->d_name, namelen);
				rpath[namelen] = '\0';
				if (aux->mda_remove &&
				    !IS_FILE_RCS(dp->d_name, namelen)) {
					if (unlink(path) == 0)
						continue;
					if (errno == ENOENT)
						continue;
					logmsg_err("%s: %s", path,
						   strerror(errno));
					pthread_mutex_unlock(&mdirent_mtx);
					free(mdp);
					closedir(dirp);
					return (NULL);
				}
				if (lstat(path, &mdp[n].md_stat) == -1) {
					logmsg_err("%s: %s", path,
						   strerror(errno));
					pthread_mutex_unlock(&mdirent_mtx);
					free(mdp);
					closedir(dirp);
					return (NULL);
				}
				if (!S_ISREG(mdp[n].md_stat.st_mode))
					continue;
				(void)memcpy(mdp[n].md_name, dp->d_name,
					     namelen);
				mdp[n].md_namelen = namelen;
				mdp[n].md_attic = true;
				mdp[n].md_dead = false;
				n++;
			}

			if (pthread_mutex_unlock(&mdirent_mtx) != 0) {
				free(mdp);
				closedir(dirp);
				return (NULL);
			}
		}

		if (closedir(dirp) == -1) {
			logmsg_err("%s: %s", path, strerror(errno));
			free(mdp);
			return (NULL);
		}
	}

	if (n == 0) {
		if (mdp != NULL)
			free(mdp);
		mdp = NULL;
	}

done:
	if ((mdirp = malloc(sizeof(*mdirp))) == NULL) {
		logmsg_err("%s", strerror(errno));
		if (mdp != NULL)
			free(mdp);
		return (NULL);
	}
	mdirp->m_entries = mdp;
	mdirp->m_nentries = n;
	mdirp->m_offset = 0;
	mdirp->m_parent = NULL;
	mdirp->m_parent_pathlen = 0;

	path[pathlen] = '\0';

	if (mdirp->m_nentries > 1) {
		qsort(mdirp->m_entries, mdirp->m_nentries, sizeof(*mdp),
		      malphasort);
		n = 0;
		while (n < mdirp->m_nentries - 1) {
			struct mdirent_rcs *m1 = &mdp[n], *m2 = &mdp[n + 1];
			struct mdirent_rcs *m;

			if (malphasort(m1, m2) != 0) {
				n++;
				continue;
			}

			logmsg_err("Found inconsistency: %.*s in %s",
				   m1->md_namelen, m1->md_name, path);

			if (errormode == CVSYNC_ERRORMODE_ABORT) {
				logmsg_err("Please set 'errormode' to 'fixup' "
					   "or 'ignore' and try again");
				free(mdirp->m_entries);
				free(mdirp);
				return (NULL);
			}

			n += 2;

			if (m1->md_stat.st_mtime < m2->md_stat.st_mtime)
				m = m1;
			else
				m = m2;
			m->md_dead = true;

			if (errormode == CVSYNC_ERRORMODE_IGNORE)
				continue;

			if (!mopendir_rcs_unlink(path, pathlen, pathmax, m)) {
				free(mdirp->m_entries);
				free(mdirp);
				return (NULL);
			}

			path[pathlen] = '\0';
		}
	}

	return (mdirp);
}

bool
mopendir_rcs_unlink(char *path, size_t pathlen, size_t pathmax,
		    struct mdirent_rcs *mdp)
{
	char *rpath = &path[pathlen];
	size_t rpathlen = pathlen;

	if (mdp->md_attic) {
		if (pathlen + 6 >= pathmax) {
			logmsg_err("%s", strerror(EINVAL));
			return (false);
		}
		(void)memcpy(rpath, "Attic/", 6);
		rpath += 6;
		rpathlen += 6;
	}
	if (rpathlen + mdp->md_namelen >= pathmax) {
		logmsg_err("%s", strerror(EINVAL));
		return (false);
	}
	(void)memcpy(rpath, mdp->md_name, mdp->md_namelen);
	rpath[mdp->md_namelen] = '\0';

	if (unlink(path) == -1) {
		if (errno != ENOENT) {
			logmsg_err("%s: %s", path, strerror(errno));
			return (false);
		}
	}

	if (mdp->md_attic) {
		path[pathlen + 6] = '\0';
		if (rmdir(path) == -1) {
			if ((errno != ENOENT) && (errno != EEXIST) &&
			    (errno != ENOTEMPTY)) {
				logmsg_err("%s: %s", path, strerror(errno));
				return (false);
			}
		}
	}

	return (true);
}
