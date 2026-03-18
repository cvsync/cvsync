/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_SCANFILE_H
#define	CVSYNC_SCANFILE_H

struct cvsync_file;

struct scanfile_attr {
	size_t		a_size;
	uint8_t		a_type;
	void		*a_name;
	size_t		a_namelen;
	void		*a_aux;
	size_t		a_auxlen;
};

struct scanfile_args {
	struct cvsync_file	*sa_scanfile;
	const char		*sa_scanfile_name;
	uint8_t			*sa_start, *sa_end;

	int			sa_tmp;
	char			sa_tmp_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	mode_t			sa_tmp_mode;

	struct list		*sa_dirlist;
	struct scanfile_attr	sa_attr;

	bool			sa_changed;
};

struct scanfile_create_args {
	const char		*sca_name, *sca_prefix, *sca_release;
	const char		*sca_rprefix;
	size_t			sca_rprefixlen;
	mode_t			sca_mode;
	struct mdirent_args	*sca_mdirent_args;
	mode_t			sca_umask;
};

void scanfile_init(struct scanfile_args *);
struct scanfile_args *scanfile_open(const char *);
void scanfile_close(struct scanfile_args *);
bool scanfile_read_attr(uint8_t *, const uint8_t *, struct scanfile_attr *);
bool scanfile_write_attr(struct scanfile_args *, struct scanfile_attr *);

bool scanfile_create_tmpfile(struct scanfile_args *, mode_t);
void scanfile_remove_tmpfile(struct scanfile_args *);
bool scanfile_rename(struct scanfile_args *);

bool scanfile_add(struct scanfile_args *, uint8_t, void *, size_t, void *, size_t);
bool scanfile_remove(struct scanfile_args *, uint8_t, void *, size_t);
bool scanfile_replace(struct scanfile_args *, uint8_t, void *, size_t, void *, size_t);
bool scanfile_update(struct scanfile_args *, uint8_t, void *, size_t, void *, size_t);

bool scanfile_create(struct scanfile_create_args *);
bool scanfile_rcs(struct scanfile_create_args *);

#endif /* CVSYNC_SCANFILE_H */
