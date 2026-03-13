/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_CONFIG_COMMON_H
#define	CVSYNC_CONFIG_COMMON_H

struct config;

struct config_args {
	FILE				*ca_fp;
	time_t				ca_mtime;
	const struct token_keyword	*ca_key;
	struct token			ca_token;
	char				*ca_buffer;
	size_t				ca_bufsize;
};

struct config_args *config_open(const char *);
bool config_close(struct config_args *);

bool config_insert_collection(struct config *, struct collection *);
bool config_parse_base(struct config_args *, struct config *);
bool config_parse_base_prefix(struct config_args *, struct config *);
bool config_parse_errormode(struct config_args *, struct collection *);
bool config_parse_hash(struct config_args *, struct config *);
bool config_parse_release(struct config_args *, struct collection *);
bool config_parse_umask(struct config_args *, struct collection *);
bool config_resolv_prefix(struct config *, struct collection *, bool);
bool config_resolv_scanfile(struct config *, struct collection *);
bool config_set_string(struct config_args *);

#endif /* CVSYNC_CONFIG_COMMON_H */
