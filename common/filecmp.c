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

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_strings.h"
#include "basedef.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "distfile.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"

#include "filecmp.h"
#include "updater.h"

bool filecmp_fetch(struct filecmp_args *);
bool filecmp_close(struct filecmp_args *);

struct filecmp_args *
filecmp_init(struct mux *mx, struct collection *list, struct collection *cls,
	     const char *hostinfo, uint32_t proto, int type)
{
	struct filecmp_args *fca;

	if ((fca = malloc(sizeof(*fca))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	fca->fca_mux = mx;
	fca->fca_hostinfo = hostinfo;
	fca->fca_collections_list = list;
	fca->fca_collections = cls;
	fca->fca_proto = proto;
	fca->fca_pathmax = sizeof(fca->fca_path);
	fca->fca_namemax = CVSYNC_NAME_MAX;
	fca->fca_cmdmax = sizeof(fca->fca_cmd);

	if (!hash_set(type, &fca->fca_hash_ops)) {
		free(fca);
		return (NULL);
	}

	return (fca);
}

void
filecmp_destroy(struct filecmp_args *fca)
{
	free(fca);
}

void *
filecmp(void *arg)
{
	struct filecmp_args *fca = arg;
	struct collection *cl;
	int type;

	for (cl = fca->fca_collections ; cl != NULL ; cl = cl->cl_next) {
		if (!filecmp_fetch(fca)) {
			logmsg_err("%s FileCmp: Fetch Error",
				   fca->fca_hostinfo);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (fca->fca_tag != FILECMP_START) {
			logmsg_err("%s FileCmp: %02x: Invalid Tag",
				   fca->fca_hostinfo, fca->fca_tag);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if ((strcasecmp(cl->cl_name, fca->fca_name) != 0) ||
		    (strcasecmp(cl->cl_release, fca->fca_release) != 0)) {
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		type = cvsync_release_pton(cl->cl_release);
		if (type == CVSYNC_RELEASE_UNKNOWN) {
			logmsg_err("%s FileCmp: %s: Release Error",
				   fca->fca_hostinfo, cl->cl_release);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!filecmp_start(fca, cl->cl_name, cl->cl_release)) {
			logmsg_err("%s FileCmp: Initializer Error",
				   fca->fca_hostinfo);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		fca->fca_pathlen = cl->cl_prefixlen;
		if (fca->fca_pathlen >= fca->fca_pathmax) {
			logmsg_err("%s FileCmp: Initializer Error",
				   fca->fca_hostinfo);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
		(void)memcpy(fca->fca_path, cl->cl_prefix, fca->fca_pathlen);
		fca->fca_path[fca->fca_pathlen] = '\0';
		fca->fca_rpath = fca->fca_path + fca->fca_pathlen;
		fca->fca_rpathlen = fca->fca_pathlen;
		fca->fca_collection = cl;
		fca->fca_umask = cl->cl_umask;

		switch (type) {
		case CVSYNC_RELEASE_LIST:
			if (!filecmp_list(fca)) {
				logmsg_err("%s FileCmp: LIST Error",
					   fca->fca_hostinfo);
				mux_abort(fca->fca_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		case CVSYNC_RELEASE_RCS:
			if (!filecmp_rcs(fca)) {
				logmsg_err("%s FileCmp: RCS Error",
					   fca->fca_hostinfo);
				mux_abort(fca->fca_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		default:
			logmsg_err("%s FileCmp: Release Error",
				   fca->fca_hostinfo);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!filecmp_end(fca)) {
			logmsg_err("%s FileCmp: Collection Finalizer Error",
				   fca->fca_hostinfo);
			mux_abort(fca->fca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (cl->cl_distfile != NULL) {
			distfile_close(cl->cl_distfile);
			cl->cl_distfile = NULL;
		}

		logmsg("%s Collection %s/%s (time=%ds)", fca->fca_hostinfo,
		       cl->cl_name, cl->cl_release, time(NULL) - cl->cl_tick);
	}

	if (!filecmp_fetch(fca)) {
		logmsg_err("%s FileCmp: Fetch Error", fca->fca_hostinfo);
		mux_abort(fca->fca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (fca->fca_tag != FILECMP_END) {
		logmsg_err("%s FileCmp: %02x: Invalid Tag", fca->fca_hostinfo,
			   fca->fca_tag);
		mux_abort(fca->fca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!filecmp_end(fca)) {
		logmsg_err("%s FileCmp: Finalizer Error", fca->fca_hostinfo);
		mux_abort(fca->fca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!filecmp_close(fca)) {
		logmsg_err("%s FileCmp: Finalizer Error", fca->fca_hostinfo);
		mux_abort(fca->fca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	return (CVSYNC_THREAD_SUCCESS);
}

bool
filecmp_fetch(struct filecmp_args *fca)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len, namelen, relnamelen;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > fca->fca_cmdmax - 2))
		return (false);
	fca->fca_tag = cmd[2];

	switch (fca->fca_tag) {
	case FILECMP_START:
		if (len < 3)
			return (false);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 2))
			return (false);
		if ((namelen = cmd[0]) > fca->fca_namemax)
			return (false);
		if ((relnamelen = cmd[1]) > fca->fca_namemax)
			return (false);
		if (len != namelen + relnamelen + 3)
			return (false);

		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, fca->fca_name,
			      namelen)) {
			return (false);
		}
		fca->fca_name[namelen] = '\0';

		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, fca->fca_release,
			      relnamelen)) {
			return (false);
		}
		fca->fca_release[relnamelen] = '\0';

		break;
	case FILECMP_END:
		if (len != 1)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

bool
filecmp_start(struct filecmp_args *fca, const char *name, const char *relname)
{
	uint8_t *cmd = fca->fca_cmd;
	size_t len, namelen, relnamelen;

	if (((namelen = strlen(name)) > fca->fca_namemax) ||
	    ((relnamelen = strlen(relname)) > fca->fca_namemax)) {
		return (false);
	}

	if ((len = namelen + relnamelen + 5) > fca->fca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = UPDATER_START;
	cmd[3] = namelen;
	cmd[4] = relnamelen;
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, name, namelen))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, relname, relnamelen))
		return (false);

	if (!mux_flush(fca->fca_mux, MUX_UPDATER))
		return (false);

	return (true);
}

bool
filecmp_end(struct filecmp_args *fca)
{
	uint8_t *cmd = fca->fca_cmd;

	SetWord(cmd, 1);
	cmd[2] = UPDATER_END;

	return (mux_send(fca->fca_mux, MUX_UPDATER, cmd, 3));
}

bool
filecmp_close(struct filecmp_args *fca)
{
	if (!mux_close_out(fca->fca_mux, MUX_UPDATER))
		return (false);
	return (mux_close_in(fca->fca_mux, MUX_FILECMP_IN));
}

int
filecmp_access(struct filecmp_args *fca, struct cvsync_attr *cap)
{
	struct distfile_args *da = fca->fca_collection->cl_distfile;
	struct stat st;
	size_t len;
	int err;

	if ((da == NULL) || (da->da_size == 0))
		return (DISTFILE_ALLOW);

	if ((len = fca->fca_pathlen + cap->ca_namelen) >= fca->fca_pathmax)
		return (DISTFILE_DENY);
	(void)memcpy(&fca->fca_path[fca->fca_pathlen], cap->ca_name,
		     cap->ca_namelen);
	fca->fca_path[len] = '\0';

	if (fca->fca_collection->cl_symfollow)
		err = stat(fca->fca_path, &st);
	else
		err = lstat(fca->fca_path, &st);
	if (err == -1) {
		if (errno != ENOENT) {
			logmsg_err("%s FileCmp: access error: %s",
				   fca->fca_hostinfo, strerror(errno));
		}
		return (DISTFILE_DENY);
	}
	if (S_ISDIR(st.st_mode)) {
		if (distfile_access(da, fca->fca_rpath) == DISTFILE_DENY)
			return (DISTFILE_DENY);
		fca->fca_path[len] = '/';
		if (++len >= fca->fca_pathmax)
			return (DISTFILE_DENY);
		fca->fca_path[len] = '\0';
	}

	return (distfile_access(da, fca->fca_rpath));
}
