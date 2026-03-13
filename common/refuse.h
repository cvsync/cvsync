/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_REFUSE_H
#define	CVSYNC_REFUSE_H

struct cvsync_attr;

struct refuse_args {
	char		**ra_patterns;
	size_t		ra_size;
};

struct refuse_args *refuse_open(const char *);
void refuse_close(struct refuse_args *);
bool refuse_access(struct refuse_args *, struct cvsync_attr *);

#endif /* CVSYNC_REFUSE_H */
