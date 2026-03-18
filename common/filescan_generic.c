/*-
 * This software is released under the BSD License, see LICENSE.
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
	static const uint8_t cmds[3] = { 0x00, 0x01, FILECMP_UPDATE_GENERIC };
	static const uint8_t cmde[3] = { 0x00, 0x01, FILECMP_UPDATE_END };
	const struct hash_args *hashops = fsa->fsa_hash_ops;
	struct cvsync_attr *cap = &fsa->fsa_attr;
	uint8_t *cmd = fsa->fsa_cmd;

	if ((cap->ca_type != FILETYPE_FILE) && (cap->ca_type != FILETYPE_RCS) && (cap->ca_type != FILETYPE_RCS_ATTIC))
		return (false);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmds, sizeof(cmds)))
		return (false);

	if (!(*hashops->init)(&fsa->fsa_hash_ctx))
		return (false);
	(*hashops->update)(fsa->fsa_hash_ctx, cfp->cf_addr, (size_t)cfp->cf_size);
	(*hashops->final)(fsa->fsa_hash_ctx, cmd);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmd, hashops->length))
		return (false);

	if (!mux_send(fsa->fsa_mux, MUX_FILECMP, cmde, sizeof(cmde)))
		return (false);

	return (true);
}
