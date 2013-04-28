/*-
 * Copyright (c) 2002-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <utime.h>

#include "compat_sys_stat.h"
#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "cvsync.h"
#include "cvsync_attr.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "rdiff.h"

#include "updater.h"

bool updater_rdiff_update_copy(struct updater_args *, struct cvsync_file *,
			       uint64_t, uint32_t);
bool updater_rdiff_update_data(struct updater_args *, uint32_t);

bool
updater_rdiff_update(struct updater_args *uda)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	struct cvsync_attr *cap = &uda->uda_attr;
	struct cvsync_file *cfp;
	struct utimbuf times;
	uint8_t *cmd = uda->uda_cmd;
	uint64_t offset;
	uint32_t length;
	bool identical = true;

	if ((cfp = cvsync_fopen(uda->uda_path)) == NULL)
		return (false);
	if (!cvsync_mmap(cfp, (off_t)0, cfp->cf_size)) {
		cvsync_fclose(cfp);
		return (false);
	}
	if ((uda->uda_fileno = mkstemp(uda->uda_tmpfile)) == -1) {
		logmsg_err("Updater Error: rdiff: %s", strerror(errno));
		cvsync_fclose(cfp);
		return (false);
	}

	if (!(*hashops->init)(&uda->uda_hash_ctx)) {
		logmsg_err("Updater Error: rdiff: hash init");
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	for (;;) {
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 1)) {
			logmsg_err("Updater Error: rdiff: recv");
			(*hashops->destroy)(uda->uda_hash_ctx);
			cvsync_fclose(cfp);
			(void)unlink(uda->uda_tmpfile);
			(void)close(uda->uda_fileno);
			return (false);
		}

		if (cmd[0] == RDIFF_CMD_EOF)
			break;

		identical = false;

		switch (cmd[0]) {
		case RDIFF_CMD_COPY:
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 12)) {
				logmsg_err("Updater Error: rdiff: recv");
				(*hashops->destroy)(uda->uda_hash_ctx);
				cvsync_fclose(cfp);
				(void)unlink(uda->uda_tmpfile);
				(void)close(uda->uda_fileno);
				return (false);
			}
			offset = GetDDWord(cmd);
			length = GetDWord(&cmd[8]);
			if (!updater_rdiff_update_copy(uda, cfp, offset,
						       length)) {
				(*hashops->destroy)(uda->uda_hash_ctx);
				cvsync_fclose(cfp);
				(void)unlink(uda->uda_tmpfile);
				(void)close(uda->uda_fileno);
				return (false);
			}
			break;
		case RDIFF_CMD_DATA:
			if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 4)) {
				logmsg_err("Updater Error: rdiff: recv");
				(*hashops->destroy)(uda->uda_hash_ctx);
				cvsync_fclose(cfp);
				(void)unlink(uda->uda_tmpfile);
				(void)close(uda->uda_fileno);
				return (false);
			}
			length = GetDWord(cmd);
			if (!updater_rdiff_update_data(uda, length)) {
				(*hashops->destroy)(uda->uda_hash_ctx);
				cvsync_fclose(cfp);
				(void)unlink(uda->uda_tmpfile);
				(void)close(uda->uda_fileno);
				return (false);
			}
			break;
		default:
			logmsg_err("Updater Error: rdiff: unsupported "
				   "command: %02x", cmd[0]);
			(*hashops->destroy)(uda->uda_hash_ctx);
			cvsync_fclose(cfp);
			(void)unlink(uda->uda_tmpfile);
			(void)close(uda->uda_fileno);
			return (false);
		}
	}

	if (identical) {
		(*hashops->update)(uda->uda_hash_ctx, cfp->cf_addr,
				   (size_t)cfp->cf_size);
		if (unlink(uda->uda_tmpfile) == -1) {
			logmsg_err("Updater Error: rdiff: identical: %s",
				   strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			cvsync_fclose(cfp);
			(void)close(uda->uda_fileno);
			return (false);
		}
		if (close(uda->uda_fileno) == -1) {
			logmsg_err("Updater Error: rdiff: identical: %s",
				   strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			cvsync_fclose(cfp);
			return (false);
		}
		if (link(uda->uda_path, uda->uda_tmpfile) == -1) {
			logmsg_err("Updater Error: rdiff: identical: %s",
				   strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			cvsync_fclose(cfp);
			return (false);
		}
		if ((uda->uda_fileno = open(uda->uda_tmpfile, O_RDONLY,
					    0)) == -1) {
			logmsg_err("Updater Error: rdiff: identical: %s",
				   strerror(errno));
			(*hashops->destroy)(uda->uda_hash_ctx);
			cvsync_fclose(cfp);
			(void)unlink(uda->uda_tmpfile);
			return (false);
		}
	}

	(*hashops->final)(uda->uda_hash_ctx, uda->uda_hash);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, hashops->length)) {
		logmsg_err("Updater Error: rdiff: recv");
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (memcmp(cmd, uda->uda_hash, hashops->length) != 0) {
		logmsg_err("Updater Error: rdiff: %s: hash mismatch",
			   uda->uda_path);
		cvsync_fclose(cfp);
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (!cvsync_fclose(cfp)) {
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (fchmod(uda->uda_fileno, (mode_t)cap->ca_mode) == -1) {
		logmsg_err("Updater Error: rdiff: %s", strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		(void)close(uda->uda_fileno);
		return (false);
	}

	if (close(uda->uda_fileno) == -1) {
		logmsg_err("Updater Error: rdiff: %s", strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	times.actime = (time_t)cap->ca_mtime;
	times.modtime = (time_t)cap->ca_mtime;

	if (utime(uda->uda_tmpfile, &times) == -1) {
		logmsg_err("Updater Error: rdiff: %s", strerror(errno));
		(void)unlink(uda->uda_tmpfile);
		return (false);
	}

	return (true);
}

bool
updater_rdiff_update_copy(struct updater_args *uda, struct cvsync_file *cfp,
			  uint64_t offset, uint32_t length)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	uint8_t *sp, *bp;
	ssize_t wn;
	size_t len;

	logmsg_debug(DEBUG_RDIFF, "rdiff(COPY): %" PRIu64 ", %u",
		     offset, length);

	if ((length == 0) || (offset + length > (uint64_t)cfp->cf_size)) {
		logmsg_err("Updater: rdiff(COPY) error: offset=%" PRIu64
			   ", length=%lu", offset, length);
		return (false);
	}

	sp = (uint8_t *)cfp->cf_addr + (size_t)offset;
	bp = sp + length;

	while (sp < bp) {
		if ((len = bp - sp) > CVSYNC_BSIZE)
			len = CVSYNC_BSIZE;

		if ((wn = write(uda->uda_fileno, sp, len)) == -1) {
			if (errno == EINTR) {
				logmsg_intr();
				return (false);
			}
			logmsg_err("Updater Error: rdiff: %s",
				   strerror(errno));
			return (false);
		}
		if (wn == 0)
			break;
		(*hashops->update)(uda->uda_hash_ctx, sp, (size_t)wn);
		sp += wn;
	}
	if (sp != bp) {
		logmsg_err("Updater Error: rdiff: flush");
		return (false);
	}

	return (true);
}

bool
updater_rdiff_update_data(struct updater_args *uda, uint32_t length)
{
	const struct hash_args *hashops = uda->uda_hash_ops;
	ssize_t wn;
	size_t len;

	logmsg_debug(DEBUG_RDIFF, "rdiff(DATA): %u", length);

	while (length > 0) {
		if (length > uda->uda_bufsize)
			len = uda->uda_bufsize;
		else
			len = length;

		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, uda->uda_buffer,
			      len)) {
			logmsg_err("Updater Error: rdiff: recv");
			return (false);
		}

		if ((wn = write(uda->uda_fileno, uda->uda_buffer,
				len)) == -1) {
			logmsg_err("Updater Error: rdiff: %s",
				   strerror(errno));
			return (false);
		}
		if (wn == 0)
			continue;

		(*hashops->update)(uda->uda_hash_ctx, uda->uda_buffer,
				   (size_t)wn);

		length -= wn;
	}
	if (length != 0) {
		logmsg_err("Updater Error: rdiff: residue %u", length);
		return (false);
	}

	return (true);
}
