/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/stat.h>

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

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "logmsg.h"

void
collection_init(struct collection *cl)
{
	(void)memset(cl, 0, sizeof(*cl));
	cl->cl_symfollow = true;
	cl->cl_umask = CVSYNC_UMASK_UNSPEC;
}

void
collection_destroy(struct collection *cl)
{
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
collection_lookup(struct collection *cls, const char *name,
		  const char *relname)
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

	if ((new_cl = malloc(sizeof(*new_cl))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}
	(void)memcpy(new_cl, cl, sizeof(*new_cl));

	new_cl->cl_next = NULL;

	return (new_cl);
}

bool
collection_set_default(struct collection *cl, const char *prefix)
{
	if (strlen(cl->cl_name) == 0) {
		snprintf(cl->cl_name, sizeof(cl->cl_name), "%s",
			 "<unspecified>");
	}
	if (strlen(cl->cl_release) == 0)
		snprintf(cl->cl_release, sizeof(cl->cl_release), "%s", "rcs");
	if (prefix != NULL) {
		cl->cl_prefixlen = strlen(prefix);
		if (cl->cl_prefixlen >= sizeof(cl->cl_prefix)) {
			logmsg_err("collection %s/%s: prefix: %s", cl->cl_name,
				   cl->cl_release, strerror(ENAMETOOLONG));
			return (false);
		}
		(void)memcpy(cl->cl_prefix, prefix, cl->cl_prefixlen);
		cl->cl_prefix[cl->cl_prefixlen] = '\0';
	}

	switch (cvsync_release_pton(cl->cl_release)) {
	case CVSYNC_RELEASE_RCS:
		if (cl->cl_umask == CVSYNC_UMASK_UNSPEC)
			cl->cl_umask = CVSYNC_UMASK_RCS;
		break;
	default:
		logmsg_err("%s: unknown release type", cl->cl_release);
		return (false);
	}

	return (true);
}
