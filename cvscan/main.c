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
#include <sys/stat.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>

#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "logmsg.h"
#include "mdirent.h"
#include "network.h"
#include "scanfile.h"

#include "defs.h"

bool cvscan(struct collection *);
void usage(void);

int
main(int argc, char *argv[])
{
	const char *cfname = NULL;
	struct collection *cls = NULL, *cl, base_cl;
	struct config *cf;
	size_t len;
	int ch, status = EXIT_SUCCESS;
	bool log_flag = false;

	cl = &base_cl;
	collection_init(cl);

	while ((ch = getopt(argc, argv, "FLc:f:hlqr:v")) != -1) {
		switch (ch) {
		case 'F':
			if (!cl->cl_symfollow) {
				usage();
				/* NOTREACHED */
			}
			cl->cl_symfollow = false;
			break;
		case 'L':
			if (cl->cl_errormode != CVSYNC_ERRORMODE_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			cl->cl_errormode = CVSYNC_ERRORMODE_FIXUP;
			break;
		case 'c':
			if (cfname != NULL) {
				usage();
				/* NOTREACHED */
			}
			cfname = optarg;
			break;
		case 'f':
			if (strlen(cl->cl_scan_name) != 0) {
				usage();
				/* NOTREACHED */
			}
			len = strlen(optarg);
			if (len >= sizeof(cl->cl_scan_name)) {
				usage();
				/* NOTREACHED */
			}
			(void)memcpy(cl->cl_scan_name, optarg, len);
			cl->cl_scan_name[len] = '\0';
			break;
		case 'h':
			usage();
			/* NOTREACHED */
		case 'l':
			if (cl->cl_errormode != CVSYNC_ERRORMODE_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			cl->cl_errormode = CVSYNC_ERRORMODE_IGNORE;
			break;
		case 'q':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_quiet = true;
			log_flag = true;
			break;
		case 'r':
			if (strlen(cl->cl_release) != 0) {
				usage();
				/* NOTREACHED */
			}
			len = strlen(optarg);
			if (len >= sizeof(cl->cl_release)) {
				usage();
				/* NOTREACHED */
			}
			(void)memcpy(cl->cl_release, optarg, len);
			cl->cl_release[len] = '\0';
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

	if (cl->cl_errormode == CVSYNC_ERRORMODE_UNSPEC)
		cl->cl_errormode = CVSYNC_ERRORMODE_ABORT;

	if (!cvsync_init())
		exit(EXIT_FAILURE);

	if (cfname == NULL) {
		if (argc != 1) {
			usage();
			/* NOTREACHED */
		}
		if (!collection_set_default(cl, argv[0]))
			exit(EXIT_FAILURE);
		if (strlen(cl->cl_scan_name) == 0) {
			logmsg_err("Not specified the output file.");
			exit(EXIT_FAILURE);
		}
		if (!cvscan(cl))
			status = EXIT_FAILURE;
	} else {
		if (!collection_set_default(cl, NULL))
			exit(EXIT_FAILURE);

		if ((cf = config_load(cfname)) == NULL)
			exit(EXIT_FAILURE);

		switch (argc) {
		case 0:
			cls = cf->cf_collections;
			break;
		case 1:
			cls = collection_lookup(cf->cf_collections, argv[0],
					       base_cl.cl_release);
			if (cls == NULL) {
				logmsg_err("Not such a collection: %s/%s",
					   argv[0], base_cl.cl_release);
				config_destroy(cf);
				exit(EXIT_FAILURE);
			}
			if (strlen(cls->cl_scan_name) == 0) {
				logmsg_err("Not specified the output file.");
				collection_destroy(cls);
				config_destroy(cf);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			usage();
			/* NOTREACHED */
		}

		for (cl = cls ; cl != NULL ; cl = cl->cl_next) {
			if (!cvscan(cl))
				status = EXIT_FAILURE;
		}

		if (argc != 0)
			collection_destroy(cls);
		config_destroy(cf);
	}

	exit(status);
	/* NOTREACHED */
}

bool
cvscan(struct collection *cl)
{
	struct scanfile_create_args sca;
	struct mdirent_args mda;

	if (strlen(cl->cl_scan_name) == 0)
		return (true);
	if ((cl->cl_super != NULL) &&
	    (strcmp(cl->cl_super->cl_scan_name, cl->cl_scan_name) == 0)) {
		return (true);
	}

	logmsg("Scanfile: name %s, release %s, prefix %s", cl->cl_name,
	       cl->cl_release, cl->cl_prefix);

	mda.mda_errormode = cl->cl_errormode;
	mda.mda_symfollow = cl->cl_symfollow;
	mda.mda_remove = false;

	sca.sca_name = cl->cl_scan_name;
	sca.sca_prefix = cl->cl_prefix;
	sca.sca_release = cl->cl_release;
	sca.sca_rprefix = cl->cl_rprefix;
	sca.sca_rprefixlen = cl->cl_rprefixlen;
	sca.sca_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	sca.sca_mdirent_args = &mda;
	sca.sca_umask = cl->cl_umask;

	if (!scanfile_create(&sca))
		return (false);

	logmsg("Finished successfully");

	return (true);
}

void
usage(void)
{
	logmsg_err("Usage: cvscan [-hqv] [-r <release>] -c <file> [<name>]\n"
		   "       cvscan [-FLhlqv] [-r <release>] -f <file> "
		   "<directory>");
	exit(EXIT_FAILURE);
}
