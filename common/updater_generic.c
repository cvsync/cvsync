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
#include <unistd.h>
#include <utime.h>

#include "compat_sys_stat.h"
#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"

#include "updater.h"

bool
updater_generic_update(struct updater_args *uda)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	struct cvsync_attr *cap = &uda->uda_attr;
	struct utimbuf times;
	uint64_t size;
	uint8_t *cmd = uda->uda_cmd;
	ssize_t wn;
	size_t len;
	int fd;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 8)) {
		logmsg_err("Updater Error: generic: recv");
		return (false);
	}
	size = GetDDWord(cmd);

	if ((fd = mkstemp(uda->uda_tmpfile)) == -1) {
		logmsg_err("Updater Error: generic: %s", strerror(errno));
		return (false);
	}

	if (!(*hashops->init)(&uda->uda_hash_ctx)) {
		logmsg_err("Updater Error: generic: hash init");
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	while (size > 0) {
		if (size < (uint64_t)uda->uda_bufsize)
			len = (size_t)size;
		else
			len = uda->uda_bufsize;

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN,
			      uda->uda_buffer, len)) {
			logmsg_err("Updater Error: generic: recv");
			(*hashops->destroy)(uda->uda_hash_ctx);
			(void)unlink(uda->uda_tmpfile);
			(void)close(fd);
			return (false);
		}

		if ((wn = write(fd, uda->uda_buffer, len)) == -1) {
			logmsg_err("Updater Error: generic: %s",
				   strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			(void)unlink(uda->uda_tmpfile);
			(void)close(fd);
			return (false);
		}
		if (wn == 0)
			break;

		(*hashops->update)(uda->uda_hash_ctx, uda->uda_buffer, wn);
		size -= (uint64_t)wn;
	}
	if (size != 0) {
		logmsg_err("Updater Error: generic: residue %" PRIu64, size);
		(*hashops->destroy)(uda->uda_hash_ctx);
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	(*hashops->final)(uda->uda_hash_ctx, uda->uda_hash);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, hashops->length)) {
		logmsg_err("Updater Error: generic: recv");
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}
	if (memcmp(uda->uda_hash, cmd, hashops->length) != 0) {
		logmsg_err("Updater Error: generic: %s: hash mismatch",
			   uda->uda_path);
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	if (fchmod(fd, cap->ca_mode) == -1) {
		logmsg_err("Updater Error: generic: %s", strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		(void)close(fd);
		return (false);
	}

	if (close(fd) == -1) {
		logmsg_err("Updater Error: generic: %s", strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	times.actime = (time_t)cap->ca_mtime;
	times.modtime = (time_t)cap->ca_mtime;

	if (utime(uda->uda_tmpfile, &times) == -1) {
		logmsg_err("Updater Error: generic: %s", strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	return (true);
}
