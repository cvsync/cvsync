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

#ifndef __DIRSCAN_H__
#define	__DIRSCAN_H__

struct collection;
struct mux;

struct dirscan_args {
	struct mux		*dsa_mux;
	struct collection	*dsa_collections, *dsa_collection;
	uint32_t		dsa_proto;
	pthread_t		dsa_thread;
	void			*dsa_status;

	char			dsa_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*dsa_rpath;
	char			dsa_symlink[PATH_MAX + CVSYNC_NAME_MAX + 1];
	size_t			dsa_pathmax, dsa_pathlen;
	size_t			dsa_namemax;

	uint8_t			dsa_cmd[CVSYNC_MAXCMDLEN];
	size_t			dsa_cmdmax;
};

struct dirscan_args *dirscan_init(struct mux *, struct collection *, uint32_t);
void dirscan_destroy(struct dirscan_args *);
void *dirscan(void *);
bool dirscan_start(struct dirscan_args *, const char *, const char *);
bool dirscan_end(struct dirscan_args *);

bool dirscan_rcs(struct dirscan_args *);
bool dirscan_rcs_scanfile(struct dirscan_args *);

#endif /* __DIRSCAN_H__ */
