/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DEFS_H
#define	CVSYNC_DEFS_H

struct scanfile_attr;

bool cvsup_init(const char *, const char *);
bool cvsup_write_header(int, time_t);
bool cvsup_write_dirdown(int, struct scanfile_attr *);
bool cvsup_write_dirup(int, struct scanfile_attr *);
bool cvsup_write_file(int, struct scanfile_attr *);
bool cvsup_write_rcs(int, struct scanfile_attr *);
bool cvsup_write_symlink(int, struct scanfile_attr *);

#endif /* CVSYNC_DEFS_H */
