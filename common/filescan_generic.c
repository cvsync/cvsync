/*-
 * Copyright (c) 2000-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "hash.h"
#include "mux.h"

#include "filescan.h"
#include "filecmp.h"

bool
filescan_generic_update(struct filescan_args *fsa, struct cvsync_file *cfp)
{
	static const uint8_t _cmds[3] = { 0x00, 0x01, FILECMP_UPDATE_GENERIC };
	static const uint8_t _cmde[3] = { 0x00, 0x01, FILECMP_UPDATE_END };
	const struct hash_args *hashops = fsa->fsa_hash_ops;
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint8_t *cmd = fsa->fsa_cmd;

	if ((cap->ca_type != FILETYPE_FILE) &&
	    (cap->ca_type != FILETYPE_RCS) &&
	    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
		return (false);
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmds, sizeof(_cmds)))
		return (false);

	if (!(*hashops->init)(&fsa->fsa_hash_ctx))
		return (false);
	(*hashops->update)(fsa->fsa_hash_ctx, cfp->cf_addr,
			   (size_t)cfp->cf_size);
	(*hashops->final)(fsa->fsa_hash_ctx, cmd);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, hashops->length))
		return (false);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}
