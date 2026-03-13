/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_UPDATER_H
#define	CVSYNC_UPDATER_H

struct collection;
struct cvsync_attr;
struct hash_args;
struct mux;
struct scanfile_args;

#define	UPDATER_START		(0x80)
#define	UPDATER_END		(0x81)

#define	UPDATER_ADD		(0x00)
#define	UPDATER_REMOVE		(0x01)
#define	UPDATER_SETATTR		(0x02)
#define	UPDATER_UPDATE		(0x03)
#define	UPDATER_RCS_ATTIC	(0x04)

/* UPDATE */
#define	UPDATER_UPDATE_END	(0x81)
#define	UPDATER_UPDATE_ADD	(0x82)
#define	UPDATER_UPDATE_REMOVE	(0x83)
#define	UPDATER_UPDATE_UPDATE	(0x84)

#define	UPDATER_UPDATE_GENERIC	(0x00)
#define	UPDATER_UPDATE_RCS	(0x01)
#define	UPDATER_UPDATE_RDIFF	(0x02)

#define	UPDATER_UPDATE_RCS_HEAD		(0x00)
#define	UPDATER_UPDATE_RCS_BRANCH	(0x01)
#define	UPDATER_UPDATE_RCS_ACCESS	(0x02)
#define	UPDATER_UPDATE_RCS_SYMBOLS	(0x03)
#define	UPDATER_UPDATE_RCS_LOCKS	(0x04)
#define	UPDATER_UPDATE_RCS_LOCKS_STRICT	(0x05)
#define	UPDATER_UPDATE_RCS_COMMENT	(0x06)
#define	UPDATER_UPDATE_RCS_EXPAND	(0x07)
#define	UPDATER_UPDATE_RCS_DELTA	(0x08)
#define	UPDATER_UPDATE_RCS_DESC		(0x09)
#define	UPDATER_UPDATE_RCS_DELTATEXT	(0x0a)

struct updater_args {
	struct mux		*uda_mux;
	struct collection	*uda_collections;
	struct scanfile_args	*uda_scanfile;
	uint32_t		uda_proto;
	pthread_t		uda_thread;
	void			*uda_status;

	char			uda_name[CVSYNC_NAME_MAX + 1];
	char			uda_release[CVSYNC_NAME_MAX + 1];

	char			uda_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*uda_rpath;
	char			uda_tmpfile[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			uda_symlink[PATH_MAX + CVSYNC_NAME_MAX + 1];
	size_t			uda_pathmax, uda_pathlen;
	size_t			uda_namemax;

	uint8_t			uda_tag, uda_cmd[CVSYNC_MAXCMDLEN];
	size_t			uda_cmdmax;
	struct cvsync_attr	uda_attr;
	uint16_t		uda_umask;

	void			*uda_hash_ctx;
	const struct hash_args	*uda_hash_ops;
	uint8_t			uda_hash[HASH_MAXLEN];

	int			uda_fileno;
	void			*uda_buffer;
	size_t			uda_bufsize;
};

struct updater_args *updater_init(struct mux *, struct collection *, uint32_t,
				  int);
void updater_destroy(struct updater_args *);
void *updater(void *);

bool updater_list(struct updater_args *);

bool updater_rcs(struct updater_args *);
bool updater_rcs_scanfile_attic(struct updater_args *);
bool updater_rcs_scanfile_create(struct updater_args *);
bool updater_rcs_scanfile_mkdir(struct updater_args *);
bool updater_rcs_scanfile_remove(struct updater_args *);
bool updater_rcs_scanfile_setattr(struct updater_args *);
bool updater_rcs_scanfile_update(struct updater_args *);

bool updater_generic_update(struct updater_args *);
bool updater_rdiff_update(struct updater_args *);

#endif /* CVSYNC_UPDATER_H */
