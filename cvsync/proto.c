/*-
 * Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <sys/stat.h>

#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "network.h"
#include "version.h"

#include "defs.h"

bool collection_exchange_list(int, struct collection *);
bool collection_exchange_rcs(int, struct collection *);

bool
protocol_exchange(int sock, struct config *cf)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN], mn;
	size_t len;

	SetWord(cmd, 2);
	cmd[2] = CVSYNC_PROTO_MAJOR;
	cmd[3] = CVSYNC_PROTO_MINOR;
	if (!sock_send(sock, cmd, 4)) {
		logmsg_err("Send: protocol version");
		return (false);
	}

	if (!sock_recv(sock, cmd, 2)) {
		logmsg_err("Recv: protocol version length");
		return (false);
	}
	if ((len = GetWord(cmd)) != 2) {
		logmsg_err("Recv: protocol version: invalid length: %u", len);
		return (false);
	}
	if (!sock_recv(sock, cmd, len)) {
		logmsg_err("Recv: protocol version");
		return (false);
	}

	if (cmd[0] == CVSYNC_PROTO_ERROR) {
		switch (cmd[1]) {
		case CVSYNC_ERROR_DENIED:
			logmsg_err("Access denied");
			break;
		case CVSYNC_ERROR_LIMITED:
			logmsg_err("Access limited");
			break;
		case CVSYNC_ERROR_UNAVAIL:
			logmsg_err("Service unavailable");
			break;
		case CVSYNC_ERROR_UNSPEC:
		default:
			logmsg_err("Your software seems to be old.\n"
				   "Please upgrade to the newer version.\n"
				   "URL: %s", CVSYNC_URL);
			break;
		}
		SetWord(cmd, 2);
		cmd[2] = CVSYNC_PROTO_ERROR;
		cmd[3] = CVSYNC_ERROR_UNSPEC;
		sock_send(sock, cmd, 4);
		return (false);
	}

	if (cmd[0] != CVSYNC_PROTO_MAJOR) {
		logmsg_err("The server(%u.%u) seems to be too old.", cmd[0],
			   cmd[1]);
		SetWord(cmd, 2);
		cmd[2] = CVSYNC_PROTO_ERROR;
		cmd[3] = CVSYNC_ERROR_UNSPEC;
		sock_send(sock, cmd, 4);
		return (false);
	}

	if ((cmd[0] == 0) && (cmd[1] < 20)) {
		logmsg_err("The server (%u.%u) seems to be old.", cmd[0],
			   cmd[1]);
		SetWord(cmd, 2);
		cmd[2] = CVSYNC_PROTO_ERROR;
		cmd[3] = CVSYNC_ERROR_UNSPEC;
		sock_send(sock, cmd, 4);
		return (false);
	}

	if ((cmd[0] == 0) && (cmd[1] < CVSYNC_PROTO_MINOR))
		mn = cmd[1];
	else
		mn = CVSYNC_PROTO_MINOR;

	SetWord(cmd, 2);
	cmd[2] = CVSYNC_PROTO_MAJOR;
	cmd[3] = mn;
	if (!sock_send(sock, cmd, 4)) {
		logmsg_err("Send: protocol version: %u.%u", CVSYNC_PROTO_MAJOR,
			   mn);
		return (false);
	}

	cf->cf_proto = CVSYNC_PROTO(CVSYNC_PROTO_MAJOR, mn);

	logmsg_verbose("Protocol: %u.%u", cmd[2], cmd[3]);

	return (true);
}

bool
hash_exchange(int sock, struct config *cf)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	char name[CVSYNC_NAME_MAX + 1];
	size_t len;

	if ((len = hash_ntop(cf->cf_hash, name, sizeof(name))) == 0)
		return (false);

	SetWord(cmd, len);
	if (!sock_send(sock, cmd, 2)) {
		logmsg_err("Send: hash type");
		return (false);
	}
	if (!sock_send(sock, name, len)) {
		logmsg_err("Send: hash type");
		return (false);
	}

	if (!sock_recv(sock, cmd, 2)) {
		logmsg_err("Recv: hash type length");
		return (false);
	}
	len = GetWord(cmd);
	if ((len == 0) || (len >= sizeof(name))) {
		logmsg_err("Recv: hash type: invaild length: %u", len);
		return (false);
	}
	if (!sock_recv(sock, name, len)) {
		logmsg_err("Recv: hash type");
		return (false);
	}

	if ((cf->cf_hash = hash_pton(name, len)) == HASH_UNSPEC) {
		logmsg_err("Unknwon hash type: %.*s", len, name);
		return (false);
	}

	logmsg_verbose("Hash: %.*s", len, name);

	return (true);
}

bool
collectionlist_exchange(int sock, struct config *cf)
{
	struct collection *cl;
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t len;
	int enabled;

	logmsg_verbose("Exchanging collection list...");

	enabled = 0;
	for (cl = cf->cf_collections ; cl != NULL ; cl = cl->cl_next) {
		switch (cvsync_release_pton(cl->cl_release)) {
		case CVSYNC_RELEASE_LIST:
			if (!collection_exchange_list(sock, cl))
				return (false);
			break;
		case CVSYNC_RELEASE_RCS:
			if (!collection_exchange_rcs(sock, cl))
				return (false);
			break;
		default:
			return (false);
		}

		if (!(cl->cl_flags & CLFLAGS_DISABLE))
			enabled++;
	}

	SetWord(cmd, 4);
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = '.';
	cmd[5] = '.';
	if (!sock_send(sock, cmd, 6))
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if ((len = GetWord(cmd)) != 4)
		return (false);
	if (!sock_recv(sock, cmd, len))
		return (false);
	if ((cmd[0] != 1) || (cmd[1] != 1) ||
	    (cmd[2] != '.') || (cmd[3] != '.')) {
		return (false);
	}

	if (!collection_resolv_prefix(cf->cf_collections))
		return (false);

	if (enabled == 0) {
		logmsg_err("No collections is available");
		return (false);
	}

	return (true);
}

bool
collection_exchange_list(int sock, struct collection *cl)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t namelen, relnamelen, len;

	if ((namelen = strlen(cl->cl_name)) >= sizeof(cl->cl_name))
		return (false);
	if ((relnamelen = strlen(cl->cl_release)) >= sizeof(cl->cl_release))
		return (false);
	if ((len = namelen + relnamelen + 4) >= sizeof(cmd))
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = (uint8_t)namelen;
	cmd[3] = (uint8_t)relnamelen;
	if (!sock_send(sock, cmd, 4))
		return (false);
	if (!sock_send(sock, cl->cl_name, namelen))
		return (false);
	if (!sock_send(sock, cl->cl_release, relnamelen))
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if ((len = GetWord(cmd)) > sizeof(cmd) - 2)
		return (false);
	if (len == 0) {
		logmsg("Not found such a collection %s/%s", cl->cl_name,
		       cl->cl_release);
		cl->cl_flags |= CLFLAGS_DISABLE;
		return (true);
	}
	if (len != namelen + relnamelen + 2)
		return (false);

	if (!sock_recv(sock, cmd, len))
		return (false);

	if ((cmd[0] != namelen) || (cmd[1] != relnamelen)) {
		logmsg_err("Not match name/release length");
		return (false);
	}
	if ((memcmp(&cmd[2], cl->cl_name, namelen) != 0) ||
	    (memcmp(&cmd[namelen + 2], cl->cl_release, relnamelen) != 0)) {
		logmsg_err("Not match collection name/release");
		return (false);
	}

	logmsg_verbose(" collection name \"%s\" release \"%s\"",
		       cl->cl_name, cl->cl_release);

	return (true);
}

bool
collection_exchange_rcs(int sock, struct collection *cl)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t namelen, relnamelen, len;

	if ((namelen = strlen(cl->cl_name)) >= sizeof(cl->cl_name))
		return (false);
	if ((relnamelen = strlen(cl->cl_release)) >= sizeof(cl->cl_release))
		return (false);
	if ((len = namelen + relnamelen + 6) >= sizeof(cmd))
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = (uint8_t)namelen;
	cmd[3] = (uint8_t)relnamelen;
	if (!sock_send(sock, cmd, 4))
		return (false);
	if (!sock_send(sock, cl->cl_name, namelen))
		return (false);
	if (!sock_send(sock, cl->cl_release, relnamelen))
		return (false);

	SetWord(cmd, cl->cl_umask);
	if (!sock_send(sock, cmd, 2))
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if ((len = GetWord(cmd)) > sizeof(cmd) - 2)
		return (false);
	if (len == 0) {
		logmsg("Not found such a collection %s/%s", cl->cl_name,
		       cl->cl_release);
		cl->cl_flags |= CLFLAGS_DISABLE;
		return (true);
	}

	if (len < namelen + relnamelen + 4)
		return (false);

	if (!sock_recv(sock, cmd, len))
		return (false);

	if ((cmd[0] != namelen) || (cmd[1] != relnamelen)) {
		logmsg_err("Not match name/release length");
		return (false);
	}
	if ((memcmp(&cmd[2], cl->cl_name, namelen) != 0) ||
	    (memcmp(&cmd[namelen + 2], cl->cl_release, relnamelen) != 0)) {
		logmsg_err("Not match collection name/release");
		return (false);
	}

	cl->cl_umask = GetWord(&cmd[namelen + relnamelen + 2]);
	if (cl->cl_umask & ~CVSYNC_ALLPERMS)
		return (false);

	cl->cl_rprefixlen = len - namelen - relnamelen - 4;
	if (cl->cl_rprefixlen > sizeof(cl->cl_rprefix))
		return (false);
	(void)memcpy(cl->cl_rprefix, &cmd[namelen + relnamelen + 4],
		     cl->cl_rprefixlen);
	cl->cl_rprefix[cl->cl_rprefixlen] = '/';

	logmsg_verbose(" collection name \"%s\" release \"%s\" umask %03o",
		       cl->cl_name, cl->cl_release, cl->cl_umask);

	return (true);
}

bool
compress_exchange(int sock, struct config *cf)
{
	const char *name;
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t len;

	if (cf->cf_proto < CVSYNC_PROTO(0, 22)) {
		cf->cf_compress = CVSYNC_COMPRESS_NO;
		cf->cf_mss = MUX_MAX_MSS;
		return (true);
	}

	if (cf->cf_proto == CVSYNC_PROTO(0, 22))
		cf->cf_compress = CVSYNC_COMPRESS_NO;

	name = cvsync_compress_ntop(cf->cf_compress);

	len = strlen(name);
	SetWord(cmd, len);
	if (!sock_send(sock, cmd, 2))
		return (false);
	if (!sock_send(sock, name, len))
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len >= sizeof(cmd)))
		return (false);
	if (!sock_recv(sock, cmd, len))
		return (false);
	cmd[len] = '\0';

	cf->cf_compress = cvsync_compress_pton((const char *)cmd);
	if (cf->cf_compress == CVSYNC_COMPRESS_UNSPEC) {
		logmsg_err("Unsupported compression type: %s", name);
		return (false);
	}

	name = cvsync_compress_ntop(cf->cf_compress);
	len = strlen(name);
	SetWord(cmd, len);
	if (!sock_send(sock, cmd, 2))
		return (false);
	if (!sock_send(sock, name, len))
		return (false);

	if ((cf->cf_proto > CVSYNC_PROTO(0, 22)) &&
	    (cf->cf_compress != CVSYNC_COMPRESS_NO)) {
		cf->cf_mss = MUX_MAX_MSS_ZLIB;
	} else {
		cf->cf_mss = MUX_DEFAULT_MSS;
	}

	logmsg_verbose("Compression: %s", name);

	return (true);
}

struct mux *
channel_establish(int sock, struct config *cf)
{
	struct mux *mx;
	struct muxbuf *mxb;
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t len;
	int i;

	logmsg_verbose("Trying to establish the multiplexed channel...");

	if ((mx = mux_init(sock, cf->cf_mss, cf->cf_compress,
			   CVSYNC_COMPRESS_LEVEL_BEST)) == NULL) {
		return (NULL);
	}

	for (i = 0 ; i < MUX_MAXCHANNELS ; i++) {
		mxb = &mx->mx_buffer[MUX_IN][i];

		SetWord(cmd, 7);
		cmd[2] = (uint8_t)i;
		SetWord(&cmd[3], mxb->mxb_mss);
		SetDWord(&cmd[5], mxb->mxb_bufsize);
		if (!sock_send(sock, cmd, 9)) {
			mux_destroy(mx);
			return (NULL);
		}

		if (!sock_recv(sock, cmd, 2)) {
			mux_destroy(mx);
			return (NULL);
		}
		if ((len = GetWord(cmd)) != 7) {
			mux_destroy(mx);
			return (NULL);
		}
		if (!sock_recv(sock, cmd, len)) {
			mux_destroy(mx);
			return (NULL);
		}
		if (cmd[0] != i) {
			mux_destroy(mx);
			return (NULL);
		}

		mxb = &mx->mx_buffer[MUX_OUT][i];

		if (!muxbuf_init(mxb, GetWord(&cmd[1]), GetDWord(&cmd[3]),
				 cf->cf_compress)) {
			mux_destroy(mx);
			return (NULL);
		}
	}

	return (mx);
}
