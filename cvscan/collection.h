/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COLLECTION_H
#define	CVSYNC_COLLECTION_H

struct collection {
	struct collection	*cl_next;
	char			cl_name[CVSYNC_NAME_MAX + 1];
	char			cl_release[CVSYNC_NAME_MAX + 1];
	char			cl_prefix[PATH_MAX], cl_rprefix[PATH_MAX];
	size_t			cl_prefixlen, cl_rprefixlen;
	char			cl_comment[256];
	int			cl_errormode;
	bool			cl_symfollow;
	uint16_t		cl_umask;

	char			cl_dist_name[PATH_MAX + CVSYNC_NAME_MAX + 1];

	char			cl_scan_name[PATH_MAX + CVSYNC_NAME_MAX + 1];

	struct collection	*cl_super;
	char			cl_super_name[CVSYNC_NAME_MAX + 1];
};

void collection_init(struct collection *);
void collection_destroy(struct collection *);
void collection_destroy_all(struct collection *);
struct collection *collection_lookup(struct collection *, const char *,
				     const char *);
bool collection_set_default(struct collection *, const char *);

#endif /* CVSYNC_COLLECTION_H */
