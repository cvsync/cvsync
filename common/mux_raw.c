/*-
 * This software is released under the BSD License, see LICENSE.
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
