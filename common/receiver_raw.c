/*-
 * Copyright (c) 2000-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <sys/uio.h>

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "basedef.h"

#include "logmsg.h"
#include "mux.h"
#include "network.h"

#include "receiver.h"

bool
receiver_data_raw(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_IN][chnum];
	uint16_t mss;
	uint8_t *cmd = mx->mx_recvcmd;
	size_t len1, len2, tail;
	int err;

	if (!sock_recv(mx->mx_socket, cmd, MUX_CMDLEN_DATA - 2)) {
		logmsg_err("Receiver(DATA) Error: recv");
		return (false);
	}

	mss = GetWord(cmd);
	if ((mss == 0) || (mss > mxb->mxb_mss)) {
		logmsg_err("Receiver(DATA) Error: invalid length: %u", mss);
		return (false);
	}

	if ((err = pthread_mutex_lock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(DATA) Error: mutex lock: %s",
			   strerror(err));
		return (false);
	}
	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		logmsg_err("Receiver(DATA) Error: not running: %u", chnum);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	while (mxb->mxb_length + mss > mxb->mxb_bufsize) {
		logmsg_debug(DEBUG_BASE, "Receiver: Sleep(%u): %u + %u > %u",
			     chnum, mxb->mxb_length, mss, mxb->mxb_bufsize);
		if ((err = pthread_cond_wait(&mxb->mxb_wait_in,
					     &mxb->mxb_lock)) != 0) {
			logmsg_err("Receiver(DATA) Error: cond wait: %s",
				   strerror(err));
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		logmsg_debug(DEBUG_BASE, "Receiver: Wakeup(%u): %u, %u, %u",
			     chnum, mxb->mxb_length, mss, mxb->mxb_bufsize);
		if (mxb->mxb_state != MUX_STATE_RUNNING) {
			logmsg_err("Receiver(DATA) Error: not running: %u",
				   chnum);
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
	}

	if ((tail = mxb->mxb_head + mxb->mxb_length) >= mxb->mxb_bufsize)
		tail -= mxb->mxb_bufsize;
	if ((len1 = tail + mss) > mxb->mxb_bufsize)
		len2 = len1 - mxb->mxb_bufsize;
	else
		len2 = 0;
	len1 = mss - len2;

	if (!sock_recv(mx->mx_socket, &mxb->mxb_buffer[tail], len1)) {
		logmsg_err("Receiver(DATA) Error: recv data");
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}
	if (len2 > 0) {
		if (!sock_recv(mx->mx_socket, mxb->mxb_buffer, len2)) {
			logmsg_err("Receiver(DATA) Error: recv data");
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
	}

	mx->mx_xfer_in += mss;
	mxb->mxb_length += mss;

	if ((err = pthread_cond_signal(&mxb->mxb_wait_out)) != 0) {
		logmsg_err("Receiver(DATA) Error: cond signal: %s",
			   strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(DATA) Error: mutex unlock: %s",
			   strerror(err));
		return(false);
	}

	return (true);
}
