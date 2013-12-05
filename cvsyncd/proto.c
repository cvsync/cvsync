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

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "network.h"
#include "version.h"

#include "defs.h"

void protocol_compat_0(uint8_t, int, uint8_t, uint8_t *, uint8_t *);
void protocol_compat_0_19(uint8_t *, uint8_t *);
void protocol_compat_0_20(int, uint8_t *, uint8_t *);
void protocol_compat_0_21(int, uint8_t, uint8_t *, uint8_t *);

bool collection_fetch(int, uint8_t *, size_t *, uint8_t *, size_t *, uint8_t *,
		      size_t *);

bool compress_exchange(int, struct config *, uint32_t, int *);

bool
protocol_exchange(int sock, int status, uint8_t error, uint32_t *proto)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN], r_mj, r_mn;
	size_t len;

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if ((len = GetWord(cmd)) != 2)
		return (false);
	if (!sock_recv(sock, cmd, len))
		return (false);

	r_mj = cmd[0];
	r_mn = cmd[1];

	switch (r_mj) {
	case 0:
		protocol_compat_0(r_mn, status, error, &cmd[2], &cmd[3]);
		break;
	default:
		cmd[2] = CVSYNC_PROTO_ERROR;
		cmd[3] = CVSYNC_ERROR_UNSPEC;
		break;
	}

	SetWord(cmd, 2);
	if (!sock_send(sock, cmd, 4))
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if ((len = GetWord(cmd)) != 2)
		return (false);
	if (!sock_recv(sock, cmd, len))
		return (false);

	if ((cmd[0] != r_mj) && (cmd[1] != r_mn))
		return (false);

	*proto = CVSYNC_PROTO(r_mj, r_mn);

	return (true);
}

void
protocol_compat_0(uint8_t r_mn, int status, uint8_t error, uint8_t *mj,
		  uint8_t *mn)
{
	switch (r_mn) {
	case 20:
		protocol_compat_0_20(status, mj, mn);
		break;
	case 21:
		protocol_compat_0_21(status, error, mj, mn);
		break;
	default:
		if (r_mn <= 19) {
			protocol_compat_0_19(mj, mn);
		} else {
			*mj = CVSYNC_PROTO_MAJOR;
			*mn = CVSYNC_PROTO_MINOR;
		}
		break;
	}
}

void
protocol_compat_0_19(uint8_t *mj, uint8_t *mn)
{
	*mj = CVSYNC_PROTO_ERROR;
	*mn = CVSYNC_ERROR_UNSPEC;
}

void
protocol_compat_0_20(int status, uint8_t *mj, uint8_t *mn)
{
	if ((status == ACL_ALLOW) || (status == ACL_ALWAYS)) {
		*mj = CVSYNC_PROTO_MAJOR;
		*mn = 20;
	} else {
		*mj = CVSYNC_PROTO_ERROR;
		*mn = CVSYNC_ERROR_UNSPEC;
	}
}

void
protocol_compat_0_21(int status, uint8_t error, uint8_t *mj, uint8_t *mn)
{
	switch (status) {
	case ACL_ALLOW:
	case ACL_ALWAYS:
		*mj = CVSYNC_PROTO_MAJOR;
		*mn = 21;
		break;
	case ACL_DENY:
		*mj = CVSYNC_PROTO_ERROR;
		*mn = error;
		break;
	default:
		*mj = CVSYNC_PROTO_ERROR;
		*mn = CVSYNC_ERROR_UNSPEC;
		break;
	}
}

int
hash_exchange(int sock, struct config *cf)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	char name[CVSYNC_NAME_MAX + 1];
	size_t len;
	int type;

	if (!sock_recv(sock, cmd, 2))
		return (HASH_UNSPEC);
	len = GetWord(cmd);
	if ((len == 0) || (len >= sizeof(name)))
		return (HASH_UNSPEC);
	if (!sock_recv(sock, name, len))
		return (HASH_UNSPEC);

	if ((type = hash_pton(name, len)) == HASH_UNSPEC)
		type = HASH_MD5;
	if (type != cf->cf_hash)
		type = HASH_MD5;

	if ((len = hash_ntop(type, name, sizeof(name))) == 0)
		return (HASH_UNSPEC);

	SetWord(cmd, len);
	if (!sock_send(sock, cmd, 2))
		return (false);
	if (!sock_send(sock, name, len))
		return (false);

	return (type);
}

