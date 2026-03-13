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
#include "basedef.h"

#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "hash.h"
#include "mux.h"

#include "filecmp.h"
#include "updater.h"

bool
filecmp_list(struct filecmp_args *fca)
{
	static const uint8_t _cmd[3] = { 0x00, 0x01, UPDATER_END };
	struct collection *cl;
	uint8_t *cmd = fca->fca_cmd;
	size_t namelen, relnamelen, commentlen, len;
	int type;

	switch (cvsync_list_pton(fca->fca_name)) {
	case CVSYNC_LIST_ALL:
		type = CVSYNC_RELEASE_MAX;
		break;
	case CVSYNC_LIST_RCS:
		type = CVSYNC_RELEASE_RCS;
		break;
	default:
		return (false);
	}

	for (cl = fca->fca_collections_list ; cl != NULL ; cl = cl->cl_next) {
		if ((cvsync_release_pton(cl->cl_release) != type) &&
		    (type != CVSYNC_RELEASE_MAX)) {
			continue;
		}

		namelen = strlen(cl->cl_name);
		relnamelen = strlen(cl->cl_release);
		commentlen = strlen(cl->cl_comment);
		len = namelen + relnamelen + commentlen + 5;
		if (len > fca->fca_cmdmax)
			return (false);

		SetWord(cmd, len - 2);
		cmd[2] = UPDATER_UPDATE; /* dummy */
		cmd[3] = (uint8_t)namelen;
		cmd[4] = (uint8_t)relnamelen;
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cmd, 5))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cl->cl_name, namelen))
			return (false);
		if (!mux_send(fca->fca_mux, MUX_UPDATER, cl->cl_release,
			      relnamelen)) {
			return (false);
		}
		if (commentlen > 0) {
			if (!mux_send(fca->fca_mux, MUX_UPDATER,
				      cl->cl_comment, commentlen)) {
				return (false);
			}
		}
	}

	if (!mux_recv(fca->fca_mux, MUX_FILECMP_IN, cmd, 3))
		return (false);
	if (GetWord(cmd) != 1)
		return (false);
	if (cmd[2] != FILECMP_END)
		return (false);

	if (!mux_send(fca->fca_mux, MUX_UPDATER, _cmd, sizeof(_cmd)))
		return (false);

	return (true);
}
