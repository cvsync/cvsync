/*-
 * Copyright (c) 2002-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "basedef.h"

#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "rdiff.h"

uint32_t
rdiff_weak(const uint8_t *addr, size_t size)
{
	uint16_t l_rv = 0, h_rv = 0;
	size_t i;

	for (i = 0 ; i < size ; i++) {
		l_rv += addr[i];
		h_rv += l_rv;
	}

	return ((uint32_t)((h_rv << 16) | l_rv));
}

uint8_t *
rdiff_search(uint8_t *sp, uint8_t *bp, uint32_t bsize, size_t length,
	     uint32_t weak, uint8_t *strong, const struct hash_args *hashops)
{
	uint32_t w;
	uint16_t wl, wh, rwl, rwh;
	uint8_t hash[HASH_MAXLEN];
	void *ctx;
	size_t len;

	if ((len = (size_t)(bp - sp)) < length)
		return (NULL);
	if (len > bsize)
		len = bsize;
	if ((w = rdiff_weak(sp, len)) == weak) {
		if (!(*hashops->init)(&ctx)) {
			logmsg_err("rdiff error: hash init");
			return (NULL);
		}
		(*hashops->update)(ctx, sp, len);
		(*hashops->final)(ctx, hash);

		if (memcmp(hash, strong, hashops->length) == 0)
			return (sp);
	}
	rwl = RDIFF_WEAK_LOW(weak);
	rwh = RDIFF_WEAK_HIGH(weak);

	wl = RDIFF_WEAK_LOW(w);
	wh = RDIFF_WEAK_HIGH(w);

	while (sp < bp) {
		if ((len = (size_t)(bp - sp)) < length)
			return (NULL);

		if (len > bsize) {
			wl = (uint16_t)(wl - sp[0] + sp[bsize]);
			wh = (uint16_t)(wh - bsize * sp[0] + wl);
			len = bsize;
		} else {
			wl = (uint16_t)(wl - sp[0]);
			wh = (uint16_t)(wh - len-- * sp[0]);
		}

		if ((rwl == wl) && (rwh == wh)) {
			if (!(*hashops->init)(&ctx)) {
				logmsg_err("rdiff error: hash init");
				return (NULL);
			}
			(*hashops->update)(ctx, sp + 1, len);
			(*hashops->final)(ctx, hash);

			if (memcmp(hash, strong, hashops->length) == 0)
				return (sp + 1);
		}

		sp++;
	}

	return (NULL);
}

bool
rdiff_copy(struct mux *mx, uint8_t chnum, off_t position, size_t length)
{
	uint8_t cmd[RDIFF_MAXCMDLEN];

	cmd[0] = RDIFF_CMD_COPY;
	SetDDWord(&cmd[1], position);
	SetDWord(&cmd[9], length);

	if (!mux_send(mx, chnum, cmd, 13)) {
		logmsg_err("rdiff(COPY) error: send");
		return (false);
	}

	return (true);
}

bool
rdiff_data(struct mux *mx, uint8_t chnum, const void *buffer, size_t bufsize)
{
	uint8_t cmd[RDIFF_MAXCMDLEN];

	cmd[0] = RDIFF_CMD_DATA;
	SetDWord(&cmd[1], bufsize);

	if (!mux_send(mx, chnum, cmd, 5)) {
		logmsg_err("rdiff(DATA) error: send");
		return (false);
	}
	if (!mux_send(mx, chnum, buffer, bufsize)) {
		logmsg_err("rdiff(DATA) error: send");
		return (false);
	}

	return (true);
}

bool
rdiff_eof(struct mux *mx, uint8_t chnum)
{
	uint8_t cmd[RDIFF_MAXCMDLEN];

	cmd[0] = RDIFF_CMD_EOF;

	if (!mux_send(mx, chnum, cmd, 1)) {
		logmsg_err("rdiff(EOF) error: send");
		return (false);
	}

	return (true);
}
