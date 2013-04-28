/*-
 * Copyright (c) 2003-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_sys_stat.h"
#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "attribute.h"
#include "cvsync.h"
#include "filetypes.h"
#include "list.h"
#include "logmsg.h"
#include "scanfile.h"

#include "defs.h"

bool cvsync2cvsup(struct scanfile_args *, const char *);
bool cvsync2cvsup_chdir(struct scanfile_args *, struct scanfile_attr *, struct scanfile_attr *);
bool cvsync2cvsup_isparent(struct scanfile_attr *, struct scanfile_attr *);
bool cvsync2cvsup_insert_attr(struct scanfile_args *, struct scanfile_attr *);
void cvsync2cvsup_remove_attr(struct scanfile_args *, struct scanfile_attr *);

void usage(void);

int
main(int argc, char *argv[])
{
	const char *ifile = NULL, *ofile = NULL;
	struct scanfile_args *sa;
	char *user = NULL, *group = NULL;
	int ch;
	bool log_flag = false;

	while ((ch = getopt(argc, argv, "g:hi:o:qu:v")) != -1) {
		switch (ch) {
		case 'g':
			group = optarg;
			break;
		case 'h':
			usage();
			/* NOTREACHED */
		case 'i':
			if (ifile != NULL) {
				usage();
				/* NOTREACHED */
			}
			ifile = optarg;
			break;
		case 'o':
			if (ofile != NULL) {
				usage();
				/* NOTREACHED */
			}
			ofile = optarg;
			break;
		case 'q':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_quiet = true;
			log_flag = true;
			break;
		case 'u':
			user = optarg;
			break;
		case 'v':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_detail = true;
			log_flag = true;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if ((argc != 0) || (ifile == NULL) || (ofile == NULL)) {
		usage();
		/* NOTREACHED */
	}

	if (!cvsync_init())
		exit(EXIT_FAILURE);

	if (!cvsup_init(user, group))
		exit(EXIT_FAILURE);

	if ((sa = scanfile_open(ifile)) == NULL)
		exit(EXIT_FAILURE);
	if ((sa->sa_dirlist = list_init()) == NULL) {
		logmsg_err("Scanfile Error: list init");
		scanfile_close(sa);
		exit(EXIT_FAILURE);
	}

	if (!cvsync2cvsup(sa, ofile)) {
		scanfile_close(sa);
		exit(EXIT_FAILURE);
	}

	scanfile_close(sa);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

bool
cvsync2cvsup(struct scanfile_args *sa, const char *ofile)
{
	struct scanfile_attr attr, dirattr;
	struct stat st;
	char new_ofile[PATH_MAX + CVSYNC_NAME_MAX + 1];
	int fd, wn;

	if (stat(ofile, &st) != -1) {
		st.st_mode &= CVSYNC_ALLPERMS;
	} else {
		if (errno != ENOENT) {
			logmsg_err("%s: %s", ofile, strerror(errno));
			return (false);
		}
		st.st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	}

	wn = snprintf(new_ofile, sizeof(new_ofile), "%s.XXXXXX", ofile);
	if ((wn <= 0) || ((size_t)wn >= sizeof(new_ofile))) {
		logmsg_err("%s: %s", ofile, strerror(EINVAL));
		return (false);
	}
	if ((fd = mkstemp(new_ofile)) == -1) {
		logmsg_err("%s: %s", ofile, strerror(errno));
		return (false);
	}
	sa->sa_tmp = fd;

	if (!cvsup_write_header(sa->sa_tmp, sa->sa_scanfile->cf_mtime))
		return (false);

	(void)memset(&dirattr, 0, sizeof(dirattr));

	while (sa->sa_start < sa->sa_end) {
		if (cvsync_isinterrupted()) {
			logmsg_intr();
			(void)unlink(new_ofile);
			return (false);
		}

		if (!scanfile_read_attr(sa->sa_start, sa->sa_end, &attr)) {
			(void)unlink(new_ofile);
			return (false);
		}

		if (!cvsync2cvsup_chdir(sa, &dirattr, &attr)) {
			(void)unlink(new_ofile);
			return (false);
		}

		switch (attr.a_type) {
		case FILETYPE_DIR:
			/* Nothing to do. */
			break;
		case FILETYPE_FILE:
			if (!cvsup_write_file(sa->sa_tmp, &attr))
				return (false);
			break;
		case FILETYPE_RCS:
		case FILETYPE_RCS_ATTIC:
			if (!cvsup_write_rcs(sa->sa_tmp, &attr))
				return (false);
			break;
		case FILETYPE_SYMLINK:
			if (!cvsup_write_symlink(sa->sa_tmp, &attr))
				return (false);
			break;
		default:
			logmsg_err("%c: Unknown attr type");
			break;
		}

		sa->sa_start += attr.a_size;
	}

	if (dirattr.a_type == FILETYPE_DIR) {
		if (!cvsup_write_dirup(sa->sa_tmp, &dirattr))
			return (false);
	}

	while (!list_isempty(sa->sa_dirlist)) {
		cvsync2cvsup_remove_attr(sa, &dirattr);
		if (dirattr.a_type != FILETYPE_DIR)
			return (false);
		if (!cvsup_write_dirup(sa->sa_tmp, &dirattr))
			return (false);
	}

	fd = sa->sa_tmp;
	sa->sa_tmp = -1;

	if (fchmod(fd, st.st_mode) == -1) {
		logmsg_err("%s: %s", ofile, strerror(errno));
		(void)unlink(new_ofile);
		return (false);
	}
	if (close(fd) == -1) {
		logmsg_err("%s: %s", ofile, strerror(errno));
		(void)unlink(new_ofile);
		return (false);
	}

	if (rename(new_ofile, ofile) == -1) {
		logmsg_err("%s: %s", ofile, strerror(errno));
		(void)unlink(new_ofile);
		return (false);
	}

	logmsg("Finished successfully");

	return (true);
}

