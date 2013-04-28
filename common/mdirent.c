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
#include "compat_limits.h"

#include "mdirent.h"

pthread_mutex_t mdirent_mtx = PTHREAD_MUTEX_INITIALIZER;

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