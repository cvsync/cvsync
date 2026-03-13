/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "compat_stdbool.h"

#include "logmsg.h"
#include "token.h"

size_t lineno = 1;

const struct token_keyword *
token_get_keyword(FILE *fp, const struct token_keyword *keys)
{
	const struct token_keyword *key;
	struct token tk;
	int c;

	if (!token_skip_whitespace(fp)) {
		logmsg_err("premature EOF");
		return (false);
	}

	tk.length = 0;

	if ((c = fgetc(fp)) == EOF) {
		logmsg_err("premature EOF");
		return (NULL);
	}

	while (isalnum(c) || (c == '{') || (c == '}') || (c == '-')) {
		tk.token[tk.length++] = (char)c;
		if (tk.length == sizeof(tk.token)) {
			logmsg_err("too long token");
			return (NULL);
		}
		if ((c = fgetc(fp)) == EOF) {
			logmsg_err("premature EOF");
			return (NULL);
		}
	}
	if (tk.length == 0) {
		logmsg_err("line %u: no keywords", lineno);
		return (NULL);
	}
	tk.token[tk.length] = '\0';

	if (ungetc(c, fp) == EOF)
		return (false);

	for (key = keys ; key->name != NULL ; key++) {
		if ((tk.length == key->namelen) &&
		    (memcmp(tk.token, key->name, tk.length) == 0)) {
			return (key);
		}
	}

	logmsg_err("%s: invalid keyword", tk.token);

	return (NULL);
}

bool
token_get_number(FILE *fp, unsigned long *num)
{
	struct token tk;
	char *ep;
	unsigned long v;

	if (!token_get_string(fp, &tk))
		return (false);

	errno = 0;
	v = strtoul(tk.token, &ep, 0);
	if ((ep == NULL) || (*ep != '\0') || ((v == 0) && (errno == EINVAL)) ||
	    ((v == ULONG_MAX) && (errno == ERANGE))) {
		logmsg_err("line %u: %s: %s", lineno, tk.token,
			   strerror(EINVAL));
		return (false);
	}

	*num = v;

	return (true);
}

bool
token_get_string(FILE *fp, struct token *tk)
{
	char *quot;
	int c;

	tk->length = 0;

	if (!token_skip_whitespace(fp)) {
		logmsg_err("premature EOF");
		return (false);
	}

	if ((c = fgetc(fp)) == EOF) {
		logmsg_err("premature EOF");
		return (false);
	}

	if ((quot = strchr("\"'", c)) == NULL) {
		do {
			tk->token[tk->length++] = (char)c;
			if (tk->length == sizeof(tk->token)) {
				logmsg_err("too long token");
				return (false);
			}
			if ((c = fgetc(fp)) == EOF) {
				logmsg_err("premature EOF");
				return (false);
			}
		} while (!isspace(c));
		if (c == '\n')
			lineno++;
	} else {
		if ((c = fgetc(fp)) == EOF) {
			logmsg_err("premature EOF");
			return (false);
		}
		while (c != *quot) {
			tk->token[tk->length++] = (char)c;
			if (tk->length == sizeof(tk->token)) {
				logmsg_err("too long token");
				return (false);
			}
			if ((c = fgetc(fp)) == EOF) {
				logmsg_err("premature EOF");
				return (false);
			}
			if (c == '\n')  {
				logmsg_err("missing close quotation %c",
					   *quot);
				return (false);
			}
		}
		if (tk->length == 0) {
			logmsg_err("empty string");
			return (false);
		}
	}

	tk->token[tk->length] = '\0';

	return (true);
}

bool
token_skip_whitespace(FILE *fp)
{
	int c;

	for (;;) {
		do {
			if ((c = fgetc(fp)) == EOF)
				return (false);
			if (c == '\n')
				lineno++;
		} while (isspace(c));

		if (c != '#')
			break;

		do {
			if ((c = fgetc(fp)) == EOF)
				return (false);
		} while (c != '\n');
		lineno++;
	}

	if (ungetc(c, fp) == EOF)
		return (false);

	return (true);
}
