/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_FILESCAN_H
#define	CVSYNC_FILESCAN_H

struct collection;
struct cvsync_file;
struct hash_args;
struct mux;
struct refuse_args;

#define	FILESCAN_START		(0x80)
#define	FILESCAN_END		(0x81)

#define	FILESCAN_ADD		(0x00)
#define	FILESCAN_REMOVE		(0x01)
#define	FILESCAN_SETATTR	(0x02)
#define	FILESCAN_UPDATE		(0x03)
#define	FILESCAN_RCS_ATTIC	(0x04)

struct filescan_args {
	struct mux		*fsa_mux;
	struct collection	*fsa_collections;
	struct refuse_args	*fsa_refuse;
	uint32_t		fsa_proto;
	pthread_t		fsa_thread;
	void			*fsa_status;

	char			fsa_name[CVSYNC_NAME_MAX + 1];
	char			fsa_release[CVSYNC_NAME_MAX + 1];

	char			fsa_path[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			*fsa_rpath;
	size_t			fsa_pathmax, fsa_pathlen, fsa_namemax;

	uint8_t			fsa_tag, fsa_cmd[CVSYNC_MAXCMDLEN];
	size_t			fsa_cmdmax;
	struct cvsync_attr	fsa_attr;
	uint16_t		fsa_umask;

	void			*fsa_hash_ctx;
	const struct hash_args	*fsa_hash_ops;
};

struct filescan_args *filescan_init(struct mux *, struct collection *,
				    uint32_t, int);
void filescan_destroy(struct filescan_args *);
void *filescan(void *);
bool filescan_start(struct filescan_args *, const char *, const char *);
bool filescan_end(struct filescan_args *);

bool filescan_rcs(struct filescan_args *);

bool filescan_generic_update(struct filescan_args *, struct cvsync_file *);
bool filescan_rdiff_update(struct filescan_args *, struct cvsync_file *);

#endif /* CVSYNC_FILESCAN_H */