struct collection *
collectionlist_exchange(int sock, struct config *cf)
{
	struct collection *cls = NULL, *cl;
	uint16_t mode_umask;
	uint8_t name[CVSYNC_NAME_MAX + 1], relname[CVSYNC_NAME_MAX + 1];
	uint8_t cmd[CVSYNC_MAXCMDLEN], aux[CVSYNC_MAXAUXLEN];
	size_t namelen, relnamelen, auxlen, len;
	int type;
	bool recv_list = false;

	for (;;) {
		namelen = sizeof(name);
		relnamelen = sizeof(relname);
		auxlen = sizeof(aux);
		if (!collection_fetch(sock, name, &namelen, relname,
				      &relnamelen, aux, &auxlen)) {
			collection_destroy_all(cls);
			return (NULL);
		}

		if ((namelen == 1) && (name[0] == '.') &&
		    (relnamelen == 1) && (relname[0] == '.') &&
		    (auxlen == 0)) {
			break;
		}

		if (recv_list) {
			collection_destroy_all(cls);
			return (NULL);
		}

		switch (cvsync_release_pton((char *)relname)) {
		case CVSYNC_RELEASE_LIST:
			if (auxlen != 0) {
				collection_destroy_all(cls);
				return (NULL);
			}
			if (cls != NULL) {
				collection_destroy_all(cls);
				return (NULL);
			}
			type = cvsync_list_pton((char *)name);
			if (type == CVSYNC_LIST_UNKNOWN) {
				SetWord(cmd, 0);
				if (!sock_send(sock, cmd, 2)) {
					collection_destroy_all(cls);
					return (NULL);
				}
				continue;
			}
			if ((cl = malloc(sizeof(*cl))) == NULL) {
				logmsg_err("%s", strerror(errno));
				collection_destroy_all(cls);
				return (NULL);
			}
			(void)memset(cl, 0, sizeof(*cl));
			(void)memcpy(cl->cl_name, name, namelen);
			cl->cl_name[namelen] = '\0';
			(void)memcpy(cl->cl_release, relname, relnamelen);
			cl->cl_release[relnamelen] = '\0';
			auxlen = 0;
			recv_list = true;
			break;
		case CVSYNC_RELEASE_RCS:
			if (auxlen != 2) {
				collection_destroy_all(cls);
				return (NULL);
			}
			mode_umask = GetWord(aux);
			if (mode_umask & ~CVSYNC_ALLPERMS) {
				collection_destroy_all(cls);
				return (NULL);
			}
			cl = collection_lookup(cf->cf_collections,
					       (const char *)name,
					       (const char *)relname);
			if (cl == NULL) {
				SetWord(cmd, 0);
				if (!sock_send(sock, cmd, 2)) {
					collection_destroy_all(cls);
					return (NULL);
				}
				continue;
			}
			mode_umask &= cl->cl_umask;

			SetWord(aux, mode_umask);
			if (cl->cl_rprefixlen > 0) {
				(void)memcpy(&aux[2], cl->cl_rprefix,
					     cl->cl_rprefixlen);
			}
			auxlen = cl->cl_rprefixlen + 2;
			break;
		default:
			collection_destroy_all(cls);
			return (NULL);
		}

		if ((len = namelen + relnamelen + 4) + auxlen >= sizeof(cmd)) {
			collection_destroy(cl);
			collection_destroy_all(cls);
			return (NULL);
		}
		SetWord(cmd, len + auxlen - 2);
		cmd[2] = (uint8_t)namelen;
		cmd[3] = (uint8_t)relnamelen;
		if (!sock_send(sock, cmd, 4)) {
			collection_destroy(cl);
			collection_destroy_all(cls);
			return (NULL);
		}
		if (!sock_send(sock, name, namelen)) {
			collection_destroy(cl);
			collection_destroy_all(cls);
			return (NULL);
		}
		if (!sock_send(sock, relname, relnamelen)) {
			collection_destroy(cl);
			collection_destroy_all(cls);
			return (NULL);
		}
		if (!sock_send(sock, aux, auxlen)) {
			collection_destroy(cl);
			collection_destroy_all(cls);
			return (NULL);
		}

		if (cls == NULL) {
			cls = cl;
		} else {
			struct collection *prev;

			for (prev = cls ;
			     prev->cl_next != NULL ;
			     prev = prev->cl_next) {
				/* Nothing to do */;
			}
			prev->cl_next = cl;
		}
	}

	SetWord(cmd, 4);
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = '.';
	cmd[5] = '.';
	if (!sock_send(sock, cmd, 6)) {
		collection_destroy_all(cls);
		return (NULL);
	}

	if (cls == NULL)
		return (NULL);

	return (cls);
}

