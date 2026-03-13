/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_LIST_H
#define	CVSYNC_LIST_H

struct listent;

struct list {
	struct listent	*l_entries, *l_head, *l_tail, *l_freelist;
	void		(*l_destructor)(void *);
};

struct listent {
	struct listent	*le_next, *le_priv;
	void		*le_elm;
	int		le_state;
};

typedef void (*list_destructor)(void *);

#define	list_set_destructor(l, f) \
		list_set_destructor_internal((l), (list_destructor)(f))

struct list *list_init(void);
void list_destroy(struct list *);
void list_set_destructor_internal(struct list *, list_destructor);
bool list_insert_head(struct list *, void *);
bool list_insert_tail(struct list *, void *);
bool list_remove(struct list *, struct listent *);
void *list_remove_head(struct list *);
void *list_remove_tail(struct list *);
bool list_isempty(struct list *);

#endif /* CVSYNC_LIST_H */
