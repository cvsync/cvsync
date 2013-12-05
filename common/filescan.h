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

#ifndef __FILESCAN_H__
#define	__FILESCAN_H__

struct collection;
struct cvsync_file;
struct hash_args;
struct mux;
struct refuse_args;

#define	FILESCAN_START		(0x80)
#define	FILESCAN_END		(0x81)

#define	FILESCAN_ADD		(0x00)
#define	FILESCAN_REMOVE		(0x01)
#define	FILESCAN_SETATTR	(0x02)
#define	FILESCAN_UPDATE		(0x03)
#define	FILESCAN_RCS_ATTIC	(0x04)

struct filescan_args {
	struct mux		*fsa_mux;
	struct collection	*fsa_collections;
	struct refuse_args	*fsa_refuse;
	uint32_t		fsa_proto;
	pthread_t		fsa_thread;
	void			*fsa_status;

	char			fsa_name[CVSYNC_NAME_MAX + 1];
	char			fsa_release[CVSYNC_NAME_MAX + 1];

	char			fsa_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*fsa_rpath;
	size_t			fsa_pathmax, fsa_pathlen, fsa_namemax;

	uint8_t			fsa_tag, fsa_cmd[CVSYNC_MAXCMDLEN];
	size_t			fsa_cmdmax;
	struct cvsync_attr	fsa_attr;
	uint16_t		fsa_umask;

	void			*fsa_hash_ctx;
	const struct hash_args	*fsa_hash_ops;
};

struct filescan_args *filescan_init(struct mux *, struct collection *,
				    uint32_t, int);
void filescan_destroy(struct filescan_args *);
void *filescan(void *);
bool filescan_start(struct filescan_args *, const char *, const char *);
bool filescan_end(struct filescan_args *);

bool filescan_rcs(struct filescan_args *);

bool filescan_generic_update(struct filescan_args *, struct cvsync_file *);
bool filescan_rdiff_update(struct filescan_args *, struct cvsync_file *);

#endif /* __FILESCAN_H__ */
