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

#include <sys/types.h>
#include <sys/uio.h>

#include <stdlib.h>

#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"

#include "rcslib.h"

struct rcs_keyword {
	const char	*name;
	const size_t	namelen;
};

static const struct rcs_keyword rcs_keywords[] = {
	{ "access",	6 },
	{ "author",	6 },
	{ "branch",	6 },
	{ "branches",	8 },
	{ "comment",	7 },
	{ "date",	4 },
	{ "desc",	4 },
	{ "expand",	6 },
	{ "head",	4 },
	{ "locks",	5 },
	{ "log",	3 },
	{ "next",	4 },
	{ "state",	5 },
	{ "strict",	6 },
	{ "symbols",	7 },
	{ "text",	4 },
	{ NULL,		0 }
};

enum {
	RCS_ACCESS	= 0,
	RCS_AUTHOR,
	RCS_BRANCH,
	RCS_BRANCHES,
	RCS_COMMENT,
	RCS_DATE,
	RCS_DESC,
	RCS_EXPAND,
	RCS_HEAD,
	RCS_LOCKS,
	RCS_LOG,
	RCS_NEXT,
	RCS_STATE,
	RCS_STRICT,
	RCS_SYMBOLS,
	RCS_TEXT
};

#define	RCS_SKIP(p, e)			\
	while (isspace((int)(*(p)))) {	\
		if (++(p) > (e))	\
			return (NULL);	\
	}

#define	RCS_SKIP_NORET(p, e)		\
	while (isspace((int)(*(p)))) {	\
		if (++(p) > (e))	\
			break;		\
	}

#define	IS_RCS_IDCHAR(p)					\
	(isalnum((int)(p)) ||					\
	(strchr("!\"#%&'()*+-/<=>?[\\]^_`{|}~", (p)) != NULL))

void rcslib_destroy_admin(struct rcslib_file *);
void rcslib_destroy_delta(struct rcslib_file *);

bool rcslib_parse_rcstext(struct rcslib_file *, char *, const char *);

char *rcslib_parse_admin(struct rcslib_file *, char *, const char *);
char *rcslib_parse_delta(struct rcslib_file *, char *, const char *);
char *rcslib_parse_deltatext(struct rcslib_file *, char *, const char *);

char *rcslib_parse_access(char *, const char *, struct rcslib_access *);
char *rcslib_parse_symbols(char *, const char *, struct rcslib_symbols *);
char *rcslib_parse_locks(char *, const char *, struct rcslib_locks *);
char *rcslib_parse_date(char *, const char *, struct rcslib_date *);
char *rcslib_parse_branches(char *, const char *, struct rcslib_branches *);
char *rcslib_parse_newphrase(char *, const char *);

char *rcslib_parse_word(char *, const char *);
char *rcslib_parse_num(char *, const char *, struct rcsnum *);
char *rcslib_parse_id(char *, const char *, struct rcsid *);
char *rcslib_parse_sym(char *, const char *, struct rcssym *);
char *rcslib_parse_string(char *, const char *, struct rcsstr *);

void rcslib_sort_revision(struct rcslib_file *);

typedef int (*rcslib_cmp_func)(const void *, const void *);

struct rcslib_file *
rcslib_init(void *addr, off_t addrlen)
{
	struct rcslib_file *rcs;
	char *sp = addr, *bp = sp + (size_t)(addrlen - 1);

	if ((addrlen == 0) || (*bp != '\n'))
		return (NULL);

	if ((rcs = malloc(sizeof(*rcs))) == NULL)
		return (NULL);
	(void)memset(rcs, 0, sizeof(*rcs));

	if (!rcslib_parse_rcstext(rcs, sp, bp)) {
		free(rcs);
		return (NULL);
	}

	return (rcs);
}

void
rcslib_destroy(struct rcslib_file *rcs)
{
	rcslib_destroy_admin(rcs);
	rcslib_destroy_delta(rcs);
	free(rcs);
}

void
rcslib_destroy_admin(struct rcslib_file *rcs)
{
	if (rcs->access.ra_id != NULL)
		free(rcs->access.ra_id);
	if (rcs->symbols.rs_symbols != NULL)
		free(rcs->symbols.rs_symbols);
	if (rcs->locks.rl_locks != NULL)
		free(rcs->locks.rl_locks);
}

void
rcslib_destroy_delta(struct rcslib_file *rcs)
{
	struct rcslib_revision *rev;
	size_t i;

	if (rcs->delta.rd_count > 0) {
		for (i = 0 ; i < rcs->delta.rd_count ; i++) {
			rev = &rcs->delta.rd_rev[i];
			if (rev->branches.rb_num != NULL)
				free(rev->branches.rb_num);
		}
	}
	free(rcs->delta.rd_rev);
}

