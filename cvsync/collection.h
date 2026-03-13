/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COLLECTION_H
#define	CVSYNC_COLLECTION_H

struct refuse_args;
struct scanfile_args;

struct collection {
	struct collection	*cl_next;
	char			cl_name[CVSYNC_NAME_MAX + 1];
	char			cl_release[CVSYNC_NAME_MAX + 1];
	char			cl_prefix[PATH_MAX], cl_rprefix[PATH_MAX];
	size_t			cl_prefixlen, cl_rprefixlen;
	int			cl_errormode;
	uint16_t		cl_umask;

	struct refuse_args	*cl_refuse;
	char			cl_refuse_name[PATH_MAX + CVSYNC_NAME_MAX + 1];

	struct scanfile_args	*cl_scanfile;
	char			cl_scan_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	mode_t			cl_scan_mode;

	int			cl_flags;
};

#define	CLFLAGS_DISABLE		(0x00000001)

void collection_destroy(struct collection *);
void collection_destroy_all(struct collection *);
bool collection_resolv_prefix(struct collection *);

#endif /* CVSYNC_COLLECTION_H */
