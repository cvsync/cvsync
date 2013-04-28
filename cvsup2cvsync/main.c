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

#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdlib.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "attribute.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "logmsg.h"
#include "scanfile.h"

#include "defs.h"

bool cvsup2cvsync(struct cvsync_file *, struct scanfile_args *);
bool cvsup2cvsync_write_attr(struct scanfile_args *, struct cvsync_attr *);
bool cvsup2cvsync_create_tmpfile(const char *, char *, struct scanfile_args *);

void usage(void);

uint16_t mode_umask = CVSYNC_UMASK_UNSPEC;

int
main(int argc, char *argv[])
{
	const char *ifile = NULL, *ofile = NULL;
	struct cvsync_file *cfp;
	struct scanfile_args sa;
	char resolved_ofile[PATH_MAX + CVSYNC_NAME_MAX + 1], *ep;
	unsigned long v;
	int ch;
	bool log_flag = false;

	while ((ch = getopt(argc, argv, "hi:o:qu:v")) != -1) {
		switch (ch) {
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
			if (mode_umask != CVSYNC_UMASK_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			errno = 0;
			v = strtoul(optarg, &ep, 0);
			if ((ep == NULL) || (*ep != '\0')) {
				v = 0;
				errno = EINVAL;
			}
			if (v & ~CVSYNC_ALLPERMS) {
				v = ULONG_MAX;
				errno = ERANGE;
			}
			if (((v == 0) && (errno == EINVAL)) ||
			    ((v == ULONG_MAX) && (errno == ERANGE))) {
				logmsg_err("%s: %s", optarg, strerror(errno));
				usage();
				/* NOTREACHED */
			}
			mode_umask = (uint16_t)v;
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

	if (mode_umask == CVSYNC_UMASK_UNSPEC)
		mode_umask = CVSYNC_UMASK_RCS;
	mode_umask = ~mode_umask & CVSYNC_ALLPERMS;

	if (!cvsync_init())
		exit(EXIT_FAILURE);

	scanfile_init(&sa);
	if (!cvsup2cvsync_create_tmpfile(ofile, resolved_ofile, &sa))
		exit(EXIT_FAILURE);

	if (cvsync_isinterrupted()) {
		logmsg_intr();
		exit(EXIT_FAILURE);
	}

	if ((cfp = cvsync_fopen(ifile)) == NULL)
		exit(EXIT_FAILURE);
	if (cfp->cf_size == 0) {
		logmsg_err("%s: an input file is empty", ifile);
		cvsync_fclose(cfp);
		exit(EXIT_FAILURE);
	}
	if (!cvsync_mmap(cfp, (off_t)0, cfp->cf_size))
		exit(EXIT_FAILURE);

	if (cvsync_isinterrupted()) {
		logmsg_intr();
		cvsync_fclose(cfp);
		scanfile_remove_tmpfile(&sa);
		exit(EXIT_FAILURE);
	}

	if (!cvsup2cvsync(cfp, &sa)) {
		cvsync_fclose(cfp);
		scanfile_remove_tmpfile(&sa);
		exit(EXIT_FAILURE);
	}

	if (!cvsync_fclose(cfp)) {
		scanfile_remove_tmpfile(&sa);
		exit(EXIT_FAILURE);
	}
	if (!scanfile_rename(&sa)) {
		scanfile_remove_tmpfile(&sa);
		exit(EXIT_FAILURE);
	}

	logmsg("Finished successfully");

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

bool
cvsup2cvsync(struct cvsync_file *ifile, struct scanfile_args *ofile)
{
	struct cvsync_attr attr;
	uint8_t *sp = ifile->cf_addr, *bp = sp + ifile->cf_msize, *ep, *sep;
	uint8_t type;
	int n = 0;

	if ((sp = cvsup_examine(sp, bp)) == NULL)
		return (false);

	while (sp < bp) {
		if (cvsync_isinterrupted()) {
			logmsg_intr();
			return (false);
		}

		if (bp - sp < 3) {
			logmsg_err("premature EOF");
			return (false);
		}

		type = *sp++;
		if (*sp++ != ' ') {
			logmsg_err("no separators");
			return (false);
		}

		if ((ep = memchr(sp, '\n', (size_t)(bp - sp))) == NULL) {
			logmsg_err("premature EOF");
			return (false);
		}

		switch (type) {
		case 'D':
			n++;

			attr.ca_namelen = (size_t)(ep - sp);
			if (attr.ca_namelen >= sizeof(attr.ca_name)) {
				logmsg_err("%s", strerror(ENOBUFS));
				return (false);
			}
			(void)memcpy(attr.ca_name, sp, attr.ca_namelen);

			if (!cvsup_decode_dir(&attr, ep + 1, bp))
				return (false);

			if (!cvsup2cvsync_write_attr(ofile, &attr))
				return (false);

			break;
		case 'U':
			if (--n < 0) {
				logmsg_err("too many 'U's");
				return (false);
			}
			break;
		case 'V':
		case 'v':
			if ((sep = memchr(sp, ' ',
					  (size_t)(ep - sp))) == NULL) {
				logmsg_err("no separators");
				return (false);
			}

			attr.ca_namelen = (size_t)(sep - sp);
			if (attr.ca_namelen >= sizeof(attr.ca_name)) {
				logmsg_err("%s", strerror(ENOBUFS));
				return (false);
			}
			(void)memcpy(attr.ca_name, sp, attr.ca_namelen);

			sp = sep + 1;

			if (!cvsup_decode_file(&attr, sp, ep))
				return (false);

			if ((type == 'v') && (attr.ca_tag == FILETYPE_RCS))
				attr.ca_tag = FILETYPE_RCS_ATTIC;

			if (!cvsup2cvsync_write_attr(ofile, &attr))
				return (false);

			break;
		default:
			logmsg_err("%c: unknown entry tag", type);
			return (false);
		}

		sp = ep + 1;
	}

	if (n > 0) {
		logmsg_err("too many 'D's");
		return (false);
	}

	return (true);
}

bool
cvsup2cvsync_write_attr(struct scanfile_args *sa, struct cvsync_attr *attr)
{
	uint8_t *sp = attr->ca_name, *bp = sp + attr->ca_namelen;

	while (sp < bp) {
		if (*sp != '\\') {
			sp++;
			continue;
		}
		if (bp - sp < 2) {
			logmsg_err("premature EOF");
			return (false);
		}
		switch (sp[1]) {
		case '_':
			*sp++ = ' ';
			break;
		case '\\':
			*sp++ = '\\';
			break;
		default:
			sp++;
			continue;
		}
		(void)memmove(sp, sp + 1, (size_t)(bp - sp - 1));
		bp--;
	}

	sa->sa_attr.a_type = attr->ca_tag;
	sa->sa_attr.a_name = attr->ca_name;
	sa->sa_attr.a_namelen = bp - attr->ca_name;
	sa->sa_attr.a_aux = attr->ca_aux;
	sa->sa_attr.a_auxlen = attr->ca_auxlen;

	if (!scanfile_write_attr(sa, &sa->sa_attr))
		return (false);

	return (true);
}

bool
cvsup2cvsync_create_tmpfile(const char *ofile, char *resolved_ofile,
			    struct scanfile_args *sa)
{
	struct stat st;

	if (realpath(ofile, resolved_ofile) == NULL) {
		logmsg_err("%s: %s", ofile, strerror(errno));
		return (false);
	}
	if (stat(resolved_ofile, &st) != -1) {
		st.st_mode &= CVSYNC_ALLPERMS;
	} else {
		if (errno != ENOENT) {
			logmsg_err("%s: %s", ofile, strerror(errno));
			return (false);
		}
		st.st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	}
	sa->sa_scanfile_name = resolved_ofile;
	sa->sa_changed = true;

	if (!scanfile_create_tmpfile(sa, st.st_mode))
		return (false);

	return (true);
}

void
usage(void)
{
	logmsg_err("Usage: cvsup2cvsync [-hqv] [-u <umask>] "
		   "-i <cvsup-scanfile> -o <cvsync-scanfile>");
	exit(EXIT_FAILURE);
}
