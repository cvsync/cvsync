/*-
 * Copyright (c) 2003-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <ctype.h>
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
#include "hash.h"
#include "logmsg.h"
#include "mux.h"

#include "updater.h"

#define	UPDATER_LIST_BUFSIZE	CVSYNC_MAXAUXLEN

bool updater_list_fetch(struct updater_args *);

bool
updater_list(struct updater_args *uda)
{
	for (;;) {
		if (!updater_list_fetch(uda))
			return (false);

		if (uda->uda_tag == UPDATER_END)
			break;

		if (uda->uda_tag != UPDATER_UPDATE)
			return (false);

		if (strlen(uda->uda_buffer) > 0) {
			logmsg(" Name: %s, Release: %s\n  Comment: %s",
			       uda->uda_name, uda->uda_release,
			       uda->uda_buffer);
		} else {
			logmsg(" Name: %s, Release: %s", uda->uda_name,
			       uda->uda_release);
		}
	}

	if (!updater_list_fetch(uda))
		return (false);

	if (uda->uda_tag != UPDATER_END)
		return (false);

	return (true);
}

bool
updater_list_fetch(struct updater_args *uda)
{
	uint8_t *cmd = uda->uda_cmd, *new;
	size_t namelen, relnamelen, commentlen, len;

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > uda->uda_cmdmax - 2))
		return (false);
	if ((uda->uda_tag = cmd[2]) == UPDATER_END)
		return (len == 1);
	if (uda->uda_tag != UPDATER_UPDATE)
		return (false);
	if (len <= 3)
		return (false);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, 2))
		return (false);
	namelen = cmd[0];
	if ((namelen == 0) || (namelen >= sizeof(uda->uda_name)))
		return (false);
	relnamelen = cmd[1];
	if ((relnamelen == 0) || (relnamelen >= sizeof(uda->uda_release)))
		return (false);
	if (len < namelen + relnamelen + 3)
		return (false);
	if ((commentlen = len - namelen - relnamelen - 3) >= uda->uda_bufsize)
		return (false);

	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, uda->uda_name, namelen))
		return (false);
	uda->uda_name[namelen] = '\0';
	if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, uda->uda_release,
		      relnamelen)) {
		return (false);
	}
	uda->uda_release[relnamelen] = '\0';
	if (commentlen > 0) {
		if (!mux_recv(uda->uda_mux, MUX_UPDATER_IN, cmd, commentlen))
			return (false);
		new = (uint8_t *)uda->uda_buffer;
		for (len = 0 ; len < commentlen ; len++) {
			if (isprint((int)(cmd[len])))
				new[len] = cmd[len];
			else
				new[len] = '.';
		}
		new[commentlen] = '\0';
	} else {
		*((uint8_t *)uda->uda_buffer) = '\0';
	}

	return (true);
}
