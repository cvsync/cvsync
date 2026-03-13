/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef __DEFS_H__
#define	__DEFS_H__

struct scanfile_attr;

bool cvsup_init(const char *, const char *);
bool cvsup_write_header(int, time_t);
bool cvsup_write_dirdown(int, struct scanfile_attr *);
bool cvsup_write_dirup(int, struct scanfile_attr *);
bool cvsup_write_file(int, struct scanfile_attr *);
bool cvsup_write_rcs(int, struct scanfile_attr *);
bool cvsup_write_symlink(int, struct scanfile_attr *);

#endif /* __DEFS_H__ */