bool
rcslib_parse_rcstext(struct rcslib_file *rcs, char *sp, const char *bp)
{
	const struct rcs_keyword *rcskey = &rcs_keywords[RCS_DESC];
	struct rcslib_revision *rev;
	char *p;
	size_t i;

	if ((sp = rcslib_parse_admin(rcs, sp, bp)) == NULL)
		return (false);

	if ((sp = rcslib_parse_delta(rcs, sp, bp)) == NULL) {
		rcslib_destroy_admin(rcs);
		return (false);
	}
	if (rcs->delta.rd_count > 0)
		rcslib_sort_revision(rcs);

	/* desc string */
	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		rcslib_destroy_admin(rcs);
		rcslib_destroy_delta(rcs);
		return (false);
	}
	sp += rcskey->namelen;

	p = sp;
	while (isspace((int)(*sp))) {
		if (++sp > bp) {
			rcslib_destroy_admin(rcs);
			rcslib_destroy_delta(rcs);
			return (false);
		}
	}
	if (sp == p) {
		rcslib_destroy_admin(rcs);
		rcslib_destroy_delta(rcs);
		return (false);
	}

	if ((sp = rcslib_parse_string(sp, bp, &rcs->desc)) == NULL) {
		rcslib_destroy_admin(rcs);
		rcslib_destroy_delta(rcs);
		return (false);
	}

	while (isspace((int)(*sp))) {
		if (++sp > bp) {
			rcslib_destroy_admin(rcs);
			rcslib_destroy_delta(rcs);
			return (false);
		}
	}

	if ((sp = rcslib_parse_deltatext(rcs, sp, bp)) == NULL) {
		rcslib_destroy_admin(rcs);
		rcslib_destroy_delta(rcs);
		return (false);
	}

	for (p = sp ; isspace((int)(*p)) ; p++) {
		if (p == bp)
			break;
	}
	if (p != bp) {
		rcslib_destroy_admin(rcs);
		rcslib_destroy_delta(rcs);
		return (false);
	}

	if (rcs->delta.rd_count == 0)
		return (true);

	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		rev = &rcs->delta.rd_rev[i];
		if (!(rev->rv_flags & RCSLIB_REVISION_DELTATEXT)) {
			rcslib_destroy_admin(rcs);
			rcslib_destroy_delta(rcs);
			return (false);
		}
		if (rev->next.n_len == 0)
			continue;
		if (i + 1 == rcs->delta.rd_count)
			continue;
		if (rcslib_cmp_num(&rcs->delta.rd_rev[i + 1].num,
				   &rev->next) == 0) {
			rev->rv_next = &rcs->delta.rd_rev[i + 1];
			continue;
		}
		rev->rv_next = rcslib_lookup_revision(rcs, &rev->next);
		if (rev->rv_next == NULL) {
			rcslib_destroy_admin(rcs);
			rcslib_destroy_delta(rcs);
			return (false);
		}
	}

	return (true);
}

char *
rcslib_parse_admin(struct rcslib_file *rcs, char *sp, const char *bp)
{
	const struct rcs_keyword *rcskey;
	char *p;

	RCS_SKIP(sp, bp)

	/* head {num}; */
	rcskey = &rcs_keywords[RCS_HEAD];
	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		return (NULL);
	}
	sp += rcskey->namelen;

	p = sp;
	RCS_SKIP(sp, bp)
	if ((sp == p) && (*sp != ';'))
		return (NULL);

	p = sp;
	if ((sp = rcslib_parse_num(sp, bp, &rcs->head)) != NULL) {
		RCS_SKIP(sp, bp)
	} else {
		sp = p;
	}
	if (*sp++ != ';')
		return (NULL);
	RCS_SKIP(sp, bp)

	/* { branch {num}; } */
	rcskey = &rcs_keywords[RCS_BRANCH];
	if ((sp + rcskey->namelen < bp) &&
	    (memcmp(sp, rcskey->name, rcskey->namelen) == 0)) {
		sp += rcskey->namelen;
		p = sp;
		RCS_SKIP(sp, bp)
		if ((sp == p) && (*sp != ';'))
			return (NULL);

		p = sp;
		if ((sp = rcslib_parse_num(sp, bp, &rcs->branch)) != NULL) {
			RCS_SKIP(sp, bp)
		} else {
			sp = p;
		}
		if (*sp++ != ';')
			return (NULL);
		RCS_SKIP(sp, bp)
	}

	/* access {id}*; */
	if ((sp = rcslib_parse_access(sp, bp, &rcs->access)) == NULL)
		return (NULL);

	/* symbols {sym : num}*; */
	if ((sp = rcslib_parse_symbols(sp, bp, &rcs->symbols)) == NULL)
		return (NULL);

	/* locks {id : num}*; {strict ;} */
	if ((sp = rcslib_parse_locks(sp, bp, &rcs->locks)) == NULL)
		return (NULL);

	/* { comment {string}; } */
	rcskey = &rcs_keywords[RCS_COMMENT];
	if ((sp + rcskey->namelen < bp) &&
	    (memcmp(sp, rcskey->name, rcskey->namelen) == 0)) {
		sp += rcskey->namelen;
		RCS_SKIP(sp, bp)

		p = sp;
		if ((sp = rcslib_parse_string(sp, bp, &rcs->comment)) != NULL) {
			RCS_SKIP(sp, bp)
		} else {
			sp = p;
		}
		if (*sp++ != ';')
			return (NULL);
		RCS_SKIP(sp, bp)
	}

	/* { expand {string}; } */
	rcskey = &rcs_keywords[RCS_EXPAND];
	if ((sp + rcskey->namelen < bp) &&
	    (memcmp(sp, rcskey->name, rcskey->namelen) == 0)) {
		sp += rcskey->namelen;
		RCS_SKIP(sp, bp)

		p = sp;
		if ((sp = rcslib_parse_string(sp, bp, &rcs->expand)) != NULL) {
			RCS_SKIP(sp, bp)
		} else {
			sp = p;
		}
		if (*sp++ != ';')
			return (NULL);
		RCS_SKIP(sp, bp)
	}

	/* { newphrase }* */
	if ((sp = rcslib_parse_newphrase(sp, bp)) == NULL)
		return (NULL);

	return (sp);
}

