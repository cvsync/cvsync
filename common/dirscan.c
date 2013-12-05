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
#include <fcntl.h>
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
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "logmsg.h"
#include "mdirent.h"
#include "mux.h"
#include "scanfile.h"

#include "dirscan.h"
#include "dircmp.h"

bool dirscan_close(struct dirscan_args *);
bool dirscan_scanfile_open(struct collection *);

struct dirscan_args *
dirscan_init(struct mux *mx, struct collection *cls, uint32_t proto)
{
	struct dirscan_args *dsa;

	if ((dsa = malloc(sizeof(*dsa))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	dsa->dsa_mux = mx;
	dsa->dsa_collections = cls;
	dsa->dsa_proto = proto;
	dsa->dsa_pathmax = sizeof(dsa->dsa_path);
	dsa->dsa_namemax = CVSYNC_NAME_MAX;
	dsa->dsa_cmdmax = sizeof(dsa->dsa_cmd);

	return (dsa);
}

void
dirscan_destroy(struct dirscan_args *dsa)
{
	free(dsa);
}

void *
dirscan(void *arg)
{
	struct dirscan_args *dsa = arg;
	struct collection *cl;

	for (cl = dsa->dsa_collections ; cl != NULL ; cl = cl->cl_next) {
		if (cl->cl_flags & CLFLAGS_DISABLE)
			continue;

		if (cvsync_isinterrupted()) {
			mux_abort(dsa->dsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!dirscan_scanfile_open(cl)) {
			logmsg_err("DirScan: Scanfile Error");
			mux_abort(dsa->dsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!dirscan_start(dsa, cl->cl_name, cl->cl_release)) {
			logmsg_err("DirScan: Initializer Error");
			mux_abort(dsa->dsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		dsa->dsa_pathlen = cl->cl_prefixlen;
		if (dsa->dsa_pathlen >= dsa->dsa_pathmax) {
			logmsg_err("DirScan: Initializer Error");
			mux_abort(dsa->dsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
		(void)memcpy(dsa->dsa_path, cl->cl_prefix, dsa->dsa_pathlen);
		dsa->dsa_path[dsa->dsa_pathlen] = '\0';
		dsa->dsa_rpath = dsa->dsa_path + dsa->dsa_pathlen;
		dsa->dsa_collection = cl;

		switch (cvsync_release_pton(cl->cl_release)) {
		case CVSYNC_RELEASE_LIST:
			/* Nothing to do. */
			break;
		case CVSYNC_RELEASE_RCS:
			if (!dirscan_rcs(dsa)) {
				logmsg_err("DirScan: RCS Error");
				mux_abort(dsa->dsa_mux);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		default:
			logmsg_err("DirScan: Release Error");
			mux_abort(dsa->dsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!dirscan_end(dsa)) {
			logmsg_err("DirScan: Collection Finalizer Error");
			mux_abort(dsa->dsa_mux);
			return (CVSYNC_THREAD_FAILURE);
		}
	}

	if (!dirscan_end(dsa)) {
		logmsg_err("DirScan: Finalizer Error");
		mux_abort(dsa->dsa_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!dirscan_close(dsa)) {
		logmsg_err("DirScan: Finalizer Error");
		mux_abort(dsa->dsa_mux);
		return (CVSYNC_THREAD_FAILURE);
	}

	return (CVSYNC_THREAD_SUCCESS);
}

bool
dirscan_start(struct dirscan_args *dsa, const char *name, const char *relname)
{
	uint8_t *cmd = dsa->dsa_cmd;
	size_t len, namelen, relnamelen;

	if (((namelen = strlen(name)) > dsa->dsa_namemax) ||
	    ((relnamelen = strlen(relname)) > dsa->dsa_namemax)) {
		return (false);
	}
	if ((len = namelen + relnamelen + 5) > dsa->dsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = DIRCMP_START;
	cmd[3] = (uint8_t)namelen;
	cmd[4] = (uint8_t)relnamelen;
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, cmd, 5))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, name, namelen))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, relname, relnamelen))
		return (false);

	if (!mux_flush(dsa->dsa_mux, MUX_DIRCMP))
		return (false);

	return (true);
}

bool
dirscan_end(struct dirscan_args *dsa)
{
	static const uint8_t _cmd[3] = { 0x00, 0x01, DIRCMP_END };

	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, _cmd, sizeof(_cmd)))
		return (false);

	return (true);
}

bool
dirscan_close(struct dirscan_args *dsa)
{
	return (mux_close_out(dsa->dsa_mux, MUX_DIRCMP));
}

bool
dirscan_scanfile_open(struct collection *cl)
{
	struct scanfile_create_args sca;
	struct mdirent_args mda;
	struct stat st;
	int fd;

	if (strlen(cl->cl_scan_name) == 0)
		return (true);

	if ((fd = open(cl->cl_scan_name, O_RDWR, 0)) == -1) {
		if (errno != ENOENT)
			return (false);

		mda.mda_errormode = cl->cl_errormode;
		mda.mda_symfollow = false;
		mda.mda_remove = true;

		sca.sca_name = cl->cl_scan_name;
		sca.sca_prefix = cl->cl_prefix;
		sca.sca_release = cl->cl_release;
		sca.sca_rprefix = cl->cl_rprefix;
		sca.sca_rprefixlen = cl->cl_rprefixlen;
		sca.sca_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
		sca.sca_mdirent_args = &mda;
		sca.sca_umask = cl->cl_umask;

		if (!scanfile_create(&sca))
			return (false);
		fd = open(cl->cl_scan_name, O_RDWR, 0);
	}
	if (fd == -1)
		return (false);

	if (fstat(fd, &st) == -1) {
		(void)close(fd);
		return (false);
	}
	if (!S_ISREG(st.st_mode)) {
		(void)close(fd);
		return (false);
	}

	if (close(fd) == -1)
		return (false);

	cl->cl_scan_mode = st.st_mode & CVSYNC_ALLPERMS;

	if ((cl->cl_scanfile = scanfile_open(cl->cl_scan_name)) == NULL)
		return (false);

	return (true);
}
