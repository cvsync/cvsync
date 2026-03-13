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