char *
rcslib_parse_delta(struct rcslib_file *rcs, char *sp, const char *bp)
{
	const struct rcs_keyword *rcskey;
	struct rcslib_delta *delta = &rcs->delta;
	struct rcslib_revision *rev;
	char *p;

	for (;;) {
		if (delta->rd_count == delta->rd_size) {
			size_t size = delta->rd_size, new;

			new = (size == 0) ? 32 : size * 4;

			rev = malloc(new * RCSLIB_REVISION_SIZE);
			if (rev == NULL) {
				if (delta->rd_rev != NULL)
					free(delta->rd_rev);
				return (NULL);
			}
			if (size > 0) {
				(void)memcpy(rev, delta->rd_rev,
					     size * RCSLIB_REVISION_SIZE);
				free(delta->rd_rev);
			}
			(void)memset(rev + size, 0,
				     (new - size) * RCSLIB_REVISION_SIZE);
			delta->rd_rev = rev;
			delta->rd_size = new;
		}

		/* num */
		rev = &delta->rd_rev[delta->rd_count++];
		if ((sp = rcslib_parse_num(sp, bp, &rev->num)) == NULL) {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}

		/* date num; */
		if ((sp = rcslib_parse_date(sp, bp, &rev->date)) == NULL) {
			free(delta->rd_rev);
			return (NULL);
		}
		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}
		if (*sp++ != ';') {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}

		/* author id; */
		rcskey = &rcs_keywords[RCS_AUTHOR];
		if ((sp + rcskey->namelen > bp) ||
		    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
			free(delta->rd_rev);
			return (NULL);
		}
		sp += rcskey->namelen;

		p = sp;
		RCS_SKIP_NORET(sp, bp)
		if ((sp > bp) || (sp == p)) {
			free(delta->rd_rev);
			return (NULL);
		}

		if ((sp = rcslib_parse_id(sp, bp, &rev->author)) == NULL) {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if ((sp > bp) || (*sp++ != ';')) {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}

		/* state {id}; */
		rcskey = &rcs_keywords[RCS_STATE];
		if ((sp + rcskey->namelen > bp) ||
		    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
			free(delta->rd_rev);
			return (NULL);
		}
		sp += rcskey->namelen;

		p = sp;
		RCS_SKIP_NORET(sp, bp)
		if ((sp > bp) || ((sp == p) && (*sp != ';'))) {
			free(delta->rd_rev);
			return (NULL);
		}

		p = sp;
		if ((sp = rcslib_parse_id(sp, bp, &rev->state)) != NULL) {
			RCS_SKIP_NORET(sp, bp)
			if (sp > bp) {
				free(delta->rd_rev);
				return (NULL);
			}
		} else {
			sp = p;
		}

		if (*sp++ != ';') {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}

		/* branches {num}*; */
		if ((sp = rcslib_parse_branches(sp, bp,
						&rev->branches)) == NULL) {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}

		/* next {num}; */
		rcskey = &rcs_keywords[RCS_NEXT];
		if ((sp + rcskey->namelen > bp) ||
		    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
			free(delta->rd_rev);
			return (NULL);
		}
		sp += rcskey->namelen;

		p = sp;
		RCS_SKIP_NORET(sp, bp)
		if ((sp > bp) || ((sp == p) && (*sp != ';'))) {
			free(delta->rd_rev);
			return (NULL);
		}

		p = sp;
		if ((sp = rcslib_parse_num(sp, bp, &rev->next)) != NULL) {
			RCS_SKIP_NORET(sp, bp)
			if (sp > bp) {
				free(delta->rd_rev);
				return (NULL);
			}
		} else {
			sp = p;
		}

		if (*sp++ != ';') {
			free(delta->rd_rev);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(delta->rd_rev);
			return (NULL);
		}

		/* { newphrase }* */
		p = sp;
		if ((sp = rcslib_parse_newphrase(sp, bp)) == NULL) {
			sp = p;
			break;
		}
	}

	if (delta->rd_count == 0) {
		free(delta->rd_rev);
		delta->rd_rev = NULL;
	}

	return (sp);
}

char *
rcslib_parse_deltatext(struct rcslib_file *rcs, char *sp, const char *bp)
{
	const struct rcs_keyword *rcskey;
	struct rcslib_delta *delta = &rcs->delta;
	struct rcslib_revision *rev = NULL;
	struct rcsnum num;
	char *p;

	if (delta->rd_count == 0)
		return (sp);

	for (;;) {
		/* num */
		if ((sp = rcslib_parse_num(sp, bp, &num)) == NULL)
			return (NULL);

		if ((rev = rcslib_lookup_revision(rcs, &num)) == NULL)
			return (NULL);

		p = sp;
		RCS_SKIP(sp, bp)
		if (sp == p)
			return (NULL);

		/* log string */
		rcskey = &rcs_keywords[RCS_LOG];
		if ((sp + rcskey->namelen > bp) ||
		    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
			return (NULL);
		}
		sp += rcskey->namelen;

		p = sp;
		RCS_SKIP(sp, bp)
		if (sp == p)
			return (NULL);

		if ((sp = rcslib_parse_string(sp, bp, &rev->log)) == NULL)
			return (NULL);

		RCS_SKIP(sp, bp)

		/* { newphrase }* */
		p = sp;
		if ((sp = rcslib_parse_newphrase(sp, bp)) == NULL)
			sp = p;

		/* text string */
		rcskey = &rcs_keywords[RCS_TEXT];
		if ((sp + rcskey->namelen > bp) ||
		    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
			return (NULL);
		}
		sp += rcskey->namelen;

		p = sp;
		RCS_SKIP(sp, bp)
		if (sp == p)
			return (NULL);

		if ((sp = rcslib_parse_string(sp, bp, &rev->text)) == NULL)
			return (NULL);

		rev->rv_flags |= RCSLIB_REVISION_DELTATEXT;

		while (isspace((int)(*sp))) {
			if (sp == bp)
				break;
			sp++;
		}
		if (sp == bp)
			break;
	}

	return (sp);
}

