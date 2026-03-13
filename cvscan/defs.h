/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DEFS_H
#define	CVSYNC_DEFS_H

struct config_include;

struct config {
	char			cf_addr[CVSYNC_MAXHOST];
	char			cf_serv[CVSYNC_MAXSERV];
	size_t			cf_maxclients;
	char			cf_base[PATH_MAX], cf_base_prefix[PATH_MAX];
	char			cf_access_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			cf_halt_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	char			cf_pid_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	int			cf_hash;
	struct collection	*cf_collections;
	struct config_include	*cf_includes;
	int			cf_refcnt;
};

struct config_include {
	struct config_include	*ci_next;
	char			ci_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	time_t			ci_mtime;
};

struct config *config_load(const char *);
void config_destroy(struct config *);
void config_acquire(struct config *);
void config_revoke(struct config *);
bool config_ischanged(struct config *);

#endif /* CVSYNC_DEFS_H */
