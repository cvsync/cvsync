/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DISTFILE_H
#define	CVSYNC_DISTFILE_H

enum {
	DISTFILE_ALLOW,
	DISTFILE_DENY,
	DISTFILE_NORDIFF
};

struct distent {
	int	de_status;
	char	*de_pattern;	/* NULL terminated */
};

struct distfile_args {
	struct distent	*da_patterns;
	size_t		da_size;
};

struct distfile_args *distfile_open(const char *);
void distfile_close(struct distfile_args *);
int distfile_access(struct distfile_args *, const char *);

#endif /* CVSYNC_DISTFILE_H */