char *
rcslib_parse_access(char *sp, const char *bp, struct rcslib_access *access)
{
	const struct rcs_keyword *rcskey = &rcs_keywords[RCS_ACCESS];
	struct rcsid *id;
	char *p;

	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		return (NULL);
	}
	sp += rcskey->namelen;

	p = sp;
	RCS_SKIP(sp, bp)
	if ((sp == p) && (*sp != ';'))
		return (NULL);
	if (*sp == ';') {
		sp++;
		RCS_SKIP(sp, bp)
		return (sp);
	}

	for (;;) {
		if (access->ra_count == access->ra_size) {
			size_t size = access->ra_size, new = size + 4;

			if ((id = malloc(new * RCSID_SIZE)) == NULL) {
				if (access->ra_id != NULL)
					free(access->ra_id);
				return (NULL);
			}
			if (size > 0) {
				(void)memcpy(id, access->ra_id,
					     size * RCSID_SIZE);
				free(access->ra_id);
			}
			access->ra_id = id;
			access->ra_size = new;
		}

		id = &access->ra_id[access->ra_count++];
		if ((sp = rcslib_parse_id(sp, bp, id)) == NULL) {
			free(access->ra_id);
			return (NULL);
		}

		p = sp;
		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(access->ra_id);
			return (NULL);
		}
		if (*sp == ';')
			break;
	}

	if (*sp++ != ';') {
		free(access->ra_id);
		return (NULL);
	}

	RCS_SKIP_NORET(sp, bp);
	if (sp > bp) {
		free(access->ra_id);
		return (NULL);
	}

	if (access->ra_count > 0) {
		qsort(access->ra_id, access->ra_count, RCSID_SIZE,
		      (rcslib_cmp_func)rcslib_cmp_id);
	}

	return (sp);
}

char *
rcslib_parse_symbols(char *sp, const char *bp, struct rcslib_symbols *symbols)
{
	const struct rcs_keyword *rcskey = &rcs_keywords[RCS_SYMBOLS];
	struct rcslib_symbol *sym;
	char *p;

	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		return (NULL);
	}
	sp += rcskey->namelen;

	p = sp;
	RCS_SKIP(sp, bp)
	if ((sp == p) && (*sp != ';'))
		return (NULL);
	if (*sp == ';') {
		sp++;
		RCS_SKIP(sp, bp)
		return (sp);
	}

	for (;;) {
		if (symbols->rs_count == symbols->rs_size) {
			size_t size = symbols->rs_size, new;

			new = (size == 0) ? 32 : size * 4;

			sym = malloc(new * RCSLIB_SYMBOL_SIZE);
			if (sym == NULL) {
				if (symbols->rs_symbols != NULL)
					free(symbols->rs_symbols);
				return (NULL);
			}
			if (size > 0) {
				(void)memcpy(sym, symbols->rs_symbols,
					     size * RCSLIB_SYMBOL_SIZE);
				free(symbols->rs_symbols);
			}
			symbols->rs_symbols = sym;
			symbols->rs_size = new;
		}

		sym = &symbols->rs_symbols[symbols->rs_count++];
		if ((sp = rcslib_parse_sym(sp, bp, &sym->sym)) == NULL) {
			free(symbols->rs_symbols);
			return (NULL);
		}

		if (*sp++ != ':')
			return (NULL);

		if ((sp = rcslib_parse_num(sp, bp, &sym->num)) == NULL) {
			free(symbols->rs_symbols);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(symbols->rs_symbols);
			return (NULL);
		}
		if (*sp == ';')
			break;
	}

	if (*sp++ != ';')
		return (NULL);

	RCS_SKIP_NORET(sp, bp)
	if (sp > bp) {
		free(symbols->rs_symbols);
		return (NULL);
	}

	if (symbols->rs_count > 0) {
		qsort(symbols->rs_symbols, symbols->rs_count,
		      RCSLIB_SYMBOL_SIZE, (rcslib_cmp_func)rcslib_cmp_symbol);
	}

	return (sp);
}

char *
rcslib_parse_locks(char *sp, const char *bp, struct rcslib_locks *locks)
{
	const struct rcs_keyword *rcskey = &rcs_keywords[RCS_LOCKS];
	struct rcslib_lock *lock;
	char *p;

	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		return (NULL);
	}
	sp += rcskey->namelen;

	p = sp;
	RCS_SKIP(sp, bp)
	if ((sp == p) && (*sp != ';'))
		return (NULL);
	if (*sp == ';') {
		sp++;
		goto do_parse_strict;
	}

	for (;;) {
		if (locks->rl_count == locks->rl_size) {
			size_t size = locks->rl_size, new = size + 4;

			if ((lock = malloc(new * RCSLIB_LOCK_SIZE)) == NULL) {
				if (locks->rl_locks != NULL)
					free(locks->rl_locks);
				return (NULL);
			}
			if (size > 0) {
				(void)memcpy(lock, locks->rl_locks,
					     size * RCSLIB_LOCK_SIZE);
				free(locks->rl_locks);
			}
			locks->rl_locks = lock;
			locks->rl_size = new;
		}

		lock = &locks->rl_locks[locks->rl_count++];
		if ((sp = rcslib_parse_id(sp, bp, &lock->id)) == NULL) {
			free(locks->rl_locks);
			return (NULL);
		}

		if (*sp++ != ':') {
			free(locks->rl_locks);
			return (NULL);
		}

		if ((sp = rcslib_parse_num(sp, bp, &lock->num)) == NULL) {
			free(locks->rl_locks);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(locks->rl_locks);
			return (NULL);
		}
		if (*sp == ';')
			break;
	}

	if (*sp++ != ';') {
		free(locks->rl_locks);
		return (NULL);
	}

