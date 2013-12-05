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
#include <string.h>

#include "compat_stdbool.h"

#include "list.h"
#include "logmsg.h"

#define	LIST_DYNAMIC		(1)
#define	LIST_STATIC		(0)

#define	LIST_MAXCHAINS		(16)

struct list *
list_init(void)
{
	struct list *lp;
	size_t i;

	if ((lp = malloc(sizeof(*lp))) == NULL) {
		logmsg_err("%s", strerror(errno));
		return (NULL);
	}

	lp->l_entries = malloc(LIST_MAXCHAINS * sizeof(*lp->l_entries));
	if (lp->l_entries == NULL) {
		logmsg_err("%s", strerror(errno));
		free(lp);
		return (NULL);
	}
	lp->l_head = NULL;
	lp->l_tail = NULL;
	lp->l_freelist = NULL;
	lp->l_destructor = NULL;

	for (i = 0 ; i < LIST_MAXCHAINS ; i++) {
		lp->l_entries[i].le_next = lp->l_freelist;
		lp->l_freelist = &lp->l_entries[i];
		lp->l_freelist->le_state = LIST_STATIC;
	}

	return (lp);
}

void
list_destroy(struct list *lp)
{
	struct listent *lep, *next;

	lep = lp->l_head;
	while (lep != NULL) {
		next = lep->le_next;
		if (lp->l_destructor != NULL)
			(*lp->l_destructor)(lep->le_elm);
		if (lep->le_state == LIST_DYNAMIC)
			free(lep);
		lep = next;
	}
	lep = lp->l_freelist;
	while (lep != NULL) {
		next = lep->le_next;
		if (lep->le_state == LIST_DYNAMIC)
			free(lep);
		lep = next;
	}
	free(lp->l_entries);
	free(lp);
}

void
__list_set_destructor(struct list *lp, list_destructor destructor)
{
	lp->l_destructor = destructor;
}

bool
list_insert_head(struct list *lp, void *elm)
{
	struct listent *lep;

	if (lp->l_freelist != NULL) {
		lep = lp->l_freelist;
		lp->l_freelist = lep->le_next;
	} else {
		if ((lep = malloc(sizeof(*lep))) == NULL) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
		lep->le_state = LIST_DYNAMIC;
	}

	lep->le_next = lp->l_head;
	lep->le_priv = NULL;
	lep->le_elm = elm;

	if (lp->l_head != NULL)
		lp->l_head->le_priv = lep;
	else
		lp->l_tail = lep;
	lp->l_head = lep;

	return (true);
}

bool
list_insert_tail(struct list *lp, void *elm)
{
	struct listent *lep;

	if (lp->l_freelist != NULL) {
		lep = lp->l_freelist;
		lp->l_freelist = lep->le_next;
	} else {
		if ((lep = malloc(sizeof(*lep))) == NULL) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
		lep->le_state = LIST_DYNAMIC;
	}

	lep->le_next = NULL;
	lep->le_priv = lp->l_tail;
	lep->le_elm = elm;

	if (lp->l_tail != NULL)
		lp->l_tail->le_next = lep;
	else
		lp->l_head = lep;
	lp->l_tail= lep;

	return (true);
}

bool
list_remove(struct list *lp, struct listent *target)
{
	struct listent *lep, *next, *priv;

	if (lp->l_head == NULL)
		return (false);

	for (lep = lp->l_head ; lep != NULL ; lep = lep->le_next) {
		if (lep == target)
			break;
	}
	if (lep == NULL)
		return (false);

	next = lep->le_next;
	priv = lep->le_priv;

	if (next != NULL)
		next->le_priv = priv;
	if (priv != NULL)
		priv->le_next = next;
	if (next == NULL)
		lp->l_tail = priv;
	if (priv == NULL)
		lp->l_head = next;

	lep->le_next = lp->l_freelist;
	lep->le_priv = NULL;
	lp->l_freelist = lep;

	return (true);
}

void *
list_remove_head(struct list *lp)
{
	struct listent *lep;
	void *elm;

	if (lp->l_head == NULL)
		return (NULL);

	lep = lp->l_head;
	lp->l_head = lep->le_next;
	if (lp->l_head != NULL)
		lp->l_head->le_priv = NULL;
	else
		lp->l_tail = NULL;

	elm = lep->le_elm;
	lep->le_next = lp->l_freelist;
	lep->le_priv = NULL;
	lp->l_freelist = lep;

	return (elm);
}

void *
list_remove_tail(struct list *lp)
{
	struct listent *lep;
	void *elm;

	if (lp->l_tail == NULL)
		return (NULL);

	lep = lp->l_tail;
	lp->l_tail = lep->le_priv;
	if (lp->l_tail != NULL)
		lp->l_tail->le_next = NULL;
	else
		lp->l_head = NULL;

	elm = lep->le_elm;
	lep->le_next = lp->l_freelist;
	lep->le_priv = NULL;
	lp->l_freelist = lep;

	return (elm);
}

bool
list_isempty(struct list *lp)
{
	if (lp->l_head != NULL)
		return (false);
	return (true);
}
