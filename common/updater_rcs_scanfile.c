/*-
 * Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include "hash.h"
#include "logmsg.h"
#include "scanfile.h"

#include "updater.h"

bool
updater_rcs_scanfile_attic(struct updater_args *uda)
{
	const char *cmdmsg, *dirmsg;
	struct cvsync_attr *cap = &uda->uda_attr;
	struct scanfile_args *sa = uda->uda_scanfile;
	size_t auxmax = sizeof(cap->ca_aux), auxlen;

	if (sa == NULL)
		goto done;

	if ((auxlen = attr_rcs_encode_rcs(cap->ca_aux, auxmax, cap->ca_mtime,
					  cap->ca_mode)) == 0) {
		return (false);
	}

	if (!scanfile_replace(sa, cap->ca_type, cap->ca_name, cap->ca_namelen,
			      cap->ca_aux, auxlen)) {
		return (false);
	}

done:
	switch (cap->ca_tag) {
	case UPDATER_UPDATE_GENERIC:
	case UPDATER_UPDATE_RDIFF:
		cmdmsg = "Update";
		break;
	case UPDATER_UPDATE_RCS:
		cmdmsg = "Edit";
		break;
	default:
		return (false);
	}

	if (cap->ca_type == FILETYPE_RCS)
		dirmsg = "<- Attic";
	else
		dirmsg = "-> Attic";

	logmsg(" %s %.*s %s", cmdmsg, cap->ca_namelen, cap->ca_name, dirmsg);

	return (true);
}

bool
updater_rcs_scanfile_create(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct scanfile_args *sa = uda->uda_scanfile;
	size_t auxmax = sizeof(cap->ca_aux), auxlen;

	if (sa == NULL)
		goto done;

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if ((auxlen = attr_rcs_encode_file(cap->ca_aux, auxmax,
						   cap->ca_mtime,
						   (off_t)cap->ca_size,
						   cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((auxlen = attr_rcs_encode_rcs(cap->ca_aux, auxmax,
						  cap->ca_mtime,
						  cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_SYMLINK:
		auxlen = cap->ca_auxlen;
		break;
	default:
		return (false);
	}

	if (!scanfile_add(sa, cap->ca_type, cap->ca_name, cap->ca_namelen,
			  cap->ca_aux, auxlen)) {
		return (false);
	}

done:
	switch (cap->ca_type) {
	case FILETYPE_RCS_ATTIC:
		logmsg(" Create %.*s -> Attic", cap->ca_namelen, cap->ca_name);
		break;
	case FILETYPE_SYMLINK:
		logmsg(" Symlink %.*s -> %.*s", cap->ca_namelen, cap->ca_name,
		       cap->ca_auxlen, cap->ca_aux);
		break;
	default:
		logmsg(" Create %.*s", cap->ca_namelen, cap->ca_name);
		break;
	}

	return (true);
}

bool
updater_rcs_scanfile_mkdir(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct scanfile_args *sa = uda->uda_scanfile;
	size_t auxmax = sizeof(cap->ca_aux), auxlen;

	if (sa == NULL)
		goto done;

	if (cap->ca_type != FILETYPE_DIR)
		return (false);

	if ((auxlen = attr_rcs_encode_dir(cap->ca_aux, auxmax,
					  cap->ca_mode)) == 0) {
		return (false);
	}

	if (!scanfile_add(sa, cap->ca_type, cap->ca_name, cap->ca_namelen,
			  cap->ca_aux, auxlen)) {
		return (false);
	}

done:
	logmsg(" Mkdir %.*s", cap->ca_namelen, cap->ca_name);

	return (true);
}

bool
updater_rcs_scanfile_remove(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct scanfile_args *sa = uda->uda_scanfile;

	if (sa == NULL)
		goto done;

	if (!scanfile_remove(sa, cap->ca_type, cap->ca_name, cap->ca_namelen))
		return (false);

done:
	if (cap->ca_type != FILETYPE_RCS_ATTIC)
		logmsg(" Remove %.*s", cap->ca_namelen, cap->ca_name);
	else
		logmsg(" Remove %.*s in Attic", cap->ca_namelen, cap->ca_name);

	return (true);
}

bool
updater_rcs_scanfile_setattr(struct updater_args *uda)
{
	struct cvsync_attr *cap = &uda->uda_attr;
	struct scanfile_args *sa = uda->uda_scanfile;
	size_t auxmax = sizeof(cap->ca_aux), auxlen;

	if (sa == NULL)
		goto done;

	switch (cap->ca_type) {
	case FILETYPE_DIR:
		if ((auxlen = attr_rcs_encode_dir(cap->ca_aux, auxmax,
						  cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_FILE:
		if ((auxlen = attr_rcs_encode_file(cap->ca_aux, auxmax,
						   cap->ca_mtime,
						   (off_t)cap->ca_size,
						   cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((auxlen = attr_rcs_encode_rcs(cap->ca_aux, auxmax,
						  cap->ca_mtime,
						  cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	default:
		return (false);
	}

	if (!scanfile_update(sa, cap->ca_type, cap->ca_name, cap->ca_namelen,
			     cap->ca_aux, auxlen)) {
		return (false);
	}

done:
	if (cap->ca_type != FILETYPE_RCS_ATTIC) {
		logmsg(" SetAttrs %.*s", cap->ca_namelen, cap->ca_name);
	} else {
		logmsg(" SetAttrs %.*s in Attic", cap->ca_namelen,
		       cap->ca_name);
	}

	return (true);
}

bool
updater_rcs_scanfile_update(struct updater_args *uda)
{
	const char *cmdmsg, *posmsg;
	struct cvsync_attr *cap = &uda->uda_attr;
	struct scanfile_args *sa = uda->uda_scanfile;
	size_t auxmax = sizeof(cap->ca_aux), auxlen;

	if (sa == NULL)
		goto done;

	switch (cap->ca_type) {
	case FILETYPE_FILE:
		if ((auxlen = attr_rcs_encode_file(cap->ca_aux, auxmax,
						   cap->ca_mtime,
						   (off_t)cap->ca_size,
						   cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((auxlen = attr_rcs_encode_rcs(cap->ca_aux, auxmax,
						  cap->ca_mtime,
						  cap->ca_mode)) == 0) {
			return (false);
		}
		break;
	case FILETYPE_SYMLINK:
		auxlen = cap->ca_auxlen;
		break;
	default:
		return (false);
	}

	if (!scanfile_update(sa, cap->ca_type, cap->ca_name, cap->ca_namelen,
			     cap->ca_aux, auxlen)) {
		return (false);
	}

done:
	switch (cap->ca_tag) {
	case UPDATER_UPDATE_GENERIC:
	case UPDATER_UPDATE_RDIFF:
		cmdmsg = "Update";
		break;
	case UPDATER_UPDATE_RCS:
		cmdmsg = "Edit";
		break;
	default:
		return (false);
	}

	if (cap->ca_type != FILETYPE_RCS_ATTIC)
		posmsg = "";
	else
		posmsg = " in Attic";

	logmsg(" %s %.*s%s", cmdmsg, cap->ca_namelen, cap->ca_name, posmsg);

	return (true);
}
