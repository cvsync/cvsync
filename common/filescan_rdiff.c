/*-
 * Copyright (c) 2002-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <limits.h>
#include <pthread.h>

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

#include "filescan.h"
#include "filecmp.h"

bool
filescan_rdiff_update(struct filescan_args *fsa, struct cvsync_file *cfp)
{
	static const uint8_t _cmds[3] = { 0x00, 0x01, FILECMP_UPDATE_RDIFF };
	static const uint8_t _cmde[3] = { 0x00, 0x01, FILECMP_UPDATE_END };
	const struct hash_args *hashops = fsa->fsa_hash_ops;
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint32_t weak, bsize = RDIFF_MIN_BLOCKSIZE;
	uint8_t *cmd = fsa->fsa_cmd, *sp, *bp;
	size_t len;

	if (cfp->cf_size < RDIFF_MIN_BLOCKSIZE)
		return (filescan_generic_update(fsa, cfp));

	while (bsize < RDIFF_MAX_BLOCKSIZE) {
		if (cfp->cf_size / bsize <= RDIFF_NBLOCKS)
			break;
		bsize *= 2;
	}

	if ((cap->ca_type != FILETYPE_FILE) &&
	    (cap->ca_type != FILETYPE_RCS) &&
	    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
		return (false);
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmds, sizeof(_cmds)))
		return (false);

	SetDDWord(cmd, (uint64_t)cfp->cf_size);
	SetDWord(&cmd[8], bsize);
	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, 12))
		return (false);

	sp = cfp->cf_addr;
	bp = sp + (size_t)cfp->cf_size;

	while (sp < bp) {
		if ((len = (size_t)(bp - sp)) > bsize)
			len = bsize;

		weak = rdiff_weak(sp, len);
		SetDWord(cmd, weak);

		if (!(*hashops->init)(&fsa->fsa_hash_ctx))
			return (false);
		(*hashops->update)(fsa->fsa_hash_ctx, sp, len);
		(*hashops->final)(fsa->fsa_hash_ctx, &cmd[4]);

		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd,
			      hashops->length + 4)) {
			return (false);
		}

		sp += len;
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}
