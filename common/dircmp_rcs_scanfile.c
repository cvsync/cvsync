/*-
 * Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "attribute.h"
#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "filetypes.h"
#include "list.h"
#include "mux.h"
#include "scanfile.h"

#include "dircmp.h"
#include "filescan.h"

bool dircmp_rcs_scanfile_add(struct dircmp_args *, struct scanfile_attr *);
bool dircmp_rcs_scanfile_add_file(struct dircmp_args *, struct scanfile_attr *);
bool dircmp_rcs_scanfile_remove(struct dircmp_args *, struct scanfile_attr *);
bool dircmp_rcs_scanfile_remove_dir(struct dircmp_args *);
bool dircmp_rcs_scanfile_remove_file(struct dircmp_args *);
bool dircmp_rcs_scanfile_replace(struct dircmp_args *, struct scanfile_attr *);
bool dircmp_rcs_scanfile_update(struct dircmp_args *, struct scanfile_attr *);

bool dircmp_rcs_scanfile_fetch(struct dircmp_args *);
bool dircmp_rcs_scanfile_read(struct dircmp_args *, struct scanfile_attr *);
bool dircmp_isparent(struct scanfile_attr *, struct scanfile_attr *);

bool
dircmp_rcs_scanfile(struct dircmp_args *dca)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	struct collection *cl = dca->dca_collection;
	struct scanfile_args *sa = cl->cl_scanfile;
	struct scanfile_attr *attr = &sa->sa_attr, *dirattr;
	struct list *lp;
	const uint8_t *name, *sv_name;
	size_t namelen;
	int rv;
	bool fetched = false;

	if ((lp = list_init()) == NULL)
		return (false);
	list_set_destructor(lp, free);

	dirattr = NULL;

	if (dircmp_rcs_scanfile_read(dca, attr)) {
		sa->sa_start += attr->a_size;
	} else {
		if (sa->sa_start != sa->sa_end) {
			list_destroy(lp);
			return (false);
		}
		attr = NULL;
	}

	for (;;) {
		if (!fetched) {
			if (!dircmp_rcs_scanfile_fetch(dca)) {
				if (dirattr != NULL)
					free(dirattr);
				list_destroy(lp);
				return (false);
			}
			fetched = true;
		}

		if (cap->ca_tag == DIRCMP_END)
			break;

		if (cap->ca_tag == DIRCMP_UP) {
			if (dirattr == NULL) {
				list_destroy(lp);
				return (false);
			}

			if (attr != NULL) {
				while (dircmp_isparent(dirattr, attr)) {
					if (!dircmp_rcs_scanfile_add(dca,
								     attr)) {
						free(dirattr);
						list_destroy(lp);
						return (false);
					}
					if (dircmp_rcs_scanfile_read(dca,
								     attr)) {
						sa->sa_start += attr->a_size;
						continue;
					}
					if (sa->sa_start != sa->sa_end) {
						free(dirattr);
						list_destroy(lp);
						return (false);
					}
					attr = NULL;
					break;
				}
			}

			free(dirattr);

			if (list_isempty(lp)) {
				dirattr = NULL;
			} else {
				if ((dirattr = list_remove_tail(lp)) == NULL) {
					list_destroy(lp);
					return (false);
				}
			}
			fetched = false;
			continue;
		}

		if ((attr == NULL) || !dircmp_isparent(dirattr, attr)) {
			if (!dircmp_rcs_scanfile_remove(dca, dirattr)) {
				if (dirattr != NULL)
					free(dirattr);
				list_destroy(lp);
				return (false);
			}
			fetched = false;
			continue;
		}

		sv_name = attr->a_name;
		for (name = &sv_name[attr->a_namelen - 1] ;
		     name > sv_name ;
		     name--) {
			if (*name == '/') {
				name++;
				break;
			}
		}
		namelen = attr->a_namelen - (size_t)(name - sv_name);

		if (namelen == cap->ca_namelen) {
			rv = memcmp(name, cap->ca_name, namelen);
		} else {
			if (namelen < cap->ca_namelen) {
				rv = memcmp(name, cap->ca_name, namelen);
			} else {
				rv = memcmp(name, cap->ca_name,
					    cap->ca_namelen);
			}
			if (rv == 0) {
				if (namelen < cap->ca_namelen)
					rv = -1;
				else
					rv = 1;
			}
		}
		if (rv == 0) {
			if (!dircmp_rcs_scanfile_update(dca, attr)) {
				if (dirattr != NULL)
					free(dirattr);
				list_destroy(lp);
				return (false);
			}
			if (cap->ca_tag == DIRCMP_DOWN) {
				if ((dirattr != NULL) &&
				    !list_insert_tail(lp, dirattr)) {
					free(dirattr);
					list_destroy(lp);
					return (false);
				}

				dirattr = cvsync_memdup(attr, sizeof(*attr));
				if (dirattr == NULL) {
					list_destroy(lp);
					return (false);
				}

				if (dircmp_rcs_scanfile_read(dca, attr)) {
					sa->sa_start += attr->a_size;
				} else {
					if (sa->sa_start != sa->sa_end) {
						free(dirattr);
						list_destroy(lp);
						return (false);
					}
					attr = NULL;
				}
			} else {
				if (dircmp_rcs_scanfile_read(dca, attr)) {
					sa->sa_start += attr->a_size;
				} else {
					if (sa->sa_start != sa->sa_end) {
						free(dirattr);
						list_destroy(lp);
						return (false);
					}
					attr = NULL;
				}
			}
			fetched = false;
		} else if (rv < 0) {
			if (!dircmp_rcs_scanfile_add(dca, attr)) {
				if (dirattr != NULL)
					free(dirattr);
				list_destroy(lp);
				return (false);
			}
			if (dircmp_rcs_scanfile_read(dca, attr)) {
				sa->sa_start += attr->a_size;
			} else {
				if (sa->sa_start != sa->sa_end) {
					if (dirattr != NULL)
						free(dirattr);
					list_destroy(lp);
					return (false);
				}
				attr = NULL;
			}
		} else { /* rv > 0 */
			if (!dircmp_rcs_scanfile_remove(dca, dirattr)) {
				if (dirattr != NULL)
					free(dirattr);
				list_destroy(lp);
				return (false);
			}
			fetched = false;
		}
	}

	if (attr != NULL) {
		if (!dircmp_rcs_scanfile_add(dca, attr)) {
			list_destroy(lp);
			return (false);
		}
	} else {
		attr = &sa->sa_attr;
	}
	while (sa->sa_start < sa->sa_end) {
		if (!dircmp_rcs_scanfile_read(dca, attr)) {
			list_destroy(lp);
			return (false);
		}
		sa->sa_start += attr->a_size;

		if (!dircmp_rcs_scanfile_add(dca, attr)) {
			list_destroy(lp);
			return (false);
		}
	}

	list_destroy(lp);

	return (true);
}

