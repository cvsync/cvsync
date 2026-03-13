/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_FILETYPES_H
#define	CVSYNC_FILETYPES_H

#define	IS_DIR_ATTIC(n, l)	(((l) == 5) && ((n)[0] == 'A') &&	\
				 ((n)[1] == 't') && ((n)[2] == 't') &&	\
				 ((n)[3] == 'i') && ((n)[4] == 'c'))
#define	IS_DIR_CURRENT(n, l)	(((l) == 1) && ((n)[0] == '.'))
#define	IS_DIR_CVS(n, l)	(((l) == 3) && ((n)[0] == 'C') &&	\
				 ((n)[1] == 'V') && ((n)[2] == 'S'))
#define	IS_DIR_PARENT(n, l)	(((l) == 2) && ((n)[0] == '.') &&	\
				 ((n)[1] == '.'))
#define	IS_FILE_CVSLOCK(n, l)	((((l) == 9) && ((n)[0] == '#') &&	\
				  ((n)[1] == 'c') && ((n)[2] == 'v') &&	\
				  ((n)[3] == 's') && ((n)[4] == '.') &&	\
				  ((n)[5] == 'l') && ((n)[6] == 'o') &&	\
				  ((n)[7] == 'c') && ((n)[8] == 'k')) || \
				 (((l) > 9) && ((n)[0] == '#') &&	\
				  ((n)[1] == 'c') && ((n)[2] == 'v') &&	\
				  ((n)[3] == 's') && ((n)[4] == '.') &&	\
				  (((n)[5] == 'r') || ((n)[5] == 't') || \
				   ((n)[5] == 'w')) &&			\
				  ((n)[6] == 'f') && ((n)[7] == 'l') &&	\
				  ((n)[8] == '.')))
#define	IS_FILE_RCS(n, l)	(((l) > 2) && ((n)[(l) - 2] == ',') &&	\
				 ((n)[(l) - 1] == 'v'))
#define	IS_FILE_TMPFILE(n, l)	(((l) == 14) && ((n)[0] == '.') &&	\
				 ((n)[1] == 'c') && ((n)[2] == 'v') &&	\
				 ((n)[3] == 's') && ((n)[4] == 'y') &&	\
				 ((n)[5] == 'n') && ((n)[6] == 'c') &&	\
				 ((n)[7] == '.')) /* CVSYNC_TMPFILE */

#define	FILETYPE_BLKDEV		'B'
#define	FILETYPE_CHRDEV		'C'
#define	FILETYPE_DIR		'D'
#define	FILETYPE_FILE		'F'
#define	FILETYPE_LINK		'L'
#define	FILETYPE_RCS		'R'
#define	FILETYPE_RCS_ATTIC	'r'
#define	FILETYPE_SYMLINK	'S'

#endif /* CVSYNC_FILETYPES_H */
