/*-
 * Copyright (c) 2000-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

#ifndef __FILETYPES_H__
#define	__FILETYPES_H__

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

#endif /* __FILETYPES_H__ */
