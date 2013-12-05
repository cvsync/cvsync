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
#include <sys/socket.h>

#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "cvsync.h"
#include "logmsg.h"
#include "mux.h"
#include "network.h"

bool mux_reset(struct mux *, struct muxbuf *, uint8_t);

struct mux *
mux_init(int sock, uint16_t mss, int compression, int level)
{
	struct mux *mx;
	uint32_t bufsize;
	int err, i, j;

	if ((bufsize = 8 * mss) > MUX_MAX_BUFSIZE)
		bufsize = MUX_MAX_BUFSIZE;

	if ((mx = malloc(sizeof(*mx))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	mx->mx_compress = compression;
	switch (mx->mx_compress) {
	case CVSYNC_COMPRESS_NO:
		/* Nothing to do. */
		break;
	case CVSYNC_COMPRESS_ZLIB:
		if (!mux_init_zlib(mx, level)) {
			free(mx);
			return (NULL);
		}
		break;
	default:
		logmsg_err("Mux Error: unknown compression type: %d",
			   mx->mx_compress);
		free(mx);
		return (NULL);
	}

	if ((err = pthread_mutex_init(&mx->mx_lock, NULL)) != 0) {
		logmsg_err("Mux Error: mutex init: %s", strerror(err));
		if (mx->mx_compress == CVSYNC_COMPRESS_ZLIB)
			mux_destroy_zlib(mx);
		free(mx);
		return (NULL);
	}
	if ((err = pthread_cond_init(&mx->mx_wait, NULL)) != 0) {
		logmsg_err("Mux Error: cond init: %s", strerror(err));
		pthread_mutex_destroy(&mx->mx_lock);
		if (mx->mx_compress == CVSYNC_COMPRESS_ZLIB)
			mux_destroy_zlib(mx);
		free(mx);
		return (NULL);
	}

	for (i = 0 ; i < 2 ; i++) {
		for (j = 0 ; j < MUX_MAXCHANNELS ; j++) {
			mx->mx_buffer[i][j].mxb_state = MUX_STATE_INIT;
			mx->mx_state[i][j] = false;
		}
	}

	for (i = 0 ; i < MUX_MAXCHANNELS ; i++) {
		if (!muxbuf_init(&mx->mx_buffer[MUX_IN][i], mss, bufsize,
				 compression)) {
			mux_destroy(mx);
			return (NULL);
		}
	}

	mx->mx_socket = sock;
	mx->mx_isconnected = true;
	mx->mx_xfer_in = mx->mx_xfer_out = 0;

	return (mx);
}

void
mux_destroy(struct mux *mx)
{
	int err, i;

	for (i = 0 ; i < MUX_MAXCHANNELS ; i++) {
		muxbuf_destroy(&mx->mx_buffer[MUX_IN][i]);
		if (mx->mx_buffer[MUX_OUT][i].mxb_state != MUX_STATE_INIT)
			muxbuf_destroy(&mx->mx_buffer[MUX_OUT][i]);
	}
	if ((err = pthread_cond_destroy(&mx->mx_wait)) != 0)
		logmsg_err("Mux Error: cond destroy: %s", strerror(err));
	pthread_mutex_destroy(&mx->mx_lock);
	switch (mx->mx_compress) {
	case CVSYNC_COMPRESS_NO:
		/* Nothing to do. */
		break;
	case CVSYNC_COMPRESS_ZLIB:
		mux_destroy_zlib(mx);
		break;
	default:
		/* Nothing to do. */
		break;
	}
	free(mx);
}

void
mux_abort(struct mux *mx)
{
	struct muxbuf *mxb;
	int err, i, j;

	pthread_mutex_lock(&mx->mx_lock);
	if (!mx->mx_isconnected) {
		pthread_mutex_unlock(&mx->mx_lock);
		return;
	}

	(void)shutdown(mx->mx_socket, SHUT_RDWR);
	mx->mx_isconnected = false;

	pthread_mutex_unlock(&mx->mx_lock);

	for (i = 0 ; i < 2 ; i++) {
		for (j = 0 ; j < MUX_MAXCHANNELS ; j++) {
			mxb = &mx->mx_buffer[i][j];
			pthread_mutex_lock(&mxb->mxb_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_cond_broadcast(&mxb->mxb_wait_in);
			pthread_cond_broadcast(&mxb->mxb_wait_out);
			pthread_mutex_unlock(&mxb->mxb_lock);
		}
	}

	if ((err = pthread_cond_broadcast(&mx->mx_wait)) != 0)
		logmsg_err("Mux Error: cond broadcast: %s", strerror(err));
}

bool
muxbuf_init(struct muxbuf *mxb, uint16_t mss, uint32_t bufsize,
	    int compression)
{
	int err;

	if (compression == CVSYNC_COMPRESS_NO) {
		if ((mss < MUX_MIN_MSS) || (mss > MUX_MAX_MSS)) {
			logmsg_err("MuxBuffer Error: invalid mss: %u", mss);
			return (false);
		}
	} else {
		if ((mss < MUX_MIN_MSS) || (mss > MUX_MAX_MSS_ZLIB)) {
			logmsg_err("MuxBuffer Error: invalid mss: %u", mss);
			return (false);
		}
	}
	if ((bufsize < MUX_MIN_BUFSIZE) || (bufsize > MUX_MAX_BUFSIZE)) {
		logmsg_err("MuxBuffer Error: invalid size: %u", bufsize);
		return (false);
	}

	mxb->mxb_bufsize = bufsize;
	if ((mxb->mxb_buffer = malloc(mxb->mxb_bufsize)) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	if ((err = pthread_mutex_init(&mxb->mxb_lock, NULL)) != 0) {
		logmsg_err("MuxBuffer Error: mutex init: %s", strerror(err));
		free(mxb->mxb_buffer);
		return (false);
	}
	if ((err = pthread_cond_init(&mxb->mxb_wait_in, NULL)) != 0) {
		logmsg_err("MuxBuffer Error: cond init: %s", strerror(err));
		pthread_mutex_destroy(&mxb->mxb_lock);
		free(mxb->mxb_buffer);
		return (false);
	}
	if ((err = pthread_cond_init(&mxb->mxb_wait_out, NULL)) != 0) {
		logmsg_err("MuxBuffer Error: cond init: %s", strerror(err));
		pthread_cond_destroy(&mxb->mxb_wait_in);
		pthread_mutex_destroy(&mxb->mxb_lock);
		free(mxb->mxb_buffer);
		return (false);
	}

	mxb->mxb_length = 0;
	mxb->mxb_head = 0;
	mxb->mxb_rlength = 0;
	mxb->mxb_state = MUX_STATE_RUNNING;

	mxb->mxb_mss = mss;
	if (compression == CVSYNC_COMPRESS_NO)
		mxb->mxb_size = mxb->mxb_mss;
	else
		mxb->mxb_size = mxb->mxb_mss / 2;

	return (true);
}

void
muxbuf_destroy(struct muxbuf *mxb)
{
	int err;

	if ((err = pthread_mutex_lock(&mxb->mxb_lock)) != 0)
		logmsg_err("MuxBuffer Error: mutex init: %s", strerror(err));
	mxb->mxb_state = MUX_STATE_ERROR;
	pthread_cond_broadcast(&mxb->mxb_wait_out);
	pthread_cond_broadcast(&mxb->mxb_wait_in);
	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0)
		logmsg_err("MuxBuffer Error: mutex init: %s", strerror(err));

	if ((err = pthread_cond_destroy(&mxb->mxb_wait_out)) != 0) {
		logmsg_err("MuxBuffer Error: cond(out) destroy: %s",
			   strerror(err));
	}
	if ((err = pthread_cond_destroy(&mxb->mxb_wait_in)) != 0) {
		logmsg_err("MuxBuffer Error: cond(in) destroy: %s",
			   strerror(err));
	}
	pthread_mutex_destroy(&mxb->mxb_lock);
	free(mxb->mxb_buffer);
}

bool
mux_send(struct mux *mx, uint8_t chnum, const void *buffer, size_t bufsize)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	const uint8_t *sp = buffer;
	size_t len;
	int err;

	if ((err = pthread_mutex_lock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Mux(SEND) Error: mutex lock: %s", strerror(err));
		return (false);
	}
	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		logmsg_err("Mux(SEND) Error: not running: %u", chnum);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (mxb->mxb_length + bufsize < mxb->mxb_size) {
		(void)memcpy(&mxb->mxb_buffer[mxb->mxb_length], buffer,
			     bufsize);
		mxb->mxb_length += bufsize;

		if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
			logmsg_err("Mux(SEND) Error: mutex unlock: %s",
				   strerror(err));
			return (false);
		}

		return (true);
	}

	while (mxb->mxb_rlength + mxb->mxb_size > mxb->mxb_bufsize) {
		logmsg_debug(DEBUG_BASE, "Mux(SEND): Sleep(%u): %u + %u > %u",
			     chnum, mxb->mxb_rlength, mxb->mxb_size,
			     mxb->mxb_bufsize);
		if ((err = pthread_cond_wait(&mxb->mxb_wait_in,
					     &mxb->mxb_lock)) != 0) {
			logmsg_err("Mux(SEND) Error: cond wait: %s",
				   strerror(err));
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		logmsg_debug(DEBUG_BASE, "Mux(SEND): Wakeup(%u): %u, %u, %u",
			     chnum, mxb->mxb_rlength, mxb->mxb_size,
			     mxb->mxb_bufsize);
		if (mxb->mxb_state != MUX_STATE_RUNNING) {
			logmsg_err("Mux(SEND) Error: not running: %u", chnum);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
	}

	len = mxb->mxb_size - mxb->mxb_length;

	if ((err = pthread_mutex_lock(&mx->mx_lock)) != 0) {
		logmsg_err("Mux(SEND) Error: mutex lock: %s", strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}
	if (!mx->mx_isconnected) {
		logmsg_err("Mux(SEND) Error: socket");
		pthread_mutex_unlock(&mx->mx_lock);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	switch (mx->mx_compress) {
	case CVSYNC_COMPRESS_NO:
		if (!mux_send_raw(mx, chnum, buffer, len)) {
			pthread_mutex_unlock(&mx->mx_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		break;
	case CVSYNC_COMPRESS_ZLIB:
		if (!mux_send_zlib(mx, chnum, buffer, len)) {
			pthread_mutex_unlock(&mx->mx_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		break;
	default:
		logmsg_err("Mux(SEND) Error: unknown compression type: %d",
			   mx->mx_compress);
		pthread_mutex_unlock(&mx->mx_lock);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (pthread_mutex_unlock(&mx->mx_lock) != 0) {
		logmsg_err("Mux(SEND) Error: mutex unlock: %s", strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	mxb->mxb_length = 0;
	mxb->mxb_rlength += mxb->mxb_size;

	sp += len;
	bufsize -= len;

	while (bufsize >= mxb->mxb_size) {
		while (mxb->mxb_rlength + mxb->mxb_size > mxb->mxb_bufsize) {
			logmsg_debug(DEBUG_BASE, "Mux(SEND): Sleep(%u): "
				     "%u + %u > %u", chnum, mxb->mxb_rlength,
				     mxb->mxb_size, mxb->mxb_bufsize);
			if ((err = pthread_cond_wait(&mxb->mxb_wait_in,
						     &mxb->mxb_lock)) != 0) {
				logmsg_err("Mux(SEND) Error: cond wait: %s",
					   strerror(err));
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
			logmsg_debug(DEBUG_BASE, "Mux(SEND): Wakeup(%u): "
				     "%u, %u, %u", chnum, mxb->mxb_rlength,
				     mxb->mxb_size, mxb->mxb_bufsize);
			if (mxb->mxb_state != MUX_STATE_RUNNING) {
				logmsg_err("Mux(SEND) Error: not running: %u",
					   chnum);
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
		}

		if ((err = pthread_mutex_lock(&mx->mx_lock)) != 0) {
			logmsg_err("Mux(SEND) Error: mutex lock: %s",
				   strerror(err));
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		if (!mx->mx_isconnected) {
			logmsg_err("Mux(SEND) Error: socket");
			pthread_mutex_unlock(&mx->mx_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}

		switch (mx->mx_compress) {
		case CVSYNC_COMPRESS_NO:
			if (!mux_send_raw(mx, chnum, sp,
					  (size_t)mxb->mxb_size)) {
				pthread_mutex_unlock(&mx->mx_lock);
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
			break;
		case CVSYNC_COMPRESS_ZLIB:
			if (!mux_send_zlib(mx, chnum, sp,
					   (size_t)mxb->mxb_size)) {
				pthread_mutex_unlock(&mx->mx_lock);
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
			break;
		default:
			logmsg_err("Mux(SEND) Error: unknown compression "
				   "type: %d", mx->mx_compress);
			pthread_mutex_unlock(&mx->mx_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}

		if ((err = pthread_mutex_unlock(&mx->mx_lock)) != 0) {
			logmsg_err("Mux(SEND) Error: mutex unlock: %s",
				   strerror(err));
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}

		sp += mxb->mxb_size;
		bufsize -= mxb->mxb_size;
		mxb->mxb_rlength += mxb->mxb_size;
	}

	if (bufsize > 0) {
		(void)memcpy(mxb->mxb_buffer, sp, bufsize);
		mxb->mxb_length += bufsize;
	}

	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Mux(SEND) Error: mutex unlock: %s", strerror(err));
		return (false);
	}

	return (true);
}

bool
mux_recv(struct mux *mx, uint8_t chnum, void *buffer, size_t bufsize)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_IN][chnum];
	uint8_t *sp = buffer;
	size_t len, len1, len2;
	int err;

	if ((err = pthread_mutex_lock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Mux(RECV) Error: mutex lock: %s", strerror(err));
		return (false);
	}
	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		logmsg_err("Mux(RECV) Error: not running: %u", chnum);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	while (bufsize > 0) {
		while (mxb->mxb_length == 0) {
			logmsg_debug(DEBUG_BASE, "Mux(RECV): Sleep(%u)", chnum);
			if ((err = pthread_cond_wait(&mxb->mxb_wait_out,
						     &mxb->mxb_lock)) != 0) {
				logmsg_err("Mux(RECV) Error: cond wait: %s",
					   strerror(err));
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
			logmsg_debug(DEBUG_BASE, "Mux(RECV): Wakeup(%u): %u",
				     chnum, mxb->mxb_length);
			if (mxb->mxb_state != MUX_STATE_RUNNING) {
				logmsg_err("Mux(RECV) Error: not running: %u",
					   chnum);
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
		}

		if (mxb->mxb_length < bufsize)
			len = mxb->mxb_length;
		else
			len = bufsize;
		len1 = mxb->mxb_head + len;
		if (len1 > mxb->mxb_bufsize)
			len2 = len1 - mxb->mxb_bufsize;
		else
			len2 = 0;
		len1 = len - len2;

		(void)memcpy(sp, &mxb->mxb_buffer[mxb->mxb_head], len1);
		if (len2 > 0)
			(void)memcpy(&sp[len1], mxb->mxb_buffer, len2);
		mxb->mxb_head += len;
		if (mxb->mxb_head >= mxb->mxb_bufsize)
			mxb->mxb_head -= mxb->mxb_bufsize;

		sp += len;
		bufsize -= len;

		mxb->mxb_rlength += len;
		if (mxb->mxb_rlength >= mxb->mxb_bufsize / 2) {
			if (!mux_reset(mx, mxb, chnum)) {
				mxb->mxb_state = MUX_STATE_ERROR;
				pthread_mutex_unlock(&mxb->mxb_lock);
				return (false);
			}
		}

		mxb->mxb_length -= len;
		if (mxb->mxb_length == 0)
			mxb->mxb_head = 0;
	}

	if ((err = pthread_cond_signal(&mxb->mxb_wait_in)) != 0) {
		logmsg_err("Mux(RECV) Error: cond signal: %s", strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Mux(RECV) Error: mutex unlock: %s", strerror(err));
		return (false);
	}

	return (true);
}

bool
mux_flush(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];

	if (pthread_mutex_lock(&mxb->mxb_lock) != 0)
		return (false);

	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (mxb->mxb_length == 0)
		goto done;

	while (mxb->mxb_rlength + mxb->mxb_length > mxb->mxb_bufsize) {
		if (pthread_cond_wait(&mxb->mxb_wait_in,
				      &mxb->mxb_lock) != 0) {
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		if (mxb->mxb_state != MUX_STATE_RUNNING) {
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
	}

	if (pthread_mutex_lock(&mx->mx_lock) != 0) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}
	if (!mx->mx_isconnected) {
		pthread_mutex_unlock(&mx->mx_lock);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	switch (mx->mx_compress) {
	case CVSYNC_COMPRESS_NO:
		if (!mux_flush_raw(mx, chnum)) {
			pthread_mutex_unlock(&mx->mx_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		break;
	case CVSYNC_COMPRESS_ZLIB:
		if (!mux_flush_zlib(mx, chnum)) {
			pthread_mutex_unlock(&mx->mx_lock);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		break;
	default:
		pthread_mutex_unlock(&mx->mx_lock);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (pthread_mutex_unlock(&mx->mx_lock) != 0) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	mxb->mxb_rlength += mxb->mxb_length;
	mxb->mxb_length = 0;

done:
	if (pthread_mutex_unlock(&mxb->mxb_lock) != 0)
		return (false);

	return (true);
}

bool
mux_close_in(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_IN][chnum];
	uint8_t cmd[MUX_CMDLEN_CLOSE];

	if (pthread_mutex_lock(&mxb->mxb_lock) != 0)
		return (false);

	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}
	if (mxb->mxb_length > 0) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((mxb->mxb_rlength > 0) && !mux_reset(mx, mxb, chnum)) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	mxb->mxb_state = MUX_STATE_CLOSED;

	if (pthread_cond_signal(&mxb->mxb_wait_in) != 0) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (pthread_mutex_unlock(&mxb->mxb_lock) != 0)
		return (false);

	cmd[0] = MUX_CMD_CLOSE;
	cmd[1] = chnum;

	if (pthread_mutex_lock(&mx->mx_lock) != 0)
		return (false);
	if (!mx->mx_isconnected) {
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}

	if (!sock_send(mx->mx_socket, cmd, MUX_CMDLEN_CLOSE)) {
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}

	if (pthread_mutex_unlock(&mx->mx_lock) != 0)
		return (false);

	return (true);
}

bool
mux_close_out(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];

	if (!mux_flush(mx, chnum))
		return (false);

	if (pthread_mutex_lock(&mxb->mxb_lock) != 0)
		return (false);

	while (mxb->mxb_state != MUX_STATE_CLOSED) {
		if (pthread_cond_wait(&mxb->mxb_wait_in,
				      &mxb->mxb_lock) != 0) {
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		if ((mxb->mxb_state != MUX_STATE_RUNNING) &&
		    (mxb->mxb_state != MUX_STATE_CLOSED)) {
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
	}
	if (mxb->mxb_rlength != 0) {
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (pthread_mutex_unlock(&mxb->mxb_lock) != 0)
		return (false);

	if (pthread_mutex_lock(&mx->mx_lock) != 0)
		return (false);
	if (!mx->mx_isconnected) {
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}

	if (mx->mx_state[MUX_OUT][chnum]) {
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}
	mx->mx_state[MUX_OUT][chnum] = true;

	if (pthread_cond_signal(&mx->mx_wait) != 0) {
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}

	if (pthread_mutex_unlock(&mx->mx_lock) != 0)
		return (false);

	return (true);
}

bool
mux_reset(struct mux *mx, struct muxbuf *mxb, uint8_t chnum)
{
	uint8_t cmd[MUX_CMDLEN_RESET];
	int err;

	cmd[0] = MUX_CMD_RESET;
	cmd[1] = chnum;
	SetDWord(&cmd[2], mxb->mxb_rlength);

	if ((err = pthread_mutex_lock(&mx->mx_lock)) != 0) {
		logmsg_err("Mux(RESET) Error: mutex lock: %s", strerror(err));
		return (false);
	}
	if (!mx->mx_isconnected) {
		logmsg_err("Mux(RESET) Error: socket");
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}

	if (!sock_send(mx->mx_socket, cmd, MUX_CMDLEN_RESET)) {
		logmsg_err("Mux(RESET) Error: send");
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}

	if ((err = pthread_mutex_unlock(&mx->mx_lock)) != 0) {
		logmsg_err("Mux(RESET) Error: mutex unlock: %s",
			   strerror(err));
		return (false);
	}

	logmsg_debug(DEBUG_BASE, "Mux(RESET) %u", chnum);

	mxb->mxb_rlength = 0;

	return (true);
}
