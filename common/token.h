/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_TOKEN_H
#define	CVSYNC_TOKEN_H

#define	MAX_TOKEN_LENGTH	(2048)

struct token {
	char	token[MAX_TOKEN_LENGTH];
	size_t	length;
};

struct token_keyword {
	const char	*name;
	size_t		namelen;
	int		type;
};

const struct token_keyword *token_get_keyword(FILE *, const struct token_keyword *);
bool token_get_number(FILE *, unsigned long *);
bool token_get_string(FILE *, struct token *);
bool token_skip_whitespace(FILE *);

extern size_t lineno;

#endif /* CVSYNC_TOKEN_H */