bool
cvsync2cvsup_chdir(struct scanfile_args *sa, struct scanfile_attr *dirattr,
		   struct scanfile_attr *attr)
{
	if (dirattr->a_type != FILETYPE_DIR) {
		if (attr->a_type == FILETYPE_DIR) {
			(void)memcpy(dirattr, attr, sizeof(*dirattr));
			if (!cvsup_write_dirdown(sa->sa_tmp, dirattr))
				return (false);
		}
		return (true);
	}

	if (cvsync2cvsup_isparent(dirattr, attr)) {
		if (attr->a_type == FILETYPE_DIR) {
			if (!cvsync2cvsup_insert_attr(sa, dirattr))
				return (false);
			(void)memcpy(dirattr, attr, sizeof(*dirattr));
			if (!cvsup_write_dirdown(sa->sa_tmp, dirattr))
				return (false);
		}
		return (true);
	}

	if (attr->a_type == FILETYPE_DIR) {
		for (;;) {
			if (!cvsup_write_dirup(sa->sa_tmp, dirattr))
				return (false);
			cvsync2cvsup_remove_attr(sa, dirattr);
			if (dirattr->a_type != FILETYPE_DIR)
				break;

			if (cvsync2cvsup_isparent(dirattr, attr)) {
				if (!cvsync2cvsup_insert_attr(sa, dirattr))
					return (false);
				break;
			}
		}
		(void)memcpy(dirattr, attr, sizeof(*dirattr));
		if (!cvsup_write_dirdown(sa->sa_tmp, dirattr))
			return (false);
		return (true);
	}

	for (;;) {
		if (list_isempty(sa->sa_dirlist)) {
			(void)memset(dirattr, 0, sizeof(*dirattr));
			break;
		}

		if (!cvsup_write_dirup(sa->sa_tmp, dirattr))
			return (false);
		cvsync2cvsup_remove_attr(sa, dirattr);

		if ((dirattr->a_type != FILETYPE_DIR) ||
		    cvsync2cvsup_isparent(dirattr, attr)) {
			break;
		}
	}

	return (true);
}

bool
cvsync2cvsup_isparent(struct scanfile_attr *attr1, struct scanfile_attr *attr2)
{
	uint8_t *name2 = (uint8_t *)attr2->a_name;

	if ((attr1->a_namelen < attr2->a_namelen) &&
	    (name2[attr1->a_namelen] == '/') &&
	    (memcmp(attr1->a_name, name2, attr1->a_namelen) == 0)) {
		return (true);
	}

	return (false);
}

bool
cvsync2cvsup_insert_attr(struct scanfile_args *sa, struct scanfile_attr *attr)
{
	struct scanfile_attr *new_attr;

	if ((new_attr = malloc(sizeof(*new_attr))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (false);
	}
	(void)memcpy(new_attr, attr, sizeof(*new_attr));

	if (!list_insert_tail(sa->sa_dirlist, new_attr)) {
		free(new_attr);
		return (false);
	}

	return (true);
}

void
cvsync2cvsup_remove_attr(struct scanfile_args *sa, struct scanfile_attr *attr)
{
	struct scanfile_attr *new_attr;

	if ((new_attr = list_remove_tail(sa->sa_dirlist)) == NULL) {
		(void)memset(attr, 0, sizeof(*attr));
	} else {
		(void)memcpy(attr, new_attr, sizeof(*attr));
		free(new_attr);
	}
}

void
usage(void)
{
	logmsg_err("Usage: cvsync2cvsup [-hqv] "
		   "-i <cvsync-scanfile> -o <cvsup-scanfile>");
	exit(EXIT_FAILURE);
}
