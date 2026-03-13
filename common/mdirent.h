/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_MDIRENT_H
#define	CVSYNC_MDIRENT_H

struct mDIR {
	void		*m_entries;
	size_t		m_nentries, m_offset;

	void		*m_parent;
	size_t		m_parent_pathlen;
};

struct mdirent {
	char		md_name[CVSYNC_NAME_MAX + 1];
	size_t		md_namelen;
	struct stat	md_stat;
	bool		md_dead;
};

struct mdirent_rcs {
	char		md_name[CVSYNC_NAME_MAX + 1];
	size_t		md_namelen;
	struct stat	md_stat;
	bool		md_dead;

	/* rcs specific */
	bool		md_attic;
};

struct mdirent_args {
	int		mda_errormode;
	bool		mda_symfollow, mda_remove;
};

void mclosedir(struct mDIR *);
int malphasort(const void *, const void *);

struct mDIR *mopendir_rcs(char *, size_t, size_t, struct mdirent_args *);

#endif /* CVSYNC_MDIRENT_H */
