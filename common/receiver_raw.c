/*-
 * This software is released under the BSD License, see LICENSE.
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
		logmsg_err("Receiver(DATA) Error: mutex lock: %s", strerror(err));
		return (false);
	}
	if (mxb->mxb_state != MUX_STATE_RUNNING) {
		logmsg_err("Receiver(DATA) Error: not running: %u", chnum);
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	while ((mxb->mxb_length + mss) > mxb->mxb_bufsize) {
		logmsg_debug(DEBUG_BASE, "Receiver: Sleep(%u): %u + %u > %u", chnum, mxb->mxb_length, mss,
			     mxb->mxb_bufsize);
		if ((err = pthread_cond_wait(&mxb->mxb_wait_in, &mxb->mxb_lock)) != 0) {
			logmsg_err("Receiver(DATA) Error: cond wait: %s", strerror(err));
			mxb->mxb_state = MUX_STATE_ERROR;
			pthread_mutex_unlock(&mxb->mxb_lock);
			return (false);
		}
		logmsg_debug(DEBUG_BASE, "Receiver: Wakeup(%u): %u, %u, %u", chnum, mxb->mxb_length, mss,
			     mxb->mxb_bufsize);
		if (mxb->mxb_state != MUX_STATE_RUNNING) {
			logmsg_err("Receiver(DATA) Error: not running: %u", chnum);
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
		logmsg_err("Receiver(DATA) Error: cond signal: %s", strerror(err));
		mxb->mxb_state = MUX_STATE_ERROR;
		pthread_mutex_unlock(&mxb->mxb_lock);
		return (false);
	}

	if ((err = pthread_mutex_unlock(&mxb->mxb_lock)) != 0) {
		logmsg_err("Receiver(DATA) Error: mutex unlock: %s", strerror(err));
		return(false);
	}

	return (true);
}
