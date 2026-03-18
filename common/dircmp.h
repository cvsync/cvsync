/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DIRCMP_H
#define	CVSYNC_DIRCMP_H

struct collection;
struct cvsync_attr;
struct mdirent;
struct mux;
struct scanfile_attr;

#define	DIRCMP_START		(0x80)
#define	DIRCMP_END		(0x81)

#define	DIRCMP_DOWN		(0x00)
#define	DIRCMP_UP		(0x01)
#define	DIRCMP_FILE		(0x02)
#define	DIRCMP_RCS		(0x03)
#define	DIRCMP_RCS_ATTIC	(0x04)
#define	DIRCMP_SYMLINK		(0x05)

struct dircmp_args {
	struct mux		*dca_mux;
	const char		*dca_hostinfo;
	struct collection	*dca_collections, *dca_collection;
	uint32_t		dca_proto;
	pthread_t		dca_thread;
	void			*dca_status;

	char			dca_name[CVSYNC_NAME_MAX + 1];
	char			dca_release[CVSYNC_NAME_MAX + 1];

	char			dca_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*dca_rpath;
	char			dca_symlink[PATH_MAX + CVSYNC_NAME_MAX + 1];
	size_t			dca_pathmax, dca_pathlen, dca_rpathlen;
	size_t			dca_namemax;

	uint8_t			dca_tag, dca_cmd[CVSYNC_MAXCMDLEN];
	size_t			dca_cmdmax;
	struct cvsync_attr	dca_attr;
};

struct dircmp_args *dircmp_init(struct mux *, const char *, struct collection *, uint32_t);
void dircmp_destroy(struct dircmp_args *);
void *dircmp(void *);
bool dircmp_start(struct dircmp_args *, const char *, const char *);
bool dircmp_end(struct dircmp_args *);
bool dircmp_access(struct dircmp_args *, void *);
bool dircmp_access_scanfile(struct dircmp_args *, struct scanfile_attr *);

bool dircmp_rcs(struct dircmp_args *);
bool dircmp_rcs_scanfile(struct dircmp_args *);

#endif /* CVSYNC_DIRCMP_H */
