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
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

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

#include "filescan.h"
#include "filecmp.h"

bool filescan_fetch(struct filescan_args *);
bool filescan_close(struct filescan_args *);

struct filescan_args *
filescan_init(struct mux *mx, struct collection *cls, uint32_t proto, int type)
{
	struct filescan_args *fsa;

	if ((fsa = malloc(sizeof(*fsa))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	fsa->fsa_mux = mx;
	fsa->fsa_proto = proto;
	fsa->fsa_collections = cls;
	fsa->fsa_pathmax = sizeof(fsa->fsa_path);
	fsa->fsa_namemax = CVSYNC_NAME_MAX;
	fsa->fsa_cmdmax = sizeof(fsa->fsa_cmd);

	if (!hash_set(type, &fsa->fsa_hash_ops)) {
		free(fsa);
		return (NULL);
	}

	return (fsa);
}

void
filescan_destroy(struct filescan_args *fsa)
{
	free(fsa);
}

void *
filescan(void *arg)
{
	struct filescan_args *fsa = arg;
	struct collection *cl;

	for (cl = fsa->fsa_collections ; cl != NULL ; cl = cl->cl_next) {
		if (cl->cl_flags & CLFLAGS_DISABLE)
			continue;

		if (cvsync_isinterrupted()) {
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!filescan_fetch(fsa)) {
			logmsg_err("FileScan: Fetch Error");
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (fsa->fsa_tag != FILESCAN_START) {
			logmsg_err("FileScan: invalid tag: %02x",
				   fsa->fsa_tag);
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if ((strcasecmp(cl->cl_name, fsa->fsa_name) != 0) ||
		    (strcasecmp(cl->cl_release, fsa->fsa_release) != 0)) {
			logmsg_err("FileScan: Collection Error");
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!filescan_start(fsa, cl->cl_name, cl->cl_release)) {
			logmsg_err("FileScan: Initializer Error");
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		fsa->fsa_refuse = cl->cl_refuse;
		fsa->fsa_pathlen = cl->cl_prefixlen;
		if (fsa->fsa_pathlen >= fsa->fsa_pathmax) {
			logmsg_err("FileScan: Initializer Error");
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
		(void)memcpy(fsa->fsa_path, cl->cl_prefix, fsa->fsa_pathlen);
		fsa->fsa_path[fsa->fsa_pathlen] = '\0';
		fsa->fsa_rpath = &fsa->fsa_path[fsa->fsa_pathlen];
		fsa->fsa_umask = cl->cl_umask;

		switch (cvsync_release_pton(cl->cl_release)) {
		case CVSYNC_RELEASE_LIST:
			if (!filescan_fetch(fsa)) {
				logmsg_err("FileScan: Fetch Error");
				mux_abort(fsa->fsa_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			if (fsa->fsa_tag != FILESCAN_END) {
				logmsg_err("FileScan: LIST Error");
				mux_abort(fsa->fsa_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		case CVSYNC_RELEASE_RCS:
			if (!filescan_rcs(fsa)) {
				logmsg_err("FileScan: RCS Error");
				mux_abort(fsa->fsa_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		default:
			logmsg_err("FileScan: Release Error");
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!filescan_end(fsa)) {
			logmsg_err("FileScan: Collection Finalizer Error");
			mux_abort(fsa->fsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
	}

	if (!filescan_fetch(fsa)) {
		logmsg_err("FileScan: Fetch Error");
		mux_abort(fsa->fsa_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (fsa->fsa_tag != FILESCAN_END) {
		logmsg_err("FileScan: invalid tag: %02x", fsa->fsa_tag);
		mux_abort(fsa->fsa_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!filescan_end(fsa)) {
		logmsg_err("FileScan: Finalizer Error");
		mux_abort(fsa->fsa_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!filescan_close(fsa)) {
		logmsg_err("FileScan: Finalizer Error");
		mux_abort(fsa->fsa_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	return (CVSYNC_THREAD_SUCCESS);
}

bool
filescan_fetch(struct filescan_args *fsa)
{
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len, namelen, relnamelen;

	if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > fsa->fsa_cmdmax - 2))
		return (false);
	fsa->fsa_tag = cmd[2];

	switch (fsa->fsa_tag) {
	case FILESCAN_START:
		if (len < 3)
			return (false);
		if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, cmd, 2))
			return (false);
		if ((namelen = cmd[0]) > fsa->fsa_namemax)
			return (false);
		if ((relnamelen = cmd[1]) > fsa->fsa_namemax)
			return (false);
		if (len != namelen + relnamelen + 3)
			return (false);

		if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, fsa->fsa_name,
			      namelen)) {
			return (false);
		}
		fsa->fsa_name[namelen] = '\0';
		if (!mux_recv(fsa->fsa_mux, MUX_FILESCAN_IN, fsa->fsa_release,
			      relnamelen)) {
			return (false);
		}
		fsa->fsa_release[relnamelen] = '\0';

		break;
	case FILESCAN_END:
		if (len != 1)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

bool
filescan_start(struct filescan_args *fsa, const char *name, const char *relname)
{
	uint8_t *cmd = fsa->fsa_cmd;
	size_t len, namelen, relnamelen;

	if (((namelen = strlen(name)) > fsa->fsa_namemax) ||
	    ((relnamelen = strlen(relname)) > fsa->fsa_namemax)) {
		return (false);
	}
	if ((len = namelen + relnamelen + 5) > fsa->fsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILECMP_START;
	cmd[3] = namelen;
	cmd[4] = relnamelen;
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 5))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, name, namelen))
		return (false);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, relname, relnamelen))
		return (false);

	if (!mux_flush(fsa->fsa_mux, MUX_FILECMP))
		return (false);

	return (true);
}

bool
filescan_end(struct filescan_args *fsa)
{
	static const uint8_t _cmd[3] = { 0x00, 0x01, FILECMP_END };

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmd, sizeof(_cmd)))
		return (false);

	return (true);
}

bool
filescan_close(struct filescan_args *fsa)
{
	if (!mux_close_out(fsa->fsa_mux, MUX_FILECMP))
		return (false);
	return (mux_close_in(fsa->fsa_mux, MUX_FILESCAN_IN));
}
