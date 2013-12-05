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

#ifndef __RCSLIB_H__
#define	__RCSLIB_H__

#define	RCSLIB_HASH_INIT	(5381)
#define	RCSLIB_HASH(h, k)	((uint32_t)(((h) * 33) + (int)(k)))
#define	RCSLIB_HASH_END(h)	((uint32_t)((h) + ((h) >> 5)))

/*
 * rcsfile(5) - Manual Page Revision: 1.5.2.1; Release Date: 2001/07/22
 */

struct rcsid {
	char	*i_id;
	size_t	i_len;
};
#define	RCSID_SIZE		(sizeof(struct rcsid))

struct rcsnum {
	char	*n_str;
	size_t	n_len;
	size_t	n_level;
#define	RCSNUM_MAXLEVEL		(16)	/* heuristic */
	int32_t	n_num[RCSNUM_MAXLEVEL];
};
#define	RCSNUM_SIZE		(sizeof(struct rcsnum))
#define	RCSNUM_MAX		(10000000)	/* heuristic */

struct rcsstr {
	char	*s_str;
	size_t	s_len;
};

struct rcssym {
	char	*s_sym;
	size_t	s_len;
};
#define	RCSSYM_SIZE		(sizeof(struct rcssym))

struct rcslib_access {
	size_t		ra_size, ra_count;
	struct rcsid	*ra_id;
};

struct rcslib_branches {
	size_t		rb_size, rb_count;
	struct rcsnum	*rb_num;
};

struct rcslib_date {
	struct rcsnum	rd_num;
	struct tm	rd_tm;
};

struct rcslib_delta {
	size_t			rd_size, rd_count;
	struct rcslib_revision	*rd_rev, **rd_rev_deltatext;
};

struct rcslib_lock {
	struct rcsid	id;
	struct rcsnum	num;
};
#define	RCSLIB_LOCK_SIZE	(sizeof(struct rcslib_lock))

struct rcslib_locks {
	size_t			rl_size, rl_count;
	struct rcslib_lock	*rl_locks;
	int			rl_strict;
};

struct rcslib_revision {
	struct rcsnum		num;
	struct rcslib_date	date;
	struct rcsid		author;
	struct rcsid		state;
	struct rcslib_branches	branches;
	struct rcsnum		next;
	struct rcsstr		log;
	struct rcsstr		text;

	/* internal use */
	struct rcslib_revision	*rv_next;
	int			rv_flags;
#define	RCSLIB_REVISION_DELTATEXT	(0x0001)
};
#define	RCSLIB_REVISION_SIZE	(sizeof(struct rcslib_revision))

struct rcslib_symbol {
	struct rcssym	sym;
	struct rcsnum	num;
};
#define	RCSLIB_SYMBOL_SIZE	(sizeof(struct rcslib_symbol))

struct rcslib_symbols {
	size_t			rs_size, rs_count;
	struct rcslib_symbol	*rs_symbols;
};

struct rcslib_file {
	/* admin */
	struct rcsnum		head;
	struct rcsnum		branch;
	struct rcslib_access	access;
	struct rcslib_symbols	symbols;
	struct rcslib_locks	locks;
	struct rcsstr		comment;
	struct rcsstr		expand;

	/* delta/deltatext */
	struct rcslib_delta	delta;

	/* desc */
	struct rcsstr		desc;
};

/* "[ad]<lineno> <count>\n" */
struct rcslib_rcsdiff {
	char	rd_cmd;
	size_t	rd_lineno;
	size_t	rd_count;
};

struct rcslib_file *rcslib_init(void *, off_t);
void rcslib_destroy(struct rcslib_file *);

struct rcslib_revision *rcslib_lookup_revision(struct rcslib_file *,
					       const struct rcsnum *);
struct rcslib_revision *rcslib_lookup_symbol(struct rcslib_file *,
					     void *, size_t);

bool rcslib_write_delta(int, const struct rcslib_revision *);
bool rcslib_write_deltatext(int, const struct rcslib_revision *);

bool rcslib_str2num(void *, size_t, struct rcsnum *);

int rcslib_cmp_lock(const struct rcslib_lock *, const struct rcslib_lock *);
int rcslib_cmp_symbol(const struct rcslib_symbol *,
		      const struct rcslib_symbol *);
int rcslib_cmp_id(const struct rcsid *, const struct rcsid *);
int rcslib_cmp_num(const struct rcsnum *, const struct rcsnum *);
int rcslib_cmp_sym(const struct rcssym *, const struct rcssym *);

bool rcslib_is_ancestor(const struct rcsnum *, const struct rcsnum *);

#endif /* __RCSLIB_H__ */