do_parse_strict:
	RCS_SKIP_NORET(sp, bp)
	if (sp > bp) {
		if (locks->rl_locks != NULL)
			free(locks->rl_locks);
		return (NULL);
	}

	rcskey = &rcs_keywords[RCS_STRICT];
	if ((sp + rcskey->namelen < bp) &&
	    (memcmp(sp, rcskey->name, rcskey->namelen) == 0)) {
		sp += rcskey->namelen;
		RCS_SKIP_NORET(sp, bp)
		if ((sp > bp) || (*sp++ != ';')) {
			if (locks->rl_locks != NULL)
				free(locks->rl_locks);
			return (NULL);
		}
		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			if (locks->rl_locks != NULL)
				free(locks->rl_locks);
			return (NULL);
		}

		locks->rl_strict = 1;
	}

	if (locks->rl_count > 0) {
		qsort(locks, locks->rl_count, RCSLIB_LOCK_SIZE,
		      (rcslib_cmp_func)rcslib_cmp_lock);
	}

	return (sp);
}

char *
rcslib_parse_date(char *sp, const char *bp, struct rcslib_date *date)
{
	const struct rcs_keyword *rcskey = &rcs_keywords[RCS_DATE];
	char *sv_sp, *p;
	int n, c;

	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		return (NULL);
	}
	sp += rcskey->namelen;

	p = sp;
	RCS_SKIP(sp, bp)
	if (sp == p)
		return (NULL);

	sv_sp = sp;

	/*
	 * Y.mm.dd.hh.mm.ss
	 * Y: year, mm: 01-12, dd: 01-31, hh: 00-23, mm: 00-59, ss: 00-60
	 * Dates use the Gregorian calendar. Times use UTC.
	 */

	/* year */
	n = 0;
	while (isdigit((int)(*sp))) {
		c = *sp - '0';
		if (INT_MAX / 10 < n)
			return (NULL);
		n *= 10;
		if (INT_MAX - n < c)
			return (NULL);
		n += c;
		if (++sp > bp)
			return (NULL);
	}
	date->rd_tm.tm_year = n;
	if (n > 100)
		date->rd_tm.tm_year -= 1900;

	if (*sp++ != '.')
		return (NULL);

	/* month */
	if (sp + 2 > bp)
		return (NULL);
	if (!isdigit((int)(*sp)))
		return (NULL);
	n = (*sp++ - '0') * 10;
	if (!isdigit((int)(*sp)))
		return (NULL);
	n += *sp++ - '0';
	if ((n < 1) || (n > 12))
		return (NULL);
	date->rd_tm.tm_mon = n - 1;

	if (*sp++ != '.')
		return (NULL);

	/* day */
	if (sp + 2 > bp)
		return (NULL);
	if (!isdigit((int)(*sp)))
		return (NULL);
	n = (*sp++ - '0') * 10;
	if (!isdigit((int)(*sp)))
		return (NULL);
	n += *sp++ - '0';
	if ((n < 1) || (n > 31))
		return (NULL);
	date->rd_tm.tm_mday = n;

	if (*sp++ != '.')
		return (NULL);

	/* hour */
	if (sp + 2 > bp)
		return (NULL);
	if (!isdigit((int)(*sp)))
		return (NULL);
	n = (*sp++ - '0') * 10;
	if (!isdigit((int)(*sp)))
		return (NULL);
	n += *sp++ - '0';
	if (n > 23)
		return (NULL);
	date->rd_tm.tm_hour = n;

	if (*sp++ != '.')
		return (NULL);

	/* minute */
	if (sp + 2 > bp)
		return (NULL);
	if (!isdigit((int)(*sp)))
		return (NULL);
	n = (*sp++ - '0') * 10;
	if (!isdigit((int)(*sp)))
		return (NULL);
	n += *sp++ - '0';
	if (n > 59)
		return (NULL);
	date->rd_tm.tm_min = n;

	if (*sp++ != '.')
		return (NULL);

	/* second */
	if (sp + 2 > bp)
		return (NULL);
	if (!isdigit((int)(*sp)))
		return (NULL);
	n = (*sp++ - '0') * 10;
	if (!isdigit((int)(*sp)))
		return (NULL);
	n += *sp++ - '0';
	if (n > 60)
		return (NULL);
	date->rd_tm.tm_sec = n;

	date->rd_num.n_str = sv_sp;
	date->rd_num.n_len = sp - sv_sp;

	return (sp);
}

char *
rcslib_parse_branches(char *sp, const char *bp,
		      struct rcslib_branches *branches)
{
	const struct rcs_keyword *rcskey = &rcs_keywords[RCS_BRANCHES];
	struct rcsnum *num;
	char *p;

	if ((sp + rcskey->namelen > bp) ||
	    (memcmp(sp, rcskey->name, rcskey->namelen) != 0)) {
		return (NULL);
	}
	sp += rcskey->namelen;

	p = sp;
	RCS_SKIP(sp, bp)
	if ((sp == p) && (*sp != ';'))
		return (NULL);

	if (*sp == ';') {
		sp++;
		RCS_SKIP(sp, bp)
		return (sp);
	}

	for (;;) {
		if (branches->rb_count == branches->rb_size) {
			size_t size = branches->rb_size, new = size + 4;

			if ((num = malloc(new * RCSNUM_SIZE)) == NULL) {
				if (branches->rb_num != NULL)
					free(branches->rb_num);
				return (NULL);
			}
			if (size > 0) {
				(void)memcpy(num, branches->rb_num,
					     size * RCSNUM_SIZE);
				free(branches->rb_num);
			}
			branches->rb_num = num;
			branches->rb_size = new;
		}

		num = &branches->rb_num[branches->rb_count++];
		if ((sp = rcslib_parse_num(sp, bp, num)) == NULL) {
			free(branches->rb_num);
			return (NULL);
		}

		RCS_SKIP_NORET(sp, bp)
		if (sp > bp) {
			free(branches->rb_num);
			return (NULL);
		}
		if (*sp == ';')
			break;
	}