bool
collection_fetch(int sock, uint8_t *name, size_t *namelen, uint8_t *relname,
		 size_t *relnamelen, uint8_t *aux, size_t *auxlen)
{
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t namemax = *namelen, relnamemax = *relnamelen, len;

	if (!sock_recv(sock, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len <= 2) || (len >= sizeof(cmd) - 2))
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if ((cmd[0] == 0) || (cmd[0] >= namemax) ||
	    (cmd[1] == 0) || (cmd[1] >= relnamemax) ||
	    ((size_t)cmd[0] + (size_t)cmd[1] + 2 > len)) {
		return (false);
	}
	if (!sock_recv(sock, name, (size_t)cmd[0]))
		return (false);
	if (!sock_recv(sock, relname, (size_t)cmd[1]))
		return (false);
	name[cmd[0]] = '\0';
	*namelen = cmd[0];
	relname[cmd[1]] = '\0';
	*relnamelen = cmd[1];
	if ((*auxlen = len - cmd[0] - cmd[1] - 2) > 0) {
		if (!sock_recv(sock, aux, *auxlen))
			return (false);
	}

	return (true);
}

struct mux *
channel_establish(int sock, struct config *cf, uint32_t proto)
{
	struct mux *mx;
	struct muxbuf *mxb;
	uint16_t mss;
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t len;
	int compression, i;

	if (!compress_exchange(sock, cf, proto, &compression))
		return (NULL);

	if ((proto > CVSYNC_PROTO(0, 22)) &&
	    (compression != CVSYNC_COMPRESS_NO)) {
		mss = MUX_MAX_MSS_ZLIB;
	} else {
		mss = MUX_DEFAULT_MSS;
	}

	if ((mx = mux_init(sock, mss, compression,
			   cf->cf_compress_level)) == NULL) {
		return (NULL);
	}

	for (i = 0 ; i < MUX_MAXCHANNELS ; i++) {
		mxb = &mx->mx_buffer[MUX_OUT][i];

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

		if (!muxbuf_init(mxb, GetWord(&cmd[1]), GetDWord(&cmd[3]),
				 compression)) {
			mux_destroy(mx);
			return (NULL);
		}

		mxb = &mx->mx_buffer[MUX_IN][i];

		SetWord(cmd, 7);
		cmd[2] = (uint8_t)i;
		SetWord(&cmd[3], mxb->mxb_mss);
		SetDWord(&cmd[5], mxb->mxb_bufsize);
		if (!sock_send(sock, cmd, 9)) {
			mux_destroy(mx);
			return (NULL);
		}
	}

	return (mx);
}

bool
compress_exchange(int sock, struct config *cf, uint32_t proto,
		  int *compression)
{
	const char *name;
	uint8_t cmd[CVSYNC_MAXCMDLEN];
	size_t len;

	if (proto < CVSYNC_PROTO(0, 22)) {
		*compression = CVSYNC_COMPRESS_NO;
		return (true);
	}

	if (!sock_recv(sock, cmd, 2))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len >= sizeof(cmd)))
		return (false);
	if (!sock_recv(sock, cmd, len))
		return (false);
	cmd[len] = '\0';

	*compression = cvsync_compress_pton((const char *)cmd);
	if (cf->cf_compress == CVSYNC_COMPRESS_NO)
		*compression = CVSYNC_COMPRESS_NO;
	if (proto == CVSYNC_PROTO(0, 22))
		*compression = CVSYNC_COMPRESS_NO;

	name = cvsync_compress_ntop(*compression);

	len = strlen(name);
	if (len > (size_t)UINT16_MAX) {
		return (false);
	}
	SetWord(cmd, len);
	if (!sock_send(sock, cmd, 2))
		return (false);
	if (!sock_send(sock, name, len))
		return (false);

	if (*compression == CVSYNC_COMPRESS_UNSPEC)
		return (false);

	if (!sock_recv(sock, cmd, 2))
		return (false);
	if (GetWord(cmd) != (uint16_t)len)
		return (false);
	if (!sock_recv(sock, cmd, len))
		return (false);
	cmd[len] = '\0';

	if (*compression != cvsync_compress_pton((const char *)cmd))
		return (false);

	return (true);
}
