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
#include "filetypes.h"
#include "logmsg.h"
#include "mdirent.h"
#include "mux.h"
#include "scanfile.h"

#include "dircmp.h"
#include "filescan.h"

bool dircmp_fetch(struct dircmp_args *);
bool dircmp_close(struct dircmp_args *);

struct dircmp_args *
dircmp_init(struct mux *mx, const char *hostinfo, struct collection *cls,
	    uint32_t proto)
{
	struct dircmp_args *dca;

	if ((dca = malloc(sizeof(*dca))) == NULL) {
		logmsg_err("%s DirCmp Init: %s", hostinfo, strerror(errno));
		return (NULL);
	}
	dca->dca_mux = mx;
	dca->dca_hostinfo = hostinfo;
	dca->dca_collections = cls;
	dca->dca_proto = proto;
	dca->dca_pathmax = sizeof(dca->dca_path);
	dca->dca_namemax = CVSYNC_NAME_MAX;
	dca->dca_cmdmax = sizeof(dca->dca_cmd);

	return (dca);
}

void
dircmp_destroy(struct dircmp_args *dca)
{
	free(dca);
}

void *
dircmp(void *arg)
{
	struct dircmp_args *dca = arg;
	struct collection *cl;
	int type;

	for (cl = dca->dca_collections ; cl != NULL ; cl = cl->cl_next) {
		if (!dircmp_fetch(dca)) {
			logmsg_err("%s DirCmp: Fetch Error",
				   dca->dca_hostinfo);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (dca->dca_tag != DIRCMP_START) {
			logmsg_err("%s DirCmp: %02x: Invalid Tag",
				   dca->dca_hostinfo, dca->dca_tag);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		time(&cl->cl_tick);

		if ((strcasecmp(cl->cl_name, dca->dca_name) != 0) ||
		    (strcasecmp(cl->cl_release, dca->dca_release) != 0)) {
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		type = cvsync_release_pton(cl->cl_release);
		if (type == CVSYNC_RELEASE_UNKNOWN) {
			logmsg_err("%s DirCmp: %s: Release Error",
				   dca->dca_hostinfo, cl->cl_release);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (strlen(cl->cl_dist_name) != 0) {
			cl->cl_distfile = distfile_open(cl->cl_dist_name);
			if (cl->cl_distfile == NULL) {
				logmsg_err("%s DirCmp: %s: Distfile Error",
					   dca->dca_hostinfo,
					   cl->cl_dist_name);
				mux_abort(dca->dca_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
		}
		if (strlen(cl->cl_scan_name) != 0) {
			cl->cl_scanfile = scanfile_open(cl->cl_scan_name);
			if (cl->cl_scanfile == NULL) {
				logmsg_err("%s DirCmp: %s: Scanfile Error "
					   "(ignored)", dca->dca_hostinfo,
					   cl->cl_scan_name);
			}
		}

		if (!dircmp_start(dca, cl->cl_name, cl->cl_release)) {
			logmsg_err("%s DirCmp: Initializer Error",
				   dca->dca_hostinfo);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		dca->dca_pathlen = cl->cl_prefixlen;
		if (dca->dca_pathlen >= dca->dca_pathmax) {
			logmsg_err("%s DirCmp: Initializer Error",
				   dca->dca_hostinfo);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
		(void)memcpy(dca->dca_path, cl->cl_prefix, dca->dca_pathlen);
		dca->dca_path[dca->dca_pathlen] = '\0';
		dca->dca_rpath = dca->dca_path + dca->dca_pathlen;
		dca->dca_rpathlen = dca->dca_pathlen;
		dca->dca_collection = cl;

		switch (type) {
		case CVSYNC_RELEASE_LIST:
			if (!dircmp_fetch(dca)) {
				logmsg_err("%s DirCmp: Fetch Error",
					   dca->dca_hostinfo);
				mux_abort(dca->dca_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			if (dca->dca_tag != DIRCMP_END) {
				logmsg_err("%s DirCmp: LIST Error",
					   dca->dca_hostinfo);
				mux_abort(dca->dca_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		case CVSYNC_RELEASE_RCS:
			if (!dircmp_rcs(dca)) {
				logmsg_err("%s DirCmp: RCS Error",
					   dca->dca_hostinfo);
				mux_abort(dca->dca_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		default:
			logmsg_err("%s DirCmp: Release Error",
				   dca->dca_hostinfo);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!dircmp_end(dca)) {
			logmsg_err("%s DirCmp: Collection Finalizer Error",
				   dca->dca_hostinfo);
			mux_abort(dca->dca_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (cl->cl_scanfile != NULL) {
			scanfile_close(cl->cl_scanfile);
			cl->cl_scanfile = NULL;
		}
	}

	if (!dircmp_fetch(dca)) {
		logmsg_err("%s DirCmp: Fetch Error", dca->dca_hostinfo);
		mux_abort(dca->dca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (dca->dca_tag != DIRCMP_END) {
		logmsg_err("%s DirCmp: %02x: Invalid Tag", dca->dca_hostinfo,
			   dca->dca_tag);
		mux_abort(dca->dca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!dircmp_end(dca)) {
		logmsg_err("%s DirCmp: Finalizer Error", dca->dca_hostinfo);
		mux_abort(dca->dca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!dircmp_close(dca)) {
		logmsg_err("%s DirCmp: Finalizer Error", dca->dca_hostinfo);
		mux_abort(dca->dca_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	return (CVSYNC_THREAD_SUCCESS);
}

bool
dircmp_fetch(struct dircmp_args *dca)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len, namelen, relnamelen;

	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > dca->dca_cmdmax - 2))
		return (false);
	dca->dca_tag = cmd[2];

	switch (dca->dca_tag) {
	case DIRCMP_START:
		if (len < 3)
			return (false);

		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd, 2))
			return (false);

		namelen = cmd[0];
		if ((namelen == 0) || (namelen > dca->dca_namemax))
			return (false);
		relnamelen = cmd[1];
		if ((namelen == 0) || (relnamelen > dca->dca_namemax))
			return (false);
		if (len != namelen + relnamelen + 3)
			return (false);

		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, dca->dca_name,
			      namelen)) {
			return (false);
		}
		dca->dca_name[namelen] = '\0';
		if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, dca->dca_release,
			      relnamelen)) {
			return (false);
		}
		dca->dca_release[relnamelen] = '\0';

		break;
	case DIRCMP_END:
		if (len != 1)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

bool
dircmp_start(struct dircmp_args *dca, const char *name, const char *relname)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len, namelen, relnamelen;

	namelen = strlen(name);
	if ((namelen == 0) || (namelen > dca->dca_namemax))
		return (false);
	relnamelen = strlen(relname);
	if ((relnamelen == 0) || (relnamelen > dca->dca_namemax))
		return (false);

	if ((len = namelen + relnamelen + 5) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_START;
	cmd[3] = namelen;
	cmd[4] = relnamelen;
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 5))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, name, namelen))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, relname, relnamelen))
		return (false);

	if (!mux_flush(dca->dca_mux, MUX_FILESCAN))
		return (false);

	return (true);
}

bool
dircmp_end(struct dircmp_args *dca)
{
	static const uint8_t _cmd[3] = { 0x00, 0x01, FILESCAN_END };

	if (!mux_send(dca->dca_mux, MUX_FILESCAN, _cmd, sizeof(_cmd)))
		return (false);

	return (true);
}

bool
dircmp_close(struct dircmp_args *dca)
{
	if (!mux_close_out(dca->dca_mux, MUX_FILESCAN))
		return (false);
	return (mux_close_in(dca->dca_mux, MUX_DIRCMP_IN));
}

bool
dircmp_access(struct dircmp_args *dca, void *aux)
{
	struct distfile_args *da = dca->dca_collection->cl_distfile;
	struct mdirent *mdp = aux;
	size_t len;

	if (mdp->md_dead || IS_FILE_TMPFILE(mdp->md_name, mdp->md_namelen))
		return (false);
	if ((da == NULL) || (da->da_size == 0))
		return (true);

	if ((len = dca->dca_pathlen + mdp->md_namelen) >= dca->dca_pathmax)
		return (false);
	(void)memcpy(&dca->dca_path[dca->dca_pathlen], mdp->md_name,
		     mdp->md_namelen);
	if (S_ISDIR(mdp->md_stat.st_mode)) {
		dca->dca_path[len] = '\0';
		if (distfile_access(da, dca->dca_rpath) == DISTFILE_DENY)
			return (false);
		dca->dca_path[len] = '/';
		if (++len >= dca->dca_pathmax)
			return (false);
	}
	dca->dca_path[len] = '\0';

	return (distfile_access(da, dca->dca_rpath) != DISTFILE_DENY);
}

bool
dircmp_access_scanfile(struct dircmp_args *dca, struct scanfile_attr *attr)
{
	struct distfile_args *da = dca->dca_collection->cl_distfile;
	size_t len;

	if ((da == NULL) || (da->da_size == 0))
		return (true);

	if ((len = dca->dca_pathlen + attr->a_namelen) >= dca->dca_pathmax)
		return (false);
	(void)memcpy(&dca->dca_path[dca->dca_pathlen], attr->a_name,
		     attr->a_namelen);
	if (attr->a_type == FILETYPE_DIR) {
		dca->dca_path[len] = '\0';
		if (distfile_access(da, dca->dca_rpath) == DISTFILE_DENY)
			return (false);
		dca->dca_path[len] = '/';
		if (++len >= dca->dca_pathmax)
			return (false);
	}
	dca->dca_path[len] = '\0';

	return (distfile_access(da, dca->dca_rpath) != DISTFILE_DENY);
}
