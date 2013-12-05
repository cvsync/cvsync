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

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <strings.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_strings.h"

#include "collection.h"
#include "cvsync.h"
#include "distfile.h"
#include "logmsg.h"
#include "scanfile.h"

void
collection_destroy(struct collection *cl)
{
	distfile_close(cl->cl_distfile);
	scanfile_close(cl->cl_scanfile);
	free(cl);
}

void
collection_destroy_all(struct collection *cls)
{
	struct collection *cl = cls, *cl_next;

	while (cl != NULL) {
		cl_next = cl->cl_next;
		collection_destroy(cl);
		cl = cl_next;
	}
}

struct collection *
collection_lookup(struct collection *cls, const char *name, const char *relname)
{
	struct collection *cl, *new_cl;

	for (cl = cls ; cl != NULL ; cl = cl->cl_next) {
		if (strcasecmp(cl->cl_name, name) != 0)
			continue;
		if (strcasecmp(cl->cl_release, relname) != 0)
			continue;
		break;
	}
	if (cl == NULL)
		return (NULL);

	if ((new_cl = cvsync_memdup(cl, sizeof(*cl))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	new_cl->cl_next = NULL;

	return (new_cl);
}
