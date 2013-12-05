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

#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <strings.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "collection.h"
#include "cvsync.h"
#include "logmsg.h"
#include "refuse.h"
#include "scanfile.h"

void
collection_destroy(struct collection *cl)
{
	refuse_close(cl->cl_refuse);
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

bool
collection_resolv_prefix(struct collection *cls)
{
	struct collection *cl1, *cl2;
	size_t len1, len2;
	bool conflict = false;

	for (cl1 = cls ; cl1 != NULL ; cl1 = cl1->cl_next) {
		len1 = cl1->cl_prefixlen + cl1->cl_rprefixlen;
		if (len1 >= sizeof(cl1->cl_prefix)) {
			logmsg_err("collection %s/%s: prefix: %s",
				   cl1->cl_name, cl1->cl_release,
				   strerror(ENAMETOOLONG));
			return (false);
		}
		(void)memcpy(&cl1->cl_prefix[cl1->cl_prefixlen],
			     cl1->cl_rprefix, cl1->cl_rprefixlen);
		cl1->cl_prefix[len1] = '\0';
	}

	for (cl1 = cls ; cl1 != NULL ; cl1 = cl1->cl_next) {
		for (cl2 = cls ; cl2 != NULL ; cl2 = cl2->cl_next) {
			if (cl2 == cl1)
				break;

			len1 = strlen(cl1->cl_prefix);
			len2 = strlen(cl2->cl_prefix);

			if (len1 == len2) {
				if (memcmp(cl1->cl_prefix, cl2->cl_prefix,
					   len1) == 0) {
					conflict = true;
				}
			} else if (len1 > len2) {
				if ((cl1->cl_prefix[len2 - 1] == '/') &&
				    (memcmp(cl1->cl_prefix, cl2->cl_prefix,
					    len2) == 0)) {
					conflict = true;
				}
			} else { /* len1 < len2 */
				if ((cl2->cl_prefix[len1 - 1] == '/') &&
				    (memcmp(cl1->cl_prefix, cl2->cl_prefix,
					    len1) == 0)) {
					conflict = true;
				}
			}

			if (!conflict)
				continue;

			logmsg_err("Detect the prefix conflict\n%s/%s\n"
				   "\tprefix %.*s\n\trprefix \"%.*s\"\n%s/%s\n"
				   "\tprefix %.*s\n\trprefix \"%.*s\"",
				   cl1->cl_name, cl1->cl_release,
				   cl1->cl_prefixlen, cl1->cl_prefix,
				   cl1->cl_rprefixlen, cl1->cl_rprefix,
				   cl2->cl_name, cl2->cl_release,
				   cl2->cl_prefixlen, cl2->cl_prefix,
				   cl2->cl_rprefixlen, cl2->cl_rprefix);
			return (false);
		}
	}

	for (cl1 = cls ; cl1 != NULL ; cl1 = cl1->cl_next)
		 cl1->cl_prefix[cl1->cl_prefixlen] = '\0';

	return (true);
}
