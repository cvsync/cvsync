/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DIRSCAN_H
#define	CVSYNC_DIRSCAN_H

struct collection;
struct mux;

struct dirscan_args {
	struct mux		*dsa_mux;
	struct collection	*dsa_collections, *dsa_collection;
	uint32_t		dsa_proto;
	pthread_t		dsa_thread;
	void			*dsa_status;

	char			dsa_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*dsa_rpath;
	char			dsa_symlink[PATH_MAX + CVSYNC_NAME_MAX + 1];
	size_t			dsa_pathmax, dsa_pathlen;
	size_t			dsa_namemax;

	uint8_t			dsa_cmd[CVSYNC_MAXCMDLEN];
	size_t			dsa_cmdmax;
};

struct dirscan_args *dirscan_init(struct mux *, struct collection *, uint32_t);
void dirscan_destroy(struct dirscan_args *);
void *dirscan(void *);
bool dirscan_start(struct dirscan_args *, const char *, const char *);
bool dirscan_end(struct dirscan_args *);

bool dirscan_rcs(struct dirscan_args *);
bool dirscan_rcs_scanfile(struct dirscan_args *);

#endif /* CVSYNC_DIRSCAN_H */