	if (*sp++ != ';') {
		free(branches->rb_num);
		return (NULL);
	}

	return (sp);
}

char *
rcslib_parse_newphrase(char *sp, const char *bp)
{
	const struct rcs_keyword *rcskey;
	struct rcsid id;
	char *p;

	for (;;) {
		p = sp;

		if ((sp = rcslib_parse_id(sp, bp, &id)) == NULL) {
			sp = p;
			break;
		}

		for (rcskey = rcs_keywords ; rcskey->name != NULL ; rcskey++) {
			if ((id.i_len >= rcskey->namelen) &&
			    (memcmp(id.i_id, rcskey->name, id.i_len) == 0)) {
				return (NULL);
			}
		}

		p = sp;
		RCS_SKIP(sp, bp)
		if ((sp == p) && (*sp != ';'))
			return (NULL);

		for (;;) {
			p = sp;
			if ((sp = rcslib_parse_word(sp, bp)) == NULL) {
				sp = p;
				break;
			}

			RCS_SKIP(sp, bp)

			if (*sp == ';')
				break;
		}

		if (*sp++ == ';')
			break;
	}

	RCS_SKIP(sp, bp)

	return (sp);
}

char *
rcslib_parse_word(char *sp, const char *bp)
{
	char *sv_sp = sp;

	if ((sp = rcslib_parse_id(sv_sp, bp, NULL)) != NULL)
		return (sp);
	if ((sp = rcslib_parse_num(sv_sp, bp, NULL)) != NULL)
		return (sp);
	if ((sp = rcslib_parse_string(sv_sp, bp, NULL)) != NULL)
		return (sp);
	if (*sv_sp == ':')
		return (sv_sp + 1);

	return (NULL);
}

char *
rcslib_parse_num(char *sp, const char *bp, struct rcsnum *num)
{
	char *sv_sp = sp;
	size_t i;

	while (isdigit((int)(*sp)) || (*sp == '.')) {
		if (++sp > bp)
			return (NULL);
	}
	if (sp == sv_sp)
		return (NULL);

	if (num != NULL) {
		num->n_str = sv_sp;
		num->n_len = sp - sv_sp;
		num->n_level = 0;
		num->n_num[num->n_level] = 0;
		sp = sv_sp;
		for (i = 0 ; i < num->n_len ; i++) {
			if (*sp == '.') {
				if (num->n_level == RCSNUM_MAXLEVEL)
					return (NULL);
				num->n_num[++num->n_level] = 0;
			} else {
				num->n_num[num->n_level] *= 10;
				num->n_num[num->n_level] += *sp - '0';
				if (num->n_num[num->n_level] > RCSNUM_MAX)
					return (NULL);
			}
			sp++;
		}
		num->n_level++;
	}

	return (sp);
}

char *
rcslib_parse_id(char *sp, const char *bp, struct rcsid *id)
{
	char *sv_sp = sp, *p;

	if ((sp = rcslib_parse_num(sp, bp, NULL)) == NULL)
		sp = sv_sp;

	if (!IS_RCS_IDCHAR(*sp))
		return (NULL);

	for (;;) {
		p = sp;

		while (IS_RCS_IDCHAR(*sp)) {
			if (++sp > bp)
				return (NULL);
		}
		if (sp != p)
			continue;

		if ((sp = rcslib_parse_num(sp, bp, NULL)) == NULL)
			sp = p;
		if (sp == p)
			break;
	}

	if (id != NULL) {
		id->i_id = sv_sp;
		id->i_len = sp - sv_sp;
	}

	return (sp);
}

char *
rcslib_parse_sym(char *sp, const char *bp, struct rcssym *sym)
{
	char *sv_sp = sp, *p;

	while (isdigit((int)(*sp))) {
		if (++sp > bp)
			return (NULL);
	}

	if (!IS_RCS_IDCHAR(*sp))
		return (NULL);

	for (;;) {
		p = sp;

		while (IS_RCS_IDCHAR(*sp)) {
			if (++sp > bp)
				return (NULL);
		}

		while (isdigit((int)(*sp))) {
			if (++sp > bp)
				return (NULL);
		}

		if (sp == p)
			break;
	}

	if (sym != NULL) {
		sym->s_sym = sv_sp;
		sym->s_len = sp - sv_sp;
	}

	return (sp);
}

char *
rcslib_parse_string(char *sp, const char *bp, struct rcsstr *str)
{
	char *sv_sp;

	if (*sp++ != '@')
		return (NULL);

	sv_sp = sp;

	while (sp < bp) {
		if (*sp != '@') {
			sp++;
			continue;
		}
		if (*(sp + 1) != '@')
			break;
		sp += 2;
	}
	if (sp == bp)
		return (NULL);

	if (str != NULL) {
		str->s_str = sv_sp;
		str->s_len = sp++ - sv_sp;
	}

	return (sp);
}

struct rcslib_revision *
rcslib_lookup_revision(struct rcslib_file *rcs, const struct rcsnum *num)
{
	struct rcslib_delta *delta = &rcs->delta;
	struct rcslib_revision *rev;
	size_t l = 0, r = delta->rd_count, i;
	int rv;

	if ((delta->rd_count == 0) || (num->n_len == 0))
		return (NULL);

	if (delta->rd_count < 100) {
		for (i = 0 ; i < delta->rd_count ; i++) {
			rev = &delta->rd_rev[i];
			if ((num->n_len == rev->num.n_len) &&
			    (memcmp(num->n_str, rev->num.n_str,
				    num->n_len) == 0)) {
				return (rev);
			}
		}
	} else {
		while (l <= r) {
			i = (l + r) / 2;
			rev = &delta->rd_rev[i];
			if ((rv = rcslib_cmp_num(num, &rev->num)) == 0)
				return (rev);
			if (rv < 0) {
				if (l == i)
					break;
				r = i - 1;
			} else { /* rv > 0 */
				if (r == i)
					break;
				l = i + 1;
			}
		}
	}

	return (NULL);
}

