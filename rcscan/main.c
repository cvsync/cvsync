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
#include <sys/uio.h>

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

#include "rcslib.h"

bool rcscan_dump(struct rcslib_file *);
void usage(void);

int
main(int argc, char *argv[])
{
	const char *fname;
	struct rcslib_file *rcs;
	void *addr;
	off_t size;
	uint64_t size64;
	int fd, ch;
	bool quiet = false;
	bool verbose = false;

	while ((ch = getopt(argc, argv, "qv")) != -1) {
		switch (ch) {
		case 'q':
			if (quiet) {
				usage();
				/* NOTREACHED */
			}
			quiet = true;
			break;
		case 'v':
			if (verbose) {
				usage();
				/* NOTREACHED */
			}
			verbose = true;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		/* NOTREACHED */
	}

	while (argc > 0) {
		fname = argv[--argc];

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

		if ((rcs = rcslib_init(addr, size)) != NULL) {
			if (verbose) {
				if (!rcscan_dump(rcs))
					(void)printf("ERROR: %s\n", fname);
			} else {
				if (!quiet)
					(void)printf("NO ERROR: %s\n", fname);
			}
			rcslib_destroy(rcs);
		} else {
			(void)printf("ERROR: %s\n", fname);
		}

		if (munmap(addr, (size_t)size) == -1) {
			(void)fprintf(stderr, "%s: %s\n", fname,
				      strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

bool
rcscan_dump(struct rcslib_file *rcs)
{
	struct rcsid *id;
	struct rcsnum *num;
	struct rcssym *sym;
	struct iovec iov[10];
	size_t len, i;
	ssize_t wn;
	int fd = STDOUT_FILENO;

	iov[0].iov_base = "head\t";
	if (rcs->head.n_len > 0)
		iov[0].iov_len = 5;
	else
		iov[0].iov_len = 4;
	iov[1].iov_base = rcs->head.n_str;
	iov[1].iov_len = rcs->head.n_len;
	iov[2].iov_base = ";\n";
	iov[2].iov_len = 2;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
	i = 3;
	if (rcs->branch.n_len > 0) {
		iov[i].iov_base = "branch\t";
		iov[i].iov_len = 7;
		len += iov[i++].iov_len;
		iov[i].iov_base = rcs->branch.n_str;
		iov[i].iov_len = rcs->branch.n_len;
		len += iov[i++].iov_len;
		iov[i].iov_base = ";\n";
		iov[i].iov_len = 2;
		len += iov[i++].iov_len;
	}
	iov[i].iov_base = "access";
	iov[i].iov_len = 6;
	len += iov[i++].iov_len;
	if ((wn = writev(fd, iov, (int)i)) == -1) {
		(void)fprintf(stderr, "%s\n", strerror(errno));
		return (false);
	}
	if ((size_t)wn != len) {
		(void)fprintf(stderr, "writev error\n");
		return (false);
	}

	for (i = 0 ; i < rcs->access.ra_count ; i++) {
		id =  &rcs->access.ra_id[i];
		iov[0].iov_base = "\n\t";
		iov[0].iov_len = 2;
		iov[1].iov_base = id->i_id;
		iov[1].iov_len = id->i_len;
		len = iov[0].iov_len + iov[1].iov_len;
		if ((wn = writev(fd, iov, 2)) == -1) {
			(void)fprintf(stderr, "%s\n", strerror(errno));
			return (false);
		}
		if ((size_t)wn != len) {
			(void)fprintf(stderr, "writev error\n");
			return (false);
		}
	}
	len = 9;
	if ((wn = write(fd, ";\nsymbols", len)) == -1) {
		(void)fprintf(stderr, "%s\n", strerror(errno));
		return (false);
	}
	if ((size_t)wn != len) {
		(void)fprintf(stderr, "write error\n");
		return (false);
	}

	for (i = 0 ; i < rcs->symbols.rs_count ; i++) {
		struct rcslib_symbol *symbol = &rcs->symbols.rs_symbols[i];
		sym = &symbol->sym;
		num = &symbol->num;
		iov[0].iov_base = "\n\t";
		iov[0].iov_len = 2;
		iov[1].iov_base = sym->s_sym;
		iov[1].iov_len = sym->s_len;
		iov[2].iov_base = ":";
		iov[2].iov_len = 1;
		iov[3].iov_base = num->n_str;
		iov[3].iov_len = num->n_len;
		len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
		      iov[3].iov_len;
		if ((wn = writev(fd, iov, 4)) == -1) {
			(void)fprintf(stderr, "%s\n", strerror(errno));
			return (false);
		}
		if ((size_t)wn != len) {
			(void)fprintf(stderr, "writev error\n");
			return (false);
		}
	}
	len = 7;
	if ((wn = write(fd, ";\nlocks", len)) == -1) {
		(void)fprintf(stderr, "%s\n", strerror(errno));
		return (false);
	}
	if ((size_t)wn != len) {
		(void)fprintf(stderr, "write error\n");
		return (false);
	}

	for (i = 0 ; i < rcs->locks.rl_count ; i++) {
		struct rcslib_lock *lock = &rcs->locks.rl_locks[i];
		id = &lock->id;
		num = &lock->num;
		iov[0].iov_base = "\n\t";
		iov[0].iov_len = 2;
		iov[1].iov_base = id->i_id;
		iov[1].iov_len = id->i_len;
		iov[2].iov_base = ":";
		iov[2].iov_len = 1;
		iov[3].iov_base = num->n_str;
		iov[3].iov_len = num->n_len;
		len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
		      iov[3].iov_len;
		if ((wn = writev(fd, iov, 4)) == -1) {
			(void)fprintf(stderr, "%s\n", strerror(errno));
			return (false);
		}
		if ((size_t)wn != len) {
			(void)fprintf(stderr, "writev error\n");
			return (false);
		}
	}

	iov[0].iov_base = ";";
	iov[0].iov_len = 1;
	iov[1].iov_base = " strict;";
	if (rcs->locks.rl_strict)
		iov[1].iov_len = 8;
	else
		iov[1].iov_len = 0;
	iov[2].iov_base = "\ncomment";
	iov[2].iov_len = 8;
	iov[3].iov_base = "\t@";
	if (rcs->comment.s_len > 0)
		iov[3].iov_len = 2;
	else
		iov[3].iov_len = 0;
	iov[4].iov_base = rcs->comment.s_str;
	iov[4].iov_len = rcs->comment.s_len;
	iov[5].iov_base = "@";
	if (rcs->comment.s_len > 0)
		iov[5].iov_len = 1;
	else
		iov[5].iov_len = 0;
	iov[6].iov_base = ";\nexpand\t@";
	if (rcs->expand.s_len > 0)
		iov[6].iov_len = 10;
	else
		iov[6].iov_len = 2;
	iov[7].iov_base = rcs->expand.s_str;
	iov[7].iov_len = rcs->expand.s_len;
	iov[8].iov_base = "@;\n";
	if (rcs->expand.s_len > 0)
		iov[8].iov_len = 3;
	else
		iov[8].iov_len = 0;
	iov[9].iov_base = "\n\n\n";
	iov[9].iov_len = 3;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
	      iov[3].iov_len + iov[4].iov_len + iov[5].iov_len +
	      iov[6].iov_len + iov[7].iov_len + iov[8].iov_len +
	      iov[9].iov_len;

	if ((wn = writev(fd, iov, 10)) == -1) {
		(void)fprintf(stderr, "%s\n", strerror(errno));
		return (false);
	}
	if ((size_t)wn != len) {
		(void)fprintf(stderr, "writev error\n");
		return (false);
	}

	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		if (!rcslib_write_delta(fd, &rcs->delta.rd_rev[i]))
			return (false);
	}

	iov[0].iov_base = "\ndesc\n@",
	iov[0].iov_len = 7;
	iov[1].iov_base = rcs->desc.s_str;
	iov[1].iov_len = rcs->desc.s_len;
	iov[2].iov_base = "@\n";
	iov[2].iov_len = 2;
	len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
	if ((wn = writev(fd, iov, 3)) == -1) {
		(void)fprintf(stderr, "%s\n", strerror(errno));
		return (false);
	}
	if ((size_t)wn != len) {
		(void)fprintf(stderr, "writev error\n");
		return (false);
	}

	for (i = 0 ; i < rcs->delta.rd_count ; i++) {
		if (!rcslib_write_deltatext(fd, &rcs->delta.rd_rev[i]))
			return (false);
	}

	return (true);
}

void
usage(void)
{
	(void)fprintf(stderr, "Usage: rcscan [-qv] <file> ...\n");
	exit(EXIT_FAILURE);
}
