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
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "scanfile.h"

#include "updater.h"

bool updater_fetch(struct updater_args *);
bool updater_close(struct updater_args *);

struct updater_args *
updater_init(struct mux *mx, struct collection *cls, uint32_t proto, int type)
{
	struct updater_args *uda;

	if ((uda = malloc(sizeof(*uda))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	uda->uda_mux = mx;
	uda->uda_proto = proto;
	uda->uda_collections = cls;
	uda->uda_pathmax = sizeof(uda->uda_path);
	uda->uda_namemax = CVSYNC_NAME_MAX;
	uda->uda_cmdmax = sizeof(uda->uda_cmd);

	uda->uda_bufsize = CVSYNC_BSIZE;
	if ((uda->uda_buffer = malloc(uda->uda_bufsize)) == NULL) {
		logmsg_err("%s", strerror(errno));
		free(uda);
		return (false);
	}

	if (!hash_set(type, &uda->uda_hash_ops)) {
		free(uda->uda_buffer);
		free(uda);
		return (NULL);
	}

	return (uda);
}

void
updater_destroy(struct updater_args *uda)
{
	free(uda->uda_buffer);
	free(uda);
}

void *
updater(void *arg)
{
	struct updater_args *uda = arg;
	struct collection *cl;

	for (cl = uda->uda_collections ; cl != NULL ; cl = cl->cl_next) {
		if (cl->cl_flags & CLFLAGS_DISABLE)
			continue;

		if (cvsync_isinterrupted()) {
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!updater_fetch(uda)) {
			logmsg_err("Updater: Fetch Error");
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (uda->uda_tag != UPDATER_START) {
			logmsg_err("Updater: invalid tag: %02x", uda->uda_tag);
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if ((strcasecmp(cl->cl_name, uda->uda_name) != 0) ||
		    (strcasecmp(cl->cl_release, uda->uda_release) != 0)) {
			logmsg_err("Updater: Collection Error");
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		uda->uda_pathlen = cl->cl_prefixlen;
		if (uda->uda_pathlen >= uda->uda_pathmax) {
			logmsg_err("Updater: Initializer Error");
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
		(void)memcpy(uda->uda_path, cl->cl_prefix, uda->uda_pathlen);
		uda->uda_path[uda->uda_pathlen] = '\0';
		uda->uda_rpath = &uda->uda_path[uda->uda_pathlen];
		uda->uda_scanfile = cl->cl_scanfile;
		uda->uda_umask = cl->cl_umask;

		cl->cl_scanfile = NULL;

		if (!scanfile_create_tmpfile(uda->uda_scanfile,
					     cl->cl_scan_mode)) {
			logmsg_err("Updater: Scanfile Error");
			scanfile_close(uda->uda_scanfile);
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		switch (cvsync_release_pton(cl->cl_release)) {
		case CVSYNC_RELEASE_LIST:
			if (!updater_list(uda)) {
				logmsg_err("Updater: LIST Error");
				scanfile_remove_tmpfile(uda->uda_scanfile);
				scanfile_close(uda->uda_scanfile);
				mux_abort(uda->uda_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		case CVSYNC_RELEASE_RCS:
			logmsg("Updating (collection %s/%s)", cl->cl_name,
			       cl->cl_release);
			if (!updater_rcs(uda)) {
				logmsg_err("Updater: RCS Error");
				scanfile_remove_tmpfile(uda->uda_scanfile);
				scanfile_close(uda->uda_scanfile);
				mux_abort(uda->uda_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			logmsg("Done (collection %s/%s)", cl->cl_name,
			       cl->cl_release);
			break;
		default:
			logmsg_err("Updater: Release Error");
			scanfile_remove_tmpfile(uda->uda_scanfile);
			scanfile_close(uda->uda_scanfile);
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!scanfile_rename(uda->uda_scanfile)) {
			logmsg_err("Updater: Scanfile Error");
			scanfile_remove_tmpfile(uda->uda_scanfile);
			scanfile_close(uda->uda_scanfile);
			mux_abort(uda->uda_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
	}

	if (!updater_fetch(uda)) {
		logmsg_err("Updater: Fetch Error");
		mux_abort(uda->uda_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (uda->uda_tag != UPDATER_END) {
		logmsg_err("Updater: invalid tag: %02x", uda->uda_tag);
		mux_abort(uda->uda_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!updater_close(uda)) {
		logmsg_err("Updater: Finalizer Error");
		mux_abort(uda->uda_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	return (CVSYNC_THREAD_SUCCESS);
}

bool
updater_fetch(struct updater_args *uda)
{
	uint8_t *cmd = uda->uda_cmd;
	size_t len, namelen, relnamelen;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > uda->uda_cmdmax - 2))
		return (false);
	uda->uda_tag = cmd[2];

	switch (uda->uda_tag) {
	case UPDATER_START:
		if (len < 3)
			return (false);
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
			return (false);
		if ((namelen = cmd[0]) > uda->uda_namemax)
			return (false);
		if ((relnamelen = cmd[1]) > uda->uda_namemax)
			return (false);
		if (len != namelen + relnamelen + 3)
			return (false);

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, uda->uda_name,
			      namelen)) {
			return (false);
		}
		uda->uda_name[namelen] = '\0';
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, uda->uda_release,
			      relnamelen)) {
			return (false);
		}
		uda->uda_release[relnamelen] = '\0';

		break;
	case UPDATER_END:
		if (len != 1)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

bool
updater_close(struct updater_args *uda)
{
	return (mux_close_in(uda->uda_mux, MUX_UPDATER_IN));
}
