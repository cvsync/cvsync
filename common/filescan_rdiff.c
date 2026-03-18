/*-
 * This software is released under the BSD License, see LICENSE.
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
	static const uint8_t cmds[3] = { 0x00, 0x01, FILECMP_UPDATE_RDIFF };
	static const uint8_t cmde[3] = { 0x00, 0x01, FILECMP_UPDATE_END };
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

	if ((cap->ca_type != FILETYPE_FILE) && (cap->ca_type != FILETYPE_RCS) && (cap->ca_type != FILETYPE_RCS_ATTIC))
		return (false);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmds, sizeof(cmds)))
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

		if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, hashops->length + 4))
			return (false);

		sp += len;
	}

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmde, sizeof(cmde)))
		return (false);

	return (true);
}
