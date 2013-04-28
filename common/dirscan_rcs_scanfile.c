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

#include <stdlib.h>

#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "list.h"
#include "mux.h"
#include "scanfile.h"

#include "dirscan.h"
#include "dircmp.h"

struct dirscan_scanfile_args {
	struct collection	*sa_collection;
	uint8_t			*sa_start, *sa_end;
};

bool dirscan_rcs_scanfile_down(struct dirscan_args *, struct scanfile_attr *);
bool dirscan_rcs_scanfile_up(struct dirscan_args *);
bool dirscan_rcs_scanfile_file(struct dirscan_args *, struct scanfile_attr *);

bool dirscan_rcs_scanfile_read(struct dirscan_scanfile_args *,
			       struct scanfile_attr *);
bool dirscan_isparent(struct scanfile_attr *, struct scanfile_attr *);

bool
dirscan_rcs_scanfile(struct dirscan_args *dsa)
{
	struct dirscan_scanfile_args args;
	struct scanfile_args *sa = dsa->dsa_collection->cl_scanfile;
	struct scanfile_attr *dirattr = NULL, attr;
	struct list *lp;

	args.sa_collection = dsa->dsa_collection;
	args.sa_start = sa->sa_start;
	args.sa_end = sa->sa_end;

	if ((lp = list_init()) == NULL)
		return (false);
	list_set_destructor(lp, free);

	while (args.sa_start < args.sa_end) {
		if (cvsync_isinterrupted()) {
			list_destroy(lp);
			if (dirattr != NULL)
				free(dirattr);
			return (false);
		}

		if (!dirscan_rcs_scanfile_read(&args, &attr)) {
			if (args.sa_start == args.sa_end)
				break;
			list_destroy(lp);
			if (dirattr != NULL)
				free(dirattr);
			return (false);
		}
		if (attr.a_namelen > dsa->dsa_namemax) {
			list_destroy(lp);
			if (dirattr != NULL)
				free(dirattr);
			return (false);
		}

		switch (attr.a_type) {
		case FILETYPE_DIR:
			while (!dirscan_isparent(dirattr, &attr)) {
				if (dirattr == NULL)
					break;
				if (!dirscan_rcs_scanfile_up(dsa)) {
					list_destroy(lp);
					free(dirattr);
					return (false);
				}
				free(dirattr);
				if (list_isempty(lp)) {
					dirattr = NULL;
					break;
				}
				if ((dirattr = list_remove_tail(lp)) == NULL) {
					list_destroy(lp);
					if (dirattr != NULL)
						free(dirattr);
					return (false);
				}
			}
			if ((dirattr != NULL) &&
			    !list_insert_tail(lp, dirattr)) {
				list_destroy(lp);
				if (dirattr != NULL)
					free(dirattr);
				return (false);
			}
			if (attr.a_auxlen != RCS_ATTRLEN_DIR) {
				list_destroy(lp);
				return (false);
			}
			if (!dirscan_rcs_scanfile_down(dsa, &attr)) {
				list_destroy(lp);
				return (false);
			}
			dirattr = cvsync_memdup(&attr, sizeof(attr));
			if (dirattr == NULL) {
				list_destroy(lp);
				return (false);
			}
			break;
		case FILETYPE_FILE:
		case FILETYPE_RCS:
		case FILETYPE_RCS_ATTIC:
		case FILETYPE_SYMLINK:
			while (!dirscan_isparent(dirattr, &attr)) {
				if ((dirattr != NULL) &&
				    !dirscan_rcs_scanfile_up(dsa)) {
					list_destroy(lp);
					free(dirattr);
					return (false);
				}
				if (dirattr != NULL)
					free(dirattr);
				if (list_isempty(lp)) {
					dirattr = NULL;
					break;
				}
				if ((dirattr = list_remove_tail(lp)) == NULL) {
					list_destroy(lp);
					if (dirattr != NULL)
						free(dirattr);
					return (false);
				}
			}
			if (!dirscan_rcs_scanfile_file(dsa, &attr)) {
				list_destroy(lp);
				if (dirattr != NULL)
					free(dirattr);
				return (false);
			}
			break;
		default:
			list_destroy(lp);
			if (dirattr != NULL)
				free(dirattr);
			return (false);
		}

		args.sa_start += attr.a_size;
	}

	if (dirattr != NULL) {
		if (!dirscan_rcs_scanfile_up(dsa)) {
			list_destroy(lp);
			free(dirattr);
			return (false);
		}
		free(dirattr);
	}
	while (!list_isempty(lp)) {
		if ((dirattr = list_remove_tail(lp)) == NULL) {
			list_destroy(lp);
			return (false);
		}
		if (!dirscan_rcs_scanfile_up(dsa)) {
			list_destroy(lp);
			free(dirattr);
			return (false);
		}
		free(dirattr);
	}

	list_destroy(lp);

	return (true);
}