struct rcslib_revision *
rcslib_lookup_symbol(struct rcslib_file *rcs, void *symstr, size_t symlen)
{
	struct rcslib_revision *rev;
	struct rcslib_symbol *symbol;
	struct rcsnum *num = NULL, *br_num = NULL, t_num;
	size_t sv_level, n, i;

	for (i = 0 ; i < rcs->symbols.rs_count ; i++) {
		symbol = &rcs->symbols.rs_symbols[i];
		if ((symlen == symbol->sym.s_len) &&
		    (memcmp(symstr, symbol->sym.s_sym, symlen) == 0)) {
			num = &symbol->num;
			break;
		}
	}
	if (num == NULL) {
		if (((symlen == 4) && (memcmp(symstr, "HEAD", 4) == 0)) ||
		    ((symlen == 1) && (memcmp(symstr, ".", 1) == 0))) {
			if (rcs->head.n_len != 0) {
				rev = rcslib_lookup_revision(rcs, &rcs->head);
			} else {
				if (rcs->delta.rd_count == 0)
					return (NULL);
				rev = &rcs->delta.rd_rev[0];
			}
			if (rev == NULL)
				return (NULL);
			if (rcs->branch.n_len == 0)
				return (rev);
			num = &rcs->branch;
		} else  {
			if (!rcslib_str2num(symstr, symlen, &t_num))
				return (NULL);
			return (rcslib_lookup_revision(rcs, &t_num));
		}
	}

	if ((num->n_level % 2) != 0) {
		for (n = num->n_len - 1 ; n > 0 ; n--) {
			if (num->n_str[n] == '.')
				break;
		}
		if (n == 0)
			return (NULL);
		sv_level = num->n_level;
	} else {
		for (sv_level = 0 ; sv_level < num->n_level ; sv_level += 2) {
			if (num->n_num[sv_level] == 0)
				break;
		}
		if (sv_level == num->n_level)
			return (rcslib_lookup_revision(rcs, num));

		i = sv_level;
		for (n = 0 ; n < num->n_len ; n++) {
			if (num->n_str[n] == '.') {
				if (--i == 0)
					break;
			}
		}
	}

	if (!rcslib_str2num(num->n_str, n, &t_num))
		return (NULL);
	if ((rev = rcslib_lookup_revision(rcs, &t_num)) == NULL)
		return (NULL);

	for (n = 0 ; n < rev->branches.rb_count ; n++) {
		br_num = &rev->branches.rb_num[n];
		for (i = 0 ; i < sv_level ; i++) {
			if (br_num->n_num[i] != num->n_num[i])
				break;
		}
		if (i == sv_level)
			break;
	}
	if (n == rev->branches.rb_count)
		return (rev);

	if ((rev = rcslib_lookup_revision(rcs, br_num)) == NULL)
		return (NULL);
	while (rev->rv_next != NULL)
		rev = rev->rv_next;

	return (rev);
}

bool
rcslib_write_delta(int fd, const struct rcslib_revision *rev)
{
	const struct rcslib_branches *branches = &rev->branches;
	struct iovec iov[8];
	size_t len, i;
	ssize_t wn;

	iov[0].iov_base = rev->num.n_str;
	iov[0].iov_len = rev->num.n_len;
	iov[1].iov_base = "\ndate\t";
	iov[1].iov_len = 6;
	iov[2].iov_base = rev->date.rd_num.n_str;
	iov[2].iov_len = rev->date.rd_num.n_len;
	iov[3].iov_base = ";\tauthor ";
	iov[3].iov_len = 9;
	iov[4].iov_base = rev->author.i_id;
	iov[4].iov_len = rev->author.i_len;
	iov[5].iov_base = ";\tstate ";
	if (rev->state.i_len > 0)
		iov[5].iov_len = 8;
	else
		iov[5].iov_len = 7;
	iov[6].iov_base = rev->state.i_id;
	iov[6].iov_len = rev->state.i_len;
	iov[7].iov_base = ";\nbranches";
	iov[7].iov_len = 10;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
	      iov[3].iov_len + iov[4].iov_len + iov[5].iov_len +
	      iov[6].iov_len + iov[7].iov_len;

	if ((wn = writev(fd, iov, 8)) == -1)
		return (false);
	if ((size_t)wn != len)
		return (false);

	iov[0].iov_base = "\n\t";
	iov[0].iov_len = 2;
	for (i = 0 ; i < branches->rb_count ; i++) {
		iov[1].iov_base = branches->rb_num[i].n_str;
		iov[1].iov_len = branches->rb_num[i].n_len;
		len = iov[0].iov_len + iov[1].iov_len;
		if ((wn = writev(fd, iov, 2)) == -1)
			return (false);
		if ((size_t)wn != len)
			return (false);
	}

	iov[0].iov_base =";\nnext\t";
	iov[0].iov_len = 7;
	iov[1].iov_base = rev->next.n_str;
	iov[1].iov_len = rev->next.n_len;
	iov[2].iov_base = ";\n\n";
	iov[2].iov_len = 3;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
	if ((wn = writev(fd, iov, 3)) == -1)
		return (false);
	if ((size_t)wn != len)
		return (false);

	return (true);
}

