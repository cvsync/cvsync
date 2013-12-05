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

#ifndef __FILECMP_H__
#define	__FILECMP_H__

struct collection;
struct cvsync_attr;
struct cvsync_file;
struct hash_args;
struct mux;

#define	FILECMP_START		(0x80)
#define	FILECMP_END		(0x81)

#define	FILECMP_ADD		(0x00)
#define	FILECMP_REMOVE		(0x01)
#define	FILECMP_SETATTR		(0x02)
#define	FILECMP_UPDATE		(0x03)
#define	FILECMP_RCS_ATTIC	(0x04)

/* UPDATE */
#define	FILECMP_UPDATE_END	(0x81)

#define	FILECMP_UPDATE_GENERIC	(0x00)
#define	FILECMP_UPDATE_RCS	(0x01)
#define	FILECMP_UPDATE_RDIFF	(0x02)

#define	FILECMP_UPDATE_RCS_HEAD		(0x00)
#define	FILECMP_UPDATE_RCS_BRANCH	(0x01)
#define	FILECMP_UPDATE_RCS_ACCESS	(0x02)
#define	FILECMP_UPDATE_RCS_SYMBOLS	(0x03)
#define	FILECMP_UPDATE_RCS_LOCKS	(0x04)
#define	FILECMP_UPDATE_RCS_COMMENT	(0x05)
#define	FILECMP_UPDATE_RCS_EXPAND	(0x06)
#define	FILECMP_UPDATE_RCS_DELTA	(0x07)
#define	FILECMP_UPDATE_RCS_DESC		(0x08)
#define	FILECMP_UPDATE_RCS_DELTATEXT	(0x09)

struct filecmp_args {
	struct mux		*fca_mux;
	const char		*fca_hostinfo;
	struct collection	*fca_collections_list, *fca_collections;
	struct collection	*fca_collection;
	uint32_t		fca_proto;
	pthread_t		fca_thread;
	void			*fca_status;

	char			fca_name[CVSYNC_NAME_MAX + 1];
	char			fca_release[CVSYNC_NAME_MAX + 1];

	char			fca_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*fca_rpath;
	size_t			fca_pathmax, fca_pathlen, fca_rpathlen;
	size_t			fca_namemax;

	uint8_t			fca_tag, fca_cmd[CVSYNC_MAXCMDLEN];
	uint8_t			fca_rvcmd[CVSYNC_MAXCMDLEN];
	size_t			fca_cmdmax;
	struct cvsync_attr	fca_attr;
	uint16_t		fca_umask;

	void			*fca_hash_ctx;
	const struct hash_args	*fca_hash_ops;
	uint8_t			fca_hash[HASH_MAXLEN];
};

struct filecmp_args *filecmp_init(struct mux *, struct collection *,
				  struct collection *, const char *, uint32_t,
				  int);
void filecmp_destroy(struct filecmp_args *);
void *filecmp(void *);
bool filecmp_start(struct filecmp_args *, const char *, const char *);
bool filecmp_end(struct filecmp_args *);
int filecmp_access(struct filecmp_args *, struct cvsync_attr *);

bool filecmp_list(struct filecmp_args *);
bool filecmp_rcs(struct filecmp_args *);

bool filecmp_generic_update(struct filecmp_args *, struct cvsync_file *);
bool filecmp_rdiff_update(struct filecmp_args *, struct cvsync_file *);
bool filecmp_rdiff_ignore(struct filecmp_args *);
bool filecmp_rdiff_ischanged(struct filecmp_args *, struct cvsync_file *);

#endif /* __FILECMP_H__ */
