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

#ifndef __DEFS_H__
#define	__DEFS_H__

struct config_include;

struct config {
	char			cf_addr[CVSYNC_MAXHOST];
	char			cf_serv[CVSYNC_MAXSERV];
	size_t			cf_maxclients;
	char			cf_base[PATH_MAX], cf_base_prefix[PATH_MAX];
	char			cf_access_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			cf_halt_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			cf_pid_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	uint8_t			cf_hash;
	struct collection	*cf_collections;
	struct config_include	*cf_includes;
	int			cf_refcnt;
};

struct config_include {
	struct config_include	*ci_next;
	char			ci_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	time_t			ci_mtime;
};

struct config *config_load(const char *);
void config_destroy(struct config *);
void config_acquire(struct config *);
void config_revoke(struct config *);
bool config_ischanged(struct config *);

#endif /* __DEFS_H__ */