bool
rcslib_write_deltatext(int fd, const struct rcslib_revision *rev)
{
	struct iovec iov[7];
	size_t len;
	ssize_t wn;

	iov[0].iov_base = "\n\n";
	iov[0].iov_len = 2;
	iov[1].iov_base = rev->num.n_str;
	iov[1].iov_len = rev->num.n_len;
	iov[2].iov_base = "\nlog\n@";
	iov[2].iov_len = 6;
	iov[3].iov_base = rev->log.s_str;
	iov[3].iov_len = rev->log.s_len;
	iov[4].iov_base = "@\ntext\n@";
	iov[4].iov_len = 8;
	iov[5].iov_base = rev->text.s_str;
	iov[5].iov_len = rev->text.s_len;
	iov[6].iov_base = "@\n";
	iov[6].iov_len = 2;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
	      iov[3].iov_len + iov[4].iov_len + iov[5].iov_len +
	      iov[6].iov_len;

	if ((wn = writev(fd, iov, 7)) == -1)
		return (false);
	if ((size_t)wn != len)
		return (false);

	return (true);
}

bool
rcslib_str2num(void *buffer, size_t bufsize, struct rcsnum *num)
{
	char *sp = buffer;
	size_t i;

	num->n_str = buffer;
	num->n_len = bufsize;
	num->n_level = 0;
	num->n_num[num->n_level] = 0;

	for (i = 0 ; i < num->n_len ; i++) {
		if (*sp == '.') {
			if (num->n_level == RCSNUM_MAXLEVEL)
				return (false);
			num->n_num[++num->n_level] = 0;
		} else if (isdigit((int)(*sp))) {
			num->n_num[num->n_level] *= 10;
			num->n_num[num->n_level] += *sp - '0';
			if (num->n_num[num->n_level] > RCSNUM_MAX)
				return (false);
		} else {
			return (false);
		}
		sp++;
	}
	num->n_level++;

	return (true);
}

int
rcslib_cmp_lock(const struct rcslib_lock *l1, const struct rcslib_lock *l2)
{
	int rv;

	if ((rv = rcslib_cmp_id(&l1->id, &l2->id)) != 0)
		return (rv);

	return (rcslib_cmp_num(&l1->num, &l2->num));
}

int
rcslib_cmp_symbol(const struct rcslib_symbol *s1,
		  const struct rcslib_symbol *s2)
{
	int rv;

	if ((rv = rcslib_cmp_sym(&s1->sym, &s2->sym)) != 0)
		return (rv);

	return (rcslib_cmp_num(&s1->num, &s2->num));
}

int
rcslib_cmp_id(const struct rcsid *i1, const struct rcsid *i2)
{
	int rv;

	if (i1->i_len == i2->i_len)
		return (memcmp(i1->i_id, i2->i_id, i1->i_len));

	if (i1->i_len > i2->i_len) {
		if ((rv = memcmp(i1->i_id, i2->i_id, i2->i_len)) == 0)
			rv = 1;
	} else {
		if ((rv = memcmp(i1->i_id, i2->i_id, i1->i_len)) == 0)
			rv = -1;
	}

	return (rv);
}

int
rcslib_cmp_num(const struct rcsnum *n1, const struct rcsnum *n2)
{
	size_t i;

	if (n1->n_level == n2->n_level) {
		if (n1->n_level == 2) { /* main trunk */
			for (i = 0 ; i < n1->n_level ; i++) {
				if (n1->n_num[i] == n2->n_num[i])
					continue;
				if (n1->n_num[i] > n2->n_num[i])
					return (-1);
				else
					return (1);
			}
		} else {
			for (i = 0 ; i < n1->n_level ; i++) {
				if (n1->n_num[i] == n2->n_num[i])
					continue;
				if (n1->n_num[i] > n2->n_num[i])
					return (1);
				else
					return (-1);
			}
		}
	} else {
		if (n1->n_level > n2->n_level)
			return (1);
		else
			return (-1);
	}

	return (0);
}

int
rcslib_cmp_sym(const struct rcssym *s1, const struct rcssym *s2)
{
	int rv;

	if (s1->s_len == s2->s_len)
		return (memcmp(s1->s_sym, s2->s_sym, s1->s_len));

	if (s1->s_len > s2->s_len) {
		if ((rv = memcmp(s1->s_sym, s2->s_sym, s2->s_len)) == 0)
			rv = 1;
	} else {
		if ((rv = memcmp(s1->s_sym, s2->s_sym, s1->s_len)) == 0)
			rv = -1;
	}

	return (rv);
}

bool
rcslib_is_ancestor(const struct rcsnum *n1, const struct rcsnum *n2)
{
	size_t n, i;

	if (n1->n_level > n2->n_level)
		return (false);

	n = n1->n_level - 1;

	for (i = 0 ; i < n ; i++) {
		if (n1->n_num[i] != n2->n_num[i])
			return (false);
	}

	if (n1->n_level == 2) {
		if (n1->n_num[n] < n2->n_num[n])
			return (false);
	} else {
		if (n1->n_num[n] > n2->n_num[n])
			return (false);
	}

	return (true);
}

void
rcslib_sort_revision(struct rcslib_file *rcs)
{
	struct rcslib_revision *rev1, *rev2;
	size_t n = rcs->delta.rd_count - 1, i;

	for (i = 0 ; i < n ; i++) {
		rev1 = &rcs->delta.rd_rev[i];
		rev2 = &rcs->delta.rd_rev[i + 1];
		if (rcslib_cmp_num(&rev1->num, &rev2->num) >= 0) {
			break;
		}
	}
	if (i == n) {
		/* already sorted */
		return;
	}

	qsort(rcs->delta.rd_rev, rcs->delta.rd_count, RCSLIB_REVISION_SIZE,
	      (rcslib_cmp_func)rcslib_cmp_num);
}
