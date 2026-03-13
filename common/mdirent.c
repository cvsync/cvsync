/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <limits.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_limits.h"

#include "mdirent.h"

void
mclosedir(struct mDIR *mdirp)
{
	if (mdirp->m_entries != NULL)
		free(mdirp->m_entries);
	free(mdirp);
}

int
malphasort(const void *p1, const void *p2)
{
	const struct mdirent *mdp1 = p1, *mdp2 = p2;
	int rv;

	if (mdp1->md_namelen == mdp2->md_namelen) {
		if (mdp1->md_namelen == 0)
			return (0);
		return (memcmp(mdp1->md_name, mdp2->md_name,
			       mdp1->md_namelen));
	}

	if (mdp1->md_namelen > mdp2->md_namelen) {
		if (mdp2->md_namelen == 0)
			return (-1);
		rv = memcmp(mdp1->md_name, mdp2->md_name, mdp2->md_namelen);
		if (rv == 0)
			rv = 1;
	} else {
		if (mdp1->md_namelen == 0)
			return (1);
		rv = memcmp(mdp1->md_name, mdp2->md_name, mdp1->md_namelen);
		if (rv == 0)
			rv = -1;
	}

	return (rv);
}