bool
dirscan_rcs_scanfile_down(struct dirscan_args *dsa, struct scanfile_attr *attr)
{
	const uint8_t *name, *sv_name = attr->a_name;
	uint8_t *cmd = dsa->dsa_cmd;
	size_t namelen, len;

	for (name = &sv_name[attr->a_namelen - 1] ; name >= sv_name ; name--) {
		if (*name == '/') {
			name++;
			break;
		}
	}
	if (name < sv_name)
		name = sv_name;
	namelen = attr->a_namelen - (name - sv_name);
	if (namelen > dsa->dsa_namemax)
		return (false);
	if ((len = namelen + attr->a_auxlen + 4) > dsa->dsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = DIRCMP_DOWN;
	cmd[3] = namelen;
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, cmd, 4))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, name, namelen))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, attr->a_aux, attr->a_auxlen))
		return (false);

	return (true);
}

bool
dirscan_rcs_scanfile_up(struct dirscan_args *dsa)
{
	static const uint8_t _cmd[3] = { 0x00, 0x01, DIRCMP_UP };

	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, _cmd, sizeof(_cmd)))
		return (false);

	return (true);
}

bool
dirscan_rcs_scanfile_file(struct dirscan_args *dsa, struct scanfile_attr *attr)
{
	uint8_t *cmd = dsa->dsa_cmd;
	const uint8_t *name, *sv_name = attr->a_name;
	size_t namelen, len;

	for (name = &sv_name[attr->a_namelen - 1] ; name >= sv_name ; name--) {
		if (*name == '/') {
			name++;
			break;
		}
	}
	if (name < sv_name)
		name = sv_name;
	namelen = attr->a_namelen - (name - sv_name);
	if (namelen > dsa->dsa_namemax)
		return (false);
	if ((len = namelen + attr->a_auxlen + 4) > dsa->dsa_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	switch (attr->a_type) {
	case FILETYPE_FILE:
		cmd[2] = DIRCMP_FILE;
		if (attr->a_auxlen != RCS_ATTRLEN_FILE)
			return (false);
		break;
	case FILETYPE_RCS:
		cmd[2] = DIRCMP_RCS;
		if (attr->a_auxlen != RCS_ATTRLEN_RCS)
			return (false);
		break;
	case FILETYPE_RCS_ATTIC:
		cmd[2] = DIRCMP_RCS_ATTIC;
		if (attr->a_auxlen != RCS_ATTRLEN_RCS)
			return (false);
		break;
	case FILETYPE_SYMLINK:
		cmd[2] = DIRCMP_SYMLINK;
		if (attr->a_auxlen > CVSYNC_MAXAUXLEN)
			return (false);
		break;
	default:
		return (false);
	}
	cmd[3] = namelen;
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, cmd, 4))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, name, namelen))
		return (false);
	if (!mux_send(dsa->dsa_mux, MUX_DIRCMP, attr->a_aux, attr->a_auxlen))
		return (false);

	return (true);
}

bool
dirscan_rcs_scanfile_read(struct dirscan_scanfile_args *args,
			  struct scanfile_attr *attr)
{
	struct collection *cl = args->sa_collection;
	struct scanfile_attr rpref_attr;
	char *name;

	rpref_attr.a_name = cl->cl_rprefix;
	rpref_attr.a_namelen = cl->cl_rprefixlen + 1;

	for (;;) {
		if (!scanfile_read_attr(args->sa_start, args->sa_end, attr))
			return (false);

		if (cl->cl_rprefixlen == 0)
			break;

		if (attr->a_namelen <= cl->cl_rprefixlen) {
			if (!dirscan_isparent(attr, &rpref_attr)) {
				args->sa_start += attr->a_size;
				continue;
			}
			break;
		}

		name = attr->a_name;
		if ((name[cl->cl_rprefixlen] == '/') &&
		    (memcmp(name, cl->cl_rprefix, cl->cl_rprefixlen) == 0)) {
			break;
		}

		args->sa_start += attr->a_size;
		if (args->sa_start == args->sa_end)
			return (false);
	}

	return (true);
}

bool
dirscan_isparent(struct scanfile_attr *dirattr, struct scanfile_attr *attr)
{
	uint8_t *name = attr->a_name;

	if (dirattr == NULL)
		return (true);
	if (dirattr->a_type != FILETYPE_DIR)
		return (false);

	if (dirattr->a_namelen >= attr->a_namelen)
		return (false);
	if (name[dirattr->a_namelen] != '/')
		return (false);
	if (memcmp(dirattr->a_name, name, dirattr->a_namelen) != 0)
		return (false);

	return (true);
}
