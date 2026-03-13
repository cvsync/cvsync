/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_CVSYNC_H
#define	CVSYNC_CVSYNC_H

#define	CVSYNC_MAXCMDLEN	(2048)
#define	CVSYNC_MAXADDRLEN	(128)

#define	CVSYNC_DEFAULT_PORT	"7777"

#define	CVSYNC_TMPFILE		".cvsync.XXXXXX"
#define	CVSYNC_TMPFILE_LEN	(14)	/* == strlen(CVSYNC_TMPFILE) */

#define	CVSYNC_THREAD_FAILURE	((void *)false)
#define	CVSYNC_THREAD_SUCCESS	((void *)true)

#define	CVSYNC_BSIZE		(1048576) /* 1MB */

enum {
	CVSYNC_NO_ERROR		= 0x00,
	CVSYNC_ERROR_DENIED	= 0x01,
	CVSYNC_ERROR_LIMITED	= 0x02,
	CVSYNC_ERROR_UNAVAIL	= 0x03,
	CVSYNC_ERROR_UNSPEC	= 0xff
};

enum {
	CVSYNC_COMPRESS_UNSPEC,
	CVSYNC_COMPRESS_NO,
	CVSYNC_COMPRESS_ZLIB
};

enum {
	CVSYNC_COMPRESS_LEVEL_UNSPEC	= -1,
	CVSYNC_COMPRESS_LEVEL_NO	= 0,
	CVSYNC_COMPRESS_LEVEL_SPEED	= 1,
	CVSYNC_COMPRESS_LEVEL_BEST	= 9
};

enum {
	CVSYNC_ERRORMODE_UNSPEC = 0,
	CVSYNC_ERRORMODE_ABORT,
	CVSYNC_ERRORMODE_FIXUP,
	CVSYNC_ERRORMODE_IGNORE
};

enum {
	CVSYNC_LIST_UNKNOWN = 0,

	CVSYNC_LIST_ALL,
	CVSYNC_LIST_RCS,

	CVSYNC_LIST_MAX
};

enum {
	CVSYNC_RELEASE_UNKNOWN = 0,

	CVSYNC_RELEASE_LIST,
	CVSYNC_RELEASE_RCS,

	CVSYNC_RELEASE_MAX
};

struct cvsync_file {
	int	cf_fileno;
	off_t	cf_size;
	time_t	cf_mtime;
	mode_t	cf_mode;

	void	*cf_addr;
	size_t	cf_msize;
};

bool cvsync_init(void);

int cvsync_compress_pton(const char *);
const char *cvsync_compress_ntop(int);
int cvsync_list_pton(const char *);
const char *cvsync_list_ntop(int);
int cvsync_release_pton(const char *);
const char *cvsync_release_ntop(int);

void *cvsync_memdup(void *, size_t);
int cvsync_cmp_pathname(const char *, size_t, const char *, size_t);
struct cvsync_file *cvsync_fopen(const char *);
bool cvsync_fclose(struct cvsync_file *);
bool cvsync_mmap(struct cvsync_file *, off_t, off_t);
bool cvsync_munmap(struct cvsync_file *);
void cvsync_signal(int);
bool cvsync_is_interrupted(void);
bool cvsync_is_terminated(void);

bool cvsync_rcs_append_attic(char *, size_t, size_t);
bool cvsync_rcs_insert_attic(char *, size_t, size_t);
bool cvsync_rcs_remove_attic(char *, size_t);
bool cvsync_rcs_filename(const char *, size_t);
bool cvsync_rcs_pathname(const char *, size_t);

#endif /* CVSYNC_CVSYNC_H */
