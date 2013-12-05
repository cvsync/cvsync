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

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "hash.h"
#include "mux.h"

#include "filecmp.h"
#include "updater.h"

bool
filecmp_generic_update(struct filecmp_args *fca, struct cvsync_file *cfp)
{
	static const uint8_t _cmds[3] = { 0x00, 0x01, UPDATER_UPDATE_GENERIC };
	static const uint8_t _cmde[3] = { 0x00, 0x01, UPDATER_UPDATE_END };
	const struct hash_args *hashops = fca->fca_hash_ops;
	struct cvsync_attr *cap = &fca->fca_attr;
	uint8_t *cmd = fca->fca_cmd;

	if ((cap->ca_type != FILETYPE_FILE) &&
	    (cap->ca_type != FILETYPE_RCS) &&
	    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
		return (false);
	}

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmds, sizeof(_cmds)))
		return (false);

	SetDDWord(cmd, cfp->cf_size);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 8))
		return (false);
	if (!mux_send(fca->fca_mux, MUX_UPDATER, cfp->cf_addr,
		      (size_t)cfp->cf_size)) {
		return (false);
	}
	if (!mux_send(fca->fca_mux, MUX_UPDATER, fca->fca_hash,
		      hashops->length)) {
		return (false);
	}

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmde, sizeof(_cmde)))
		return (false);

	return (true);
}
