/*-
 * This software is released under the BSD License, see LICENSE.
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
