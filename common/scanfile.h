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

#ifndef __SCANFILE_H__
#define	__SCANFILE_H__

struct cvsync_file;

struct scanfile_attr {
	size_t		a_size;
	uint8_t		a_type;
	void		*a_name;
	size_t		a_namelen;
	void		*a_aux;
	size_t		a_auxlen;
};

struct scanfile_args {
	struct cvsync_file	*sa_scanfile;
	const char		*sa_scanfile_name;
	uint8_t			*sa_start, *sa_end;

	int			sa_tmp;
	char			sa_tmp_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	mode_t			sa_tmp_mode;

	struct list		*sa_dirlist;
	struct scanfile_attr	sa_attr;

	bool			sa_changed;
};

struct scanfile_create_args {
	const char		*sca_name, *sca_prefix, *sca_release;
	const char		*sca_rprefix;
	size_t			sca_rprefixlen;
	mode_t			sca_mode;
	struct mdirent_args	*sca_mdirent_args;
	mode_t			sca_umask;
};

void scanfile_init(struct scanfile_args *);
struct scanfile_args *scanfile_open(const char *);
void scanfile_close(struct scanfile_args *);
bool scanfile_read_attr(uint8_t *, const uint8_t *, struct scanfile_attr *);
bool scanfile_write_attr(struct scanfile_args *, struct scanfile_attr *);

bool scanfile_create_tmpfile(struct scanfile_args *, mode_t);
void scanfile_remove_tmpfile(struct scanfile_args *);
bool scanfile_rename(struct scanfile_args *);

bool scanfile_add(struct scanfile_args *, uint8_t, void *, size_t, void *,
		  size_t);
bool scanfile_remove(struct scanfile_args *, uint8_t, void *, size_t);
bool scanfile_replace(struct scanfile_args *, uint8_t, void *, size_t, void *,
		      size_t);
bool scanfile_update(struct scanfile_args *, uint8_t, void *, size_t, void *,
		     size_t);

bool scanfile_create(struct scanfile_create_args *);
bool scanfile_rcs(struct scanfile_create_args *);

#endif /* __SCANFILE_H__ */
