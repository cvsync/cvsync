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

#include "receiver.h"

void *
receiver(void *arg)
{
	struct mux *mx = (struct mux *)arg;
	uint8_t *cmd = mx->mx_recvcmd, chnum;
	int err;

	for (;;) {
		if ((err = pthread_mutex_lock(&mx->mx_lock)) != 0) {
			logmsg_err("Receiver Error: mutex lock: %s",
				   strerror(err));
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}
		if (!mx->mx_isconnected) {
			logmsg_err("Receiver Error: socket");
			pthread_mutex_unlock(&mx->mx_lock);
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (mx->mx_state[MUX_IN][0] && mx->mx_state[MUX_IN][1])
			break;

		if ((err = pthread_mutex_unlock(&mx->mx_lock)) != 0) {
			logmsg_err("Receiver Error: mutex unlock: %s",
				   strerror(err));
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}

		if (!sock_recv(mx->mx_socket, cmd, 2)) {
			logmsg_err("Receiver Error: recv");
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}
		chnum = cmd[1];
		if ((chnum != 0) && (chnum != 1)) {
			logmsg_err("Receiver Error: invalid channel: %u",
				   chnum);
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}

		switch (cmd[0]) {
		case MUX_CMD_CLOSE:
			if (!receiver_close(mx, chnum)) {
				mux_abort(mx);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		case MUX_CMD_DATA:
			switch (mx->mx_compress) {
			case CVSYNC_COMPRESS_NO:
				if (!receiver_data_raw(mx, chnum)) {
					mux_abort(mx);
					return (CVSYNC_THREAD_FAILURE);
				}
				break;
			case CVSYNC_COMPRESS_ZLIB:
				if (!receiver_data_zlib(mx, chnum)) {
					mux_abort(mx);
					return (CVSYNC_THREAD_FAILURE);
				}
				break;
			default:
				logmsg_err("Receiver Error: unknown "
					   "compression type: %d",
					   mx->mx_compress);
				mux_abort(mx);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		case MUX_CMD_RESET:
			if (!receiver_reset(mx, chnum)) {
				mux_abort(mx);
				return (CVSYNC_THREAD_FAILURE);
			}
			break;
		default:
			logmsg_err("Receiver Error: unknown command: %02x",
				   cmd[0]);
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}
	}

	while (!mx->mx_state[MUX_OUT][0] || !mx->mx_state[MUX_OUT][1]) {
		logmsg_debug(DEBUG_BASE, "Receiver: Sleep: %u %u",
			     mx->mx_state[MUX_OUT][0],
			     mx->mx_state[MUX_OUT][1]);
		if ((err = pthread_cond_wait(&mx->mx_wait,
					     &mx->mx_lock)) != 0) {
			logmsg_err("Receiver Error: cond wait: %s",
				   strerror(err));
			pthread_mutex_unlock(&mx->mx_lock);
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}
		logmsg_debug(DEBUG_BASE, "Receiver: Wakeup: %u %u",
			     mx->mx_state[MUX_OUT][0],
			     mx->mx_state[MUX_OUT][1]);
		if (!mx->mx_isconnected) {
			logmsg_err("Receiver Error: socket");
			pthread_mutex_unlock(&mx->mx_lock);
			mux_abort(mx);
			return (CVSYNC_THREAD_FAILURE);
		}
	}

	if ((err = pthread_mutex_unlock(&mx->mx_lock)) != 0) {
		logmsg_err("Receiver Error: mutex unlock: %s", strerror(err));
		mux_abort(mx);
		return (CVSYNC_THREAD_FAILURE);
	}

	return (CVSYNC_THREAD_SUCCESS);
}

bool
receiver_close(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	int err;

	if ((err = pthread_mutex_lock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(CLOSE) Error: mutex lock: %s",
			   strerror(err));
		return (false);
	}
	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		logmsg_err("Receiver(CLOSE) Error: not running: %u", chnum);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((mxb->mxb_length != 0) || (mxb->mxb_rlength != 0)) {
		logmsg_err("Receiver(CLOSE) Error: work in progress");
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}
	mxb->mxb_state = MUX_STATE_CLOSED;

	if ((err = pthread_cond_signal(&mxb->mxb_wait_in)) != 0) {
		logmsg_err("Receiver(CLOSE) Error: cond signal: %s",
			   strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(CLOSE) Error: mutex unlock: %s",
			   strerror(err));
		return (false);
	}

	if ((err = pthread_mutex_lock(&mx->mx_lock)) != 0) {
		logmsg_err("Receiver(CLOSE) Error: mutex lock: %s",
			   strerror(err));
		return (false);
	}
	if (mx->mx_state[MUX_IN][chnum]) {
		logmsg_err("Receiver(CLOSE) Error: not active: %u", chnum);
		pthread_mutex_unlock(&mx->mx_lock);
		return (false);
	}
	mx->mx_state[MUX_IN][chnum] = true;
	if ((err = pthread_mutex_unlock(&mx->mx_lock)) != 0) {
		logmsg_err("Receiver(CLOSE) Error: mutex unlock: %s",
			   strerror(err));
		return (false);
	}

	return (true);
}

bool
receiver_reset(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	uint32_t len;
	uint8_t *cmd = mx->mx_recvcmd;
	int err;

	if (!sock_recv(mx->mx_socket, cmd, MUX_CMDLEN_RESET - 2)) {
		logmsg_err("Receiver(RESET) Error: recv");
		return (false);
	}
	if ((len = GetDWord(cmd)) == 0) {
		logmsg_err("Receiver(RESET) Error: invalid length: %u", len);
		return (false);
	}

	if ((err = pthread_mutex_lock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(RESET) Error: mutex lock: %s",
			   strerror(err));
		return (false);
	}
	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		logmsg_err("Receiver(RESET) Error: not running: %u", chnum);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if (len > mxb->mxb_rlength) {
		logmsg_err("Receiver(RESET) Error: invalid length: %u > "
			   "%u(rlength)", len, mxb->mxb_rlength);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	logmsg_debug(DEBUG_BASE, "Receiver(RESET) %u: %u -> %u", chnum,
		     mxb->mxb_rlength, mxb->mxb_rlength - len);

	mxb->mxb_rlength -= len;

	if ((err = pthread_cond_signal(&mxb->mxb_wait_in)) != 0) {
		logmsg_err("Receiver(RESET) Error: cond signal: %s",
			   strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(RESET) Error: mutex unlock: %s",
			   strerror(err));
		return (false);
	}

	return (true);
}
