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
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"

#include "hash.h"
#include "rcslib.h"

void rcscmp_hash(struct rcslib_file *, const struct hash_args *, uint8_t *);

int
main(int argc, char *argv[])
{
	const struct hash_args *hashops;
	const char *fname;
	struct rcslib_file *rcs;
	uint64_t size64;
	uint8_t hash[2][HASH_MAXLEN];
	void *addr;
	off_t size;
	int fd, type, i;

	if (argc != 3) {
		(void)fprintf(stderr, "Usage: rcscmp <file1> <file2>\n");
		exit(EXIT_FAILURE);
	}
	argc--;
	argv++;

	if ((type = hash_pton("sha1", 4)) == HASH_UNSPEC) {
		(void)fprintf(stderr, "The hash type 'sha1' is not supported. "
			      "Use the 'md5' instead\n");
		type = hash_pton("md5", 3);
	}
	if (type == HASH_UNSPEC) {
		(void)fprintf(stderr, "No strong hash is available\n");
		exit(EXIT_FAILURE);
	}
	if (!hash_set(type, &hashops)) {
		(void)fprintf(stderr, "No strong hash is available\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0 ; i < argc ; i++) {
		fname = argv[i];

		if ((fd = open(fname, O_RDONLY, 0)) == -1) {
			(void)fprintf(stderr, "%s: %s\n", fname,
				      strerror(errno));
			exit(EXIT_FAILURE);
		}

		if ((size = lseek(fd, (off_t)0, SEEK_END)) == (off_t)-1) {
			(void)fprintf(stderr, "%s: %s\n", fname,
				      strerror(errno));
			(void)close(fd);
			exit(EXIT_FAILURE);
		}
		if (size == 0) {
			(void)fprintf(stderr, "%s: empty file\n", fname);
			(void)close(fd);
			exit(EXIT_FAILURE);
		}
		size64 = (uint64_t)size;
		if (size64 > SIZE_MAX) {
			(void)fprintf(stderr, "%s: %" PRIu64 ": %s\n", fname,
				      size64, strerror(ERANGE));
			(void)close(fd);
			exit(EXIT_FAILURE);
		}
		if ((addr = mmap(NULL, (size_t)size, PROT_READ, MAP_PRIVATE,
				 fd, (off_t)0)) == MAP_FAILED) {
			(void)fprintf(stderr, "%s: %s\n", fname,
				      strerror(errno));
			(void)close(fd);
			exit(EXIT_FAILURE);
		}

		if (close(fd) == -1) {
			(void)fprintf(stderr, "%s: %s\n", fname,
				      strerror(errno));
			(void)munmap(addr, (size_t)size);
			exit(EXIT_FAILURE);
		}

		if ((rcs = rcslib_init(addr, size)) == NULL) {
			(void)fprintf(stderr, "%s: rcsfile(5) error\n", fname);
			(void)munmap(addr, (size_t)size);
			exit(EXIT_FAILURE);
		}

		rcscmp_hash(rcs, hashops, hash[i]);

		rcslib_destroy(rcs);

		if (munmap(addr, (size_t)size) == -1) {
			(void)fprintf(stderr, "%s: %s\n", fname,
				      strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (memcmp(hash[0], hash[1], hashops->length) != 0) {
		(void)fprintf(stderr, "%s != %s\n", argv[0], argv[1]);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

void
rcscmp_hash(struct rcslib_file *rcs, const struct hash_args *hashops,
	    uint8_t *hash)
{
	struct rcslib_revision *rev;
	struct rcsid *id;
	struct rcsnum *num;
	struct rcssym *sym;
	void *ctx;
	size_t i, j;

	if (!(*hashops->init)(&ctx)) {
		(void)memset(hash, 0, hashops->length);
		return;
	}

	if (rcs->head.n_len > 0)
		(*hashops->update)(ctx, rcs->head.n_str, rcs->head.n_len);

	if (rcs->branch.n_len > 0)
		(*hashops->update)(ctx, rcs->branch.n_str, rcs->branch.n_len);

	for (i = 0 ; i < rcs->access.ra_count ; i++) {
		id =  &rcs->access.ra_id[i];
		(*hashops->update)(ctx, id->i_id, id->i_len);
	}

	for (i = 0 ; i < rcs->symbols.rs_count ; i++) {
		struct rcslib_symbol *symbol = &rcs->symbols.rs_symbols[i];
		sym = &symbol->sym;
		num = &symbol->num;
		(*hashops->update)(ctx, sym->s_sym, sym->s_len);
		(*hashops->update)(ctx, num->n_str, num->n_len);
	}

	for (i = 0 ; i < rcs->locks.rl_count ; i++) {
		struct rcslib_lock *lock = &rcs->locks.rl_locks[i];
		id = &lock->id;
		num = &lock->num;
		(*hashops->update)(ctx, id->i_id, id->i_len);
		(*hashops->update)(ctx, num->n_str, num->n_len);
	}
	if (rcs->locks.rl_strict) {
		uint8_t strict = 1;
		(*hashops->update)(ctx, &strict, 1);
	}

	if (rcs->comment.s_len > 0) {
		(*hashops->update)(ctx, rcs->comment.s_str,
				   rcs->comment.s_len);
	}

	if (rcs->expand.s_len > 0)
		(*hashops->update)(ctx, rcs->expand.s_str, rcs->expand.s_len);

	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		rev = &rcs->delta.rd_rev[i];
		(*hashops->update)(ctx, rev->num.n_str, rev->num.n_len);
		(*hashops->update)(ctx, rev->author.i_id, rev->author.i_len);
		if (rev->state.i_len > 0) {
			(*hashops->update)(ctx, rev->state.i_id,
					   rev->state.i_len);
		}
		for (j = 0 ; j < rev->branches.rb_count ; j++) {
			(*hashops->update)(ctx, rev->branches.rb_num[j].n_str,
				   rev->branches.rb_num[j].n_len);
		}
		if (rev->next.n_len > 0) {
			(*hashops->update)(ctx, rev->next.n_str,
					   rev->next.n_len);
		}
	}

	if (rcs->desc.s_len > 0)
		(*hashops->update)(ctx, rcs->desc.s_str, rcs->desc.s_len);

	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		rev = &rcs->delta.rd_rev[i];
		if (rev->log.s_len > 0) {
			(*hashops->update)(ctx, rev->log.s_str,
					   rev->log.s_len);
		}
		if (rev->text.s_len > 0) {
			(*hashops->update)(ctx, rev->text.s_str,
					   rev->text.s_len);
		}
	}

	(*hashops->final)(ctx, hash);
}
