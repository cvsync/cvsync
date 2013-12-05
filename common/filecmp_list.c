/*-
 * Copyright (c) 2003-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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