bool
dircmp_rcs_scanfile_fetch(struct dircmp_args *dca)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	uint8_t *cmd = dca->dca_cmd;
	size_t len;

	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd, 3))
		return (false);
	len = GetWord(cmd);
	if ((len == 0) || (len > dca->dca_cmdmax - 2))
		return (false);
	if ((cap->ca_tag = cmd[2]) == DIRCMP_END)
		return (len == 1);
	if (cap->ca_tag == DIRCMP_UP) {
		cap->ca_type = FILETYPE_DIR;
		return (len == 1);
	}
	if (len < 2)
		return (false);

	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cmd, 1))
		return (false);
	cap->ca_namelen = cmd[0];

	switch (cap->ca_tag) {
	case DIRCMP_DOWN:
		cap->ca_type = FILETYPE_DIR;
		cap->ca_auxlen = RCS_ATTRLEN_DIR;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name)) ||
		    (cap->ca_namelen != len - cap->ca_auxlen - 2)) {
			return (false);
		}
		break;
	case DIRCMP_FILE:
		cap->ca_type = FILETYPE_FILE;
		cap->ca_auxlen = RCS_ATTRLEN_FILE;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name)) ||
		    (cap->ca_namelen != len - cap->ca_auxlen - 2)) {
			return (false);
		}
		break;
	case DIRCMP_RCS:
	case DIRCMP_RCS_ATTIC:
		if (cap->ca_tag == DIRCMP_RCS)
			cap->ca_type = FILETYPE_RCS;
		else
			cap->ca_type = FILETYPE_RCS_ATTIC;
		cap->ca_auxlen = RCS_ATTRLEN_RCS;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name)) ||
		    (cap->ca_namelen != len - cap->ca_auxlen - 2)) {
			return (false);
		}
		break;
	case DIRCMP_SYMLINK:
		cap->ca_type = FILETYPE_SYMLINK;
		if ((cap->ca_namelen == 0) ||
		    (cap->ca_namelen > sizeof(cap->ca_name))) {
			return (false);
		}
		if (len <= cap->ca_namelen + 2)
			return (false);
		cap->ca_auxlen = len - cap->ca_namelen - 2;
		if (cap->ca_auxlen > sizeof(cap->ca_aux))
			return (false);
		break;
	default:
		return (false);
	}

	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}
	if (!cvsync_rcs_filename((char *)cap->ca_name, cap->ca_namelen))
		return (false);
	if (!mux_recv(dca->dca_mux, MUX_DIRCMP_IN, cap->ca_aux,
		      cap->ca_auxlen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_scanfile_add(struct dircmp_args *dca, struct scanfile_attr *attr)
{
	struct scanfile_args *sa = dca->dca_collection->cl_scanfile;
	struct scanfile_attr *dirattr;
	struct list *lp;

	if (!dircmp_rcs_scanfile_add_file(dca, attr))
		return (false);

	if (attr->a_type != FILETYPE_DIR)
		return (true);

	if ((lp = list_init()) == NULL)
		return (false);
	list_set_destructor(lp, free);

	if ((dirattr = cvsync_memdup(attr, sizeof(*attr))) == NULL) {
		list_destroy(lp);
		return (false);
	}

	if (!list_insert_tail(lp, dirattr)) {
		free(dirattr);
		list_destroy(lp);
		return (false);
	}

	if (dircmp_rcs_scanfile_read(dca, attr)) {
		sa->sa_start += attr->a_size;
	} else {
		if (sa->sa_start != sa->sa_end) {
			list_destroy(lp);
			return (false);
		}
		attr = NULL;
	}

	do {
		if ((dirattr = list_remove_tail(lp)) == NULL) {
			list_destroy(lp);
			return (false);
		}

		while ((attr != NULL) && dircmp_isparent(dirattr, attr)) {
			switch (attr->a_type) {
			case FILETYPE_DIR:
				if (!list_insert_tail(lp, dirattr)) {
					free(dirattr);
					list_destroy(lp);
					return (false);
				}
				if (!dircmp_rcs_scanfile_add_file(dca, attr)) {
					list_destroy(lp);
					return (false);
				}
				dirattr = cvsync_memdup(attr, sizeof(*attr));
				if (dirattr == NULL) {
					list_destroy(lp);
					return (false);
				}
				break;
			case FILETYPE_FILE:
			case FILETYPE_RCS:
			case FILETYPE_RCS_ATTIC:
			case FILETYPE_SYMLINK:
				if (!dircmp_rcs_scanfile_add_file(dca, attr)) {
					free(dirattr);
					list_destroy(lp);
					return (false);
				}
				break;
			default:
				free(dirattr);
				list_destroy(lp);
				return (false);
			}

			if (!dircmp_rcs_scanfile_read(dca, attr)) {
				if (sa->sa_start == sa->sa_end) {
					attr = NULL;
					break;
				}
				free(dirattr);
				list_destroy(lp);
				return (false);
			}
			sa->sa_start += attr->a_size;
		}

		free(dirattr);
	} while (!list_isempty(lp));

	if (attr != NULL)
		sa->sa_start -= attr->a_size;

	list_destroy(lp);

	return (true);
}

bool
dircmp_rcs_scanfile_add_file(struct dircmp_args *dca,
			     struct scanfile_attr *attr)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len;

	if ((len = attr->a_namelen + 6) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_ADD;
	cmd[3] = attr->a_type;
	SetWord(&cmd[4], attr->a_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, attr->a_name,
		      attr->a_namelen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_scanfile_remove(struct dircmp_args *dca, struct scanfile_attr *attr)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	size_t len, *plen_stack, c, n;

	if (attr != NULL) {
		dca->dca_pathlen = attr->a_namelen + 1;
		if (dca->dca_pathlen >= dca->dca_pathmax)
			return (false);
		(void)memcpy(dca->dca_path, attr->a_name, attr->a_namelen);
		dca->dca_path[attr->a_namelen] = '/';
	} else {
		dca->dca_pathlen = 0;
	}

	if (cap->ca_type != FILETYPE_DIR)
		return (dircmp_rcs_scanfile_remove_file(dca));

	n = 4;
	if ((plen_stack = malloc(n * sizeof(*plen_stack))) == NULL)
		return (false);
	plen_stack[0] = dca->dca_pathlen;
	c = 1;

	len =  dca->dca_pathlen + cap->ca_namelen + 1;
	if (len >= dca->dca_pathmax) {
		free(plen_stack);
		return (false);
	}

	(void)memcpy(&dca->dca_path[dca->dca_pathlen], cap->ca_name,
		     cap->ca_namelen);
	dca->dca_path[len - 1] = '/';
	dca->dca_path[len] = '\0';
	dca->dca_pathlen = len;

	do {
		if (!dircmp_rcs_scanfile_fetch(dca)) {
			free(plen_stack);
			return (false);
		}

		switch (cap->ca_tag) {
		case DIRCMP_END:
			free(plen_stack);
			return (false);
		case DIRCMP_DOWN:
			if (c == n) {
				size_t *newp, old = n, new = old * 2;

				newp = malloc(new * sizeof(*newp));
				if (newp == NULL) {
					free(plen_stack);
					return (false);
				}
				(void)memcpy(newp, plen_stack,
					     old * sizeof(*newp));
				(void)memset(&newp[old], 0,
					     (new - old) * sizeof(*newp));

				free(plen_stack);
				plen_stack = newp;
				n = new;
			}
			plen_stack[c++] = dca->dca_pathlen;

			len = dca->dca_pathlen + cap->ca_namelen + 1;
			if (len >= dca->dca_pathmax) {
				free(plen_stack);
				return (false);
			}

			(void)memcpy(&dca->dca_path[dca->dca_pathlen],
				     cap->ca_name, cap->ca_namelen);
			dca->dca_path[len - 1] = '/';
			dca->dca_path[len] = '\0';
			dca->dca_pathlen = len;

			break;
		case DIRCMP_UP:
			dca->dca_path[dca->dca_pathlen - 1] = '\0';

			if (!dircmp_rcs_scanfile_remove_dir(dca)) {
				free(plen_stack);
				return (false);
			}

			dca->dca_pathlen = plen_stack[--c];
			dca->dca_path[dca->dca_pathlen] = '\0';

			break;
		case DIRCMP_FILE:
		case DIRCMP_RCS:
		case DIRCMP_RCS_ATTIC:
		case DIRCMP_SYMLINK:
			if (!dircmp_rcs_scanfile_remove_file(dca)) {
				free(plen_stack);
				return (false);
			}
			break;
		default:
			free(plen_stack);
			return (false);
		}
	} while (c > 0);

	free(plen_stack);

	return (true);
}

bool
dircmp_rcs_scanfile_remove_dir(struct dircmp_args *dca)
{
	uint8_t *cmd = dca->dca_cmd;
	size_t len;

	if ((len = dca->dca_pathlen + 5) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_REMOVE;
	cmd[3] = FILETYPE_DIR;
	SetWord(&cmd[4], dca->dca_pathlen - 1);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_path,
		      dca->dca_pathlen - 1)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_scanfile_remove_file(struct dircmp_args *dca)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	uint8_t *cmd = dca->dca_cmd;
	size_t len;

	if ((len = dca->dca_pathlen + cap->ca_namelen + 6) > dca->dca_cmdmax)
		return (false);

	SetWord(cmd, len - 2);
	cmd[2] = FILESCAN_REMOVE;
	cmd[3] = cap->ca_type;
	SetWord(&cmd[4], dca->dca_pathlen + cap->ca_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, dca->dca_path,
		      dca->dca_pathlen)) {
		return (false);
	}
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cap->ca_name,
		      cap->ca_namelen)) {
		return (false);
	}

	return (true);
}

