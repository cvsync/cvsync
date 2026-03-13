/*-
 * This software is released under the BSD License, see LICENSE.
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
