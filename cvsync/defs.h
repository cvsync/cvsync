/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DEFS_H
#define	CVSYNC_DEFS_H

struct config {
	struct config		*cf_next;
	char			cf_host[CVSYNC_MAXHOST];
	char			cf_serv[CVSYNC_MAXSERV];
	char			cf_base[PATH_MAX], cf_base_prefix[PATH_MAX];
	int			cf_family;
	int			cf_compress;
	int			cf_hash;
	uint32_t		cf_proto;
	uint16_t		cf_mss;
	struct collection	*cf_collections;
};

struct config *config_load_file(const char *);
struct config *config_load_uri(const char *);
void config_destroy(struct config *);

struct mux *channel_establish(int, struct config *);
bool collectionlist_exchange(int, struct config *);
bool compress_exchange(int, struct config *);
bool hash_exchange(int, struct config *);
bool protocol_exchange(int, struct config *);

#endif /* CVSYNC_DEFS_H */