bool
dircmp_rcs_scanfile_replace(struct dircmp_args *dca,
			    struct scanfile_attr *attr)
{
	char *sp = attr->a_name, *bp = sp + attr->a_namelen - 1, *p;
	size_t sv_namelen = attr->a_namelen;

	switch (attr->a_type) {
	case FILETYPE_DIR:
		/* Nothing to do. */
		break;
	case FILETYPE_FILE:
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
	case FILETYPE_SYMLINK:
		for (p = bp ; p >= sp ; p--) {
			attr->a_namelen--;
			if (*p == '/')
				break;
		}
		break;
	default:
		return (false);
	}

	if (attr->a_namelen == 0) {
		if (!dircmp_rcs_scanfile_remove(dca, NULL))
			return (false);
	} else {
		if (!dircmp_rcs_scanfile_remove(dca, attr))
			return (false);
	}
	attr->a_namelen = sv_namelen;
	if (!dircmp_rcs_scanfile_add(dca, attr))
		return (false);

	return (true);
}

bool
dircmp_rcs_scanfile_update(struct dircmp_args *dca, struct scanfile_attr *attr)
{
	struct cvsync_attr *cap = &dca->dca_attr;
	uint8_t *cmd = dca->dca_cmd;
	size_t base, len;

	if ((base = attr->a_namelen + 6) > dca->dca_cmdmax)
		return (false);

	switch (attr->a_type) {
	case FILETYPE_DIR:
		if (cap->ca_type != FILETYPE_DIR)
			return (dircmp_rcs_scanfile_replace(dca, attr));
		if (attr->a_auxlen != cap->ca_auxlen)
			return (false);
		if (memcmp(cap->ca_aux, attr->a_aux, attr->a_auxlen) == 0)
			return (true);
		cmd[2] = FILESCAN_SETATTR;
		len = attr->a_auxlen;
		break;
	case FILETYPE_FILE:
		if (cap->ca_type != FILETYPE_FILE)
			return (dircmp_rcs_scanfile_replace(dca, attr));
		if (attr->a_auxlen != cap->ca_auxlen)
			return (false);
		if (memcmp(cap->ca_aux, attr->a_aux, attr->a_auxlen) == 0)
			return (true);
		if (GetDDWord(&((uint8_t *)attr->a_aux)[8]) == 0)
			return (dircmp_rcs_scanfile_replace(dca, attr));
		if (memcmp(cap->ca_aux, attr->a_aux, attr->a_auxlen - 2) == 0)
			cmd[2] = FILESCAN_SETATTR;
		else
			cmd[2] = FILESCAN_UPDATE;
		len = attr->a_auxlen;
		break;
	case FILETYPE_RCS:
	case FILETYPE_RCS_ATTIC:
		if ((cap->ca_type != FILETYPE_RCS) &&
		    (cap->ca_type != FILETYPE_RCS_ATTIC)) {
			return (dircmp_rcs_scanfile_replace(dca, attr));
		}
		if (attr->a_auxlen != cap->ca_auxlen)
			return (false);
		if (memcmp(cap->ca_aux, attr->a_aux, attr->a_auxlen) == 0)
			return (true);
		if (cap->ca_type != attr->a_type) {
			cmd[2] = FILESCAN_RCS_ATTIC;
		} else {
			if (memcmp(cap->ca_aux, attr->a_aux,
				   attr->a_auxlen - 2) == 0) {
				cmd[2] = FILESCAN_SETATTR;
			} else {
				cmd[2] = FILESCAN_UPDATE;
			}
		}
		len = attr->a_auxlen;
		break;
	case FILETYPE_SYMLINK:
		if (cap->ca_type != FILETYPE_SYMLINK)
			return (dircmp_rcs_scanfile_replace(dca, attr));
		if ((attr->a_auxlen == cap->ca_auxlen) &&
		    (memcmp(cap->ca_aux, attr->a_aux, attr->a_auxlen) == 0)) {
			return (true);
		}
		cmd[2] = FILESCAN_UPDATE;
		len = 0;
		break;
	default:
		return (false);
	}

	SetWord(cmd, len + base - 2);
	cmd[3] = attr->a_type;
	SetWord(&cmd[4], attr->a_namelen);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, cmd, 6))
		return (false);
	if (!mux_send(dca->dca_mux, MUX_FILESCAN, attr->a_name,
		      attr->a_namelen)) {
		return (false);
	}
	if (len > 0) {
		if (!mux_send(dca->dca_mux, MUX_FILESCAN, attr->a_aux,
			      attr->a_auxlen)) {
			return (false);
		}
	}

	return (true);
}

