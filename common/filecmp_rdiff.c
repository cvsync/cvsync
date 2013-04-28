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

#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "hash.h"
#include "mux.h"
#include "rdiff.h"

#include "filecmp.h"
#include "updater.h"

bool
filecmp_rdiff_update(struct filecmp_args *fca, struct cvsync_file *cfp)
{
	static const uint8_t _cmds[3] = { 0x00, 0x01, UPDATER_UPDATE_RDIFF };
	static const uint8_t _cmde[3] = { 0x00, 0x01, UPDATER_UPDATE_END };
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct cvsync_attr *cap = &fca->fca_attr;
	uint64_t fsize, offset = 0;
	uint32_t bsize, length = 0, weak;
	uint8_t *cmd = fca->fca_cmd, *sp, *bp, *sv_sp;
	size_t rsize, len, n, i;

	if ((cap->ca_type != FILETYPE_FILE) &&
	    (cap->ca_type != FILETYPE_RCS) &&
	    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
		return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 12))
		return (false);
	fsize = GetDDWord(cmd);
	bsize = GetDWord(&cmd[8]);
	n = (size_t)(fsize / bsize);
	if ((rsize = (size_t)(fsize % bsize)) != 0)
		n++;
	else
		rsize = bsize;

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmds, sizeof(_cmds)))
		return (false);

	sp = cfp->cf_addr;
	bp = sp + (size_t)cfp->cf_size;

	for (i = 0 ; i < n ; i++) {
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
			return (false);
		weak = GetDWord(cmd);
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, fca->fca_hash,
			      hashops->length)) {
			return (false);
		}
		if (sp >= bp)
			continue;

		if (i != n - 1)
			len = bsize;
		else
			len = rsize;

		sv_sp = sp;
		if ((sp = rdiff_search(sp, bp, bsize, len, weak, fca->fca_hash,
				       hashops)) == NULL) {
			sp = sv_sp;
			continue;
		}

		if ((len = sp - sv_sp) > 0) {
			if (length > 0) {
				if (!rdiff_copy(fca->fca_mux, MUX_UPDATER,
						(off_t)offset, length)) {
					return (false);
				}
				length = 0;
			}
			if (!rdiff_data(fca->fca_mux, MUX_UPDATER, sv_sp, len))
				return (false);
		}

		if ((len = (size_t)(fsize - i * bsize)) > bsize)
			len = bsize;

		sp += len;

		if (length == 0) {
			offset = i * bsize;
			length = len;
			continue;
		}
		if (offset + length == i * bsize) {
			length += len;
			continue;
		}

		if (!rdiff_copy(fca->fca_mux, MUX_UPDATER, (off_t)offset,
				length)) {
			return (false);
		}
		if (!rdiff_copy(fca->fca_mux, MUX_UPDATER, (off_t)(i * bsize),
				len)) {
			return (false);
		}

		length = 0;
	}
	if ((length > 0) && ((uint64_t)length < fsize)) {
		if (!rdiff_copy(fca->fca_mux, MUX_UPDATER, (off_t)offset,
				length)) {
			return (false);
		}
		length = 0;
	}
	if (sp < bp) {
		if (length > 0) {
			if (!rdiff_copy(fca->fca_mux, MUX_UPDATER,
					(off_t)offset, length)) {
				return (false);
			}
		}
		if (!rdiff_data(fca->fca_mux, MUX_UPDATER, sp,
				(size_t)(bp - sp))) {
			return (false);
		}
	}
	if (!rdiff_eof(fca->fca_mux, MUX_UPDATER))
		return (false);

	if (!(*hashops->init)(&fca->fca_hash_ctx))
		return (false);
	(*hashops->update)(fca->fca_hash_ctx, cfp->cf_addr,
			   (size_t)cfp->cf_size);
	(*hashops->final)(fca->fca_hash_ctx, cmd);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, hashops->length))
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}

bool
filecmp_rdiff_ignore(struct filecmp_args *fca)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct cvsync_attr *cap = &fca->fca_attr;
	uint64_t fsize;
	uint32_t bsize;
	uint8_t *cmd = fca->fca_cmd;
	size_t n, i;

	if ((cap->ca_type != FILETYPE_FILE) &&
	    (cap->ca_type != FILETYPE_RCS) &&
	    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
		return (false);
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 12))
		return (false);
	fsize = GetDDWord(cmd);
	bsize = GetDWord(&cmd[8]);
	n = (size_t)(fsize / bsize);
	if ((fsize % bsize) != 0)
		n++;

	for (i = 0 ; i < n ; i++) {
		if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
			      hashops->length + 4)) {
			return (false);
		}
	}

	return (true);
}

bool
filecmp_rdiff_ischanged(struct filecmp_args *fca, struct cvsync_file *cfp)
{
	const struct hash_args *hashops = fca->fca_hash_ops;
	uint64_t fsize;
	uint32_t bsize, weak;
	uint8_t *cmd = fca->fca_cmd, *sp, *bp;
	size_t len, n, i = 0;

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 12))
		return (false);
	fsize = GetDDWord(cmd);
	bsize = GetDWord(&cmd[8]);

	n = (size_t)(fsize / bsize);
	if ((fsize % bsize) != 0)
		n++;

	sp = cfp->cf_addr;
	bp = sp + (size_t)fsize;

	if ((uint64_t)cfp->cf_size == fsize) {
		while (i < n) {
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 4))
				return (false);
			weak = GetDWord(cmd);
			if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd,
				      hashops->length)) {
				return (false);
			}
			i++;

			if ((len = bp - sp) > bsize)
				len = bsize;
			if (rdiff_weak(sp, len) != weak)
				break;

			if (!(*hashops->init)(&fca->fca_hash_ctx))
				return (false);
			(*hashops->update)(fca->fca_hash_ctx, sp, len);
			(*hashops->final)(fca->fca_hash_ctx, fca->fca_hash);

			if (memcmp(fca->fca_hash, cmd, hashops->length) != 0)
				break;

			sp += len;
		}
		if (i == n)
			return (true);
	}

	len = (n - i) * (hashops->length + 4);

	while (len > 0) {
		if (len > fca->fca_cmdmax)
			n = fca->fca_cmdmax;
		else
			n = len;
		mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, n);
		len -= n;
	}

	return (false);
}
