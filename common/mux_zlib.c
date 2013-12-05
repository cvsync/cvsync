/*-
 * Copyright (c) 2003-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <zlib.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "cvsync.h"
#include "logmsg.h"
#include "mux.h"
#include "mux_zlib.h"
#include "network.h"

bool
mux_init_zlib(struct mux *mx, int level)
{
	struct mux_stream_zlib *stream;

	if ((stream = malloc(sizeof(*stream))) == NULL) {
		logmsg_err("Mux Error: %s", strerror(errno));
		return (false);
	}
	stream->ms_zbufsize_in = sizeof(stream->ms_zbuffer_in);
	stream->ms_zbufsize_out = sizeof(stream->ms_zbuffer_out);

	stream->ms_zstream_in.zalloc = Z_NULL;
	stream->ms_zstream_in.zfree = Z_NULL;
	stream->ms_zstream_in.opaque = Z_NULL;
	if (inflateInit(&stream->ms_zstream_in) != Z_OK) {
		logmsg_err("Mux Error: INFLATE init: %s",
			   stream->ms_zstream_in.msg);
		free(stream);
		return (false);
	}

	stream->ms_zstream_out.zalloc = Z_NULL;
	stream->ms_zstream_out.zfree = Z_NULL;
	stream->ms_zstream_out.opaque = Z_NULL;
	if (deflateInit(&stream->ms_zstream_out, level) != Z_OK) {
		logmsg_err("Mux Error: DEFLATE init: %s",
			   stream->ms_zstream_out.msg);
		inflateEnd(&stream->ms_zstream_in);
		free(stream);
		return (false);
	}
	stream->ms_zstream_out.next_out = stream->ms_zbuffer_out;
	stream->ms_zstream_out.avail_out = stream->ms_zbufsize_out;

	mx->mx_stream = stream;

	return (true);
}

void
mux_destroy_zlib(struct mux *mx)
{
	struct mux_stream_zlib *stream = mx->mx_stream;

	inflateEnd(&stream->ms_zstream_in);
	deflateEnd(&stream->ms_zstream_out);
	free(stream);
}

bool
mux_send_zlib(struct mux *mx, uint8_t chnum, const void *buffer, size_t bufsize)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	struct mux_stream_zlib *stream = mx->mx_stream;
	z_stream *z = &stream->ms_zstream_out;
	uint8_t cmd[MUX_CMDLEN_DATA];

	if (mxb->mxb_length > 0) {
		z->next_in = mxb->mxb_buffer;
		z->avail_in = mxb->mxb_length;
		if (deflate(z, Z_NO_FLUSH) != Z_OK) {
			logmsg_err("Mux(SEND) Error: DEFLATE: %s", z->msg);
			return (false);
		}
	}
	z->next_in = (void *)(unsigned long)buffer;
	z->avail_in = bufsize;
	if (deflate(z, Z_FINISH) != Z_STREAM_END) {
		logmsg_err("Mux(SEND) Error: DEFLATE: %s", z->msg);
		return (false);
	}
	if (z->total_out > mxb->mxb_mss) {
		logmsg_err("Mux(SEND) Error: DEFLATE: %u > %u(mss)",
			   z->total_out, mxb->mxb_mss);
		return (false);
	}

	logmsg_debug(DEBUG_ZLIB, "DEFLATE: %u => %u",
		     mxb->mxb_length + bufsize, z->total_out);

	cmd[0] = MUX_CMD_DATA;
	cmd[1] = chnum;
	SetWord(&cmd[2], z->total_out);

	if (!sock_send(mx->mx_socket, cmd, MUX_CMDLEN_DATA)) {
		logmsg_err("Mux(SEND) Error: send");
		return (false);
	}
	if (!sock_send(mx->mx_socket, stream->ms_zbuffer_out,
		       (size_t)z->total_out)) {
		logmsg_err("Mux(SEND) Error: send");
		return (false);
	}
	mx->mx_xfer_out += mxb->mxb_length + bufsize;

	if (deflateReset(z) != Z_OK) {
		logmsg_err("Mux(SEND) Error: DEFLATE: %s", z->msg);
		return (false);
	}
	z->next_out = stream->ms_zbuffer_out;
	z->avail_out = stream->ms_zbufsize_out;
	z->total_out = 0;

	return (true);
}

bool
mux_flush_zlib(struct mux *mx, uint8_t chnum)
{
	struct muxbuf *mxb = &mx->mx_buffer[MUX_OUT][chnum];
	struct mux_stream_zlib *stream = mx->mx_stream;
	z_stream *z = &stream->ms_zstream_out;
	uint8_t cmd[MUX_CMDLEN_DATA];

	z->next_in = mxb->mxb_buffer;
	z->avail_in = mxb->mxb_length;
	if (deflate(z, Z_FINISH) != Z_STREAM_END) {
		logmsg_err("Mux(FLUSH) Error: DEFLATE: %s", z->msg);
		return (false);
	}
	if (z->total_out > mxb->mxb_mss) {
		logmsg_err("Mux(FLUSH) Error: DEFLATE: %u > %u(mss)",
			   z->total_out, mxb->mxb_mss);
		return (false);
	}

	logmsg_debug(DEBUG_ZLIB, "DEFLATE: %u => %u", mxb->mxb_length,
		     z->total_out);

	cmd[0] = MUX_CMD_DATA;
	cmd[1] = chnum;
	SetWord(&cmd[2], z->total_out);

	if (!sock_send(mx->mx_socket, cmd, MUX_CMDLEN_DATA)) {
		logmsg_err("Mux(FLUSH) Error: send");
		return (false);
	}
	if (!sock_send(mx->mx_socket, stream->ms_zbuffer_out,
		       (size_t)z->total_out)) {
		logmsg_err("Mux(FLUSH) Error: send");
		return (false);
	}
	mx->mx_xfer_out += mxb->mxb_length;

	if (deflateReset(z) != Z_OK) {
		logmsg_err("Mux(FLUSH) Error: DEFLATE: %s", z->msg);
		return (false);
	}
	z->next_out = stream->ms_zbuffer_out;
	z->avail_out = stream->ms_zbufsize_out;
	z->total_out = 0;

	return (true);
}