bool
dircmp_rcs_scanfile_read(struct dircmp_args *dca, struct scanfile_attr *attr)
{
	struct collection *cl = dca->dca_collection;
	struct scanfile_args *sa = cl->cl_scanfile;
	struct scanfile_attr rpref_attr;
	char *name;

	if (sa->sa_start == sa->sa_end)
		return (false);

	rpref_attr.a_name = cl->cl_rprefix;
	rpref_attr.a_namelen = cl->cl_rprefixlen + 1;

	for (;;) {
		if (!scanfile_read_attr(sa->sa_start, sa->sa_end, attr))
			return (false);

		if (cl->cl_rprefixlen == 0) {
			if (dircmp_access_scanfile(dca, attr))
				break;
			sa->sa_start += attr->a_size;
			if (sa->sa_start == sa->sa_end)
				return (false);
			continue;
		}

		if (attr->a_namelen <= cl->cl_rprefixlen) {
			if (!dircmp_isparent(attr, &rpref_attr)) {
				sa->sa_start += attr->a_size;
				if (sa->sa_start == sa->sa_end)
					return (false);
				continue;
			}
			if (dircmp_access_scanfile(dca, attr))
				break;
			sa->sa_start += attr->a_size;
			if (sa->sa_start == sa->sa_end)
				return (false);
			continue;
		}

		name = attr->a_name;
		if ((name[cl->cl_rprefixlen] == '/') &&
		    (memcmp(name, cl->cl_rprefix, cl->cl_rprefixlen) == 0)) {
			if (dircmp_access_scanfile(dca, attr))
				break;
			sa->sa_start += attr->a_size;
			if (sa->sa_start == sa->sa_end)
				return (false);
			continue;
		}

		sa->sa_start += attr->a_size;
		if (sa->sa_start == sa->sa_end)
			return (false);
	}

	return (true);
}

bool
dircmp_isparent(struct scanfile_attr *dirattr, struct scanfile_attr *attr)
{
	if (dirattr == NULL)
		return (true);
	if (dirattr->a_type != FILETYPE_DIR)
		return (false);

	if (dirattr->a_namelen >= attr->a_namelen)
		return (false);
	if (((char *)attr->a_name)[dirattr->a_namelen] != '/')
		return (false);
	if (memcmp(dirattr->a_name, attr->a_name, dirattr->a_namelen) != 0)
		return (false);

	return (true);
}
