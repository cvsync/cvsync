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

#include <pthread.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "basedef.h"

#include "logmsg.h"
#include "mux.h"
#include "network.h"

bool
mux_send_raw(struct mux *mx, uint8_t chnum, const void *buffer, size_t bufsize)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	uint8_t cmd[MUX_CMDLEN_DATA];

	cmd[0] = MUX_CMD_DATA;
	cmd[1] = chnum;
	SetWord(&cmd[2], mxb->mxb_length + bufsize);

	if (!sock_send(mx->mx_socket, cmd, MUX_CMDLEN_DATA)) {
		logmsg_err("Mux(SEND) Error: send");
		return (false);
	}
	if (!sock_send(mx->mx_socket, mxb->mxb_buffer, mxb->mxb_length)) {
		logmsg_err("Mux(SEND) Error: send");
		return (false);
	}
	mx->mx_xfer_out += mxb->mxb_length;
	if (!sock_send(mx->mx_socket, buffer, bufsize)) {
		logmsg_err("Mux(SEND) Error: send");
		return (false);
	}
	mx->mx_xfer_out += bufsize;

	return (true);
}

bool
mux_flush_raw(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	uint8_t cmd[MUX_CMDLEN_DATA];

	cmd[0] = MUX_CMD_DATA;
	cmd[1] = chnum;
	SetWord(&cmd[2], mxb->mxb_length);

	if (!sock_send(mx->mx_socket, cmd, MUX_CMDLEN_DATA)) {
		logmsg_err("Mux(FLUSH) Error: send");
		return (false);
	}
	if (!sock_send(mx->mx_socket, mxb->mxb_buffer, mxb->mxb_length)) {
		logmsg_err("Mux(FLUSH) Error: send");
		return (false);
	}
	mx->mx_xfer_out += mxb->mxb_length;

	return (true);
}
