#ifndef	__RIGEL_FCNTL_H__
#define	__RIGEL_FCNTL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __RIGEL__
#include <_ansi.h>
#endif

#define	_RS_FOPEN		(-1)	/* from sys/file.h, kernel use only */
#define	_RS_FREAD		0x0001	/* read enabled */
#define	_RS_FWRITE		0x0002	/* write enabled */
#define	_RS_FAPPEND	0x0400	/* append (writes guaranteed at the end) */
#define	_RS_FMARK		0x0010	/* internal; mark during gc() */
#define	_RS_FDEFER		0x0020	/* internal; defer for next gc pass */
#define	_RS_FASYNC		0x2000	/* signal pgrp when data ready */
#define	_RS_FSHLOCK	0x0080	/* BSD flock() shared lock present */
#define	_RS_FEXLOCK	0x0100	/* BSD flock() exclusive lock present */
#define	_RS_FCREAT		0x0040	/* open with file create */
#define	_RS_FTRUNC		0x0200	/* open with truncation */
#define	_RS_FEXCL		0x0080	/* error on open if file exists */
#define	_RS_FNBIO		0x1000	/* non blocking I/O (sys5 style) */
#define	_RS_FSYNC		0x101000	/* do all writes synchronously */
#define	_RS_FNONBLOCK	0x0800	/* non blocking I/O (POSIX style) */
#define	_RS_FNDELAY	_FNONBLOCK	/* non blocking I/O (4.2 style) */
#define	_RS_FNOCTTY	0x0100	/* don't assign a ctty on this open */

#define RS_O_ACCMODE (RS_O_RDONLY | RS_O_WRONLY | RS_O_RDWR)

/*
 * Flag values for open(2) and fcntl(2)
 * The kernel adds 1 to the open modes to turn it into some
 * combination of FREAD and FWRITE.
 */
#define	RS_O_RDONLY	0		/* +1 == FREAD */
#define	RS_O_WRONLY	1		/* +1 == FWRITE */
#define	RS_O_RDWR		2		/* +1 == FREAD|FWRITE */
#define	RS_O_APPEND	_RS_FAPPEND
#define	RS_O_CREAT		_RS_FCREAT
#define	RS_O_TRUNC		_RS_FTRUNC
#define	RS_O_EXCL		_RS_FEXCL
#define RS_O_SYNC		_RS_FSYNC
/*	RS_O_NDELAY	_RS_FNDELAY 	set in include/fcntl.h */
/*	RS_O_NDELAY	_RS_FNBIO 		set in include/fcntl.h */
#define	RS_O_NONBLOCK	_RS_FNONBLOCK
#define	RS_O_NOCTTY	_RS_FNOCTTY
/* NOTE Rigel (as well as many Linux kernels, particularly pre-2.6.33)
   do not implement DSYNC and RSYNC semantics as defined by POSIX.
   glibc maps DSYNC and RSYNC to vanilla SYNC.  We do the same. */
#define RS_O_DSYNC _RS_FSYNC
#define RS_O_RSYNC _RS_FSYNC

/* For machines which care - */
#if defined (_WIN32) || defined (__CYGWIN__)
#define RS__FBINARY        0x10000
#define RS__FTEXT          0x20000
#define RS__FNOINHERIT	0x40000

#define RS_O_BINARY	RS__FBINARY
#define RS_O_TEXT		RS__FTEXT
#define RS_O_NOINHERIT	RS__FNOINHERIT
/* O_CLOEXEC is the Linux equivalent to O_NOINHERIT */
#define RS_O_CLOEXEC	RS__FNOINHERIT

/* The windows header files define versions with a leading underscore.  */
#define RS__O_RDONLY	RS_O_RDONLY
#define RS__O_WRONLY	RS_O_WRONLY
#define RS__O_RDWR		RS_O_RDWR
#define RS__O_APPEND	RS_O_APPEND
#define RS__O_CREAT	RS_O_CREAT
#define RS__O_TRUNC	RS_O_TRUNC
#define RS__O_EXCL		RS_O_EXCL
#define RS__O_TEXT		RS_O_TEXT
#define RS__O_BINARY	RS_O_BINARY
#define RS__O_RAW		RS_O_BINARY
#define RS__O_NOINHERIT	RS_O_NOINHERIT
#endif

#ifndef	_POSIX_SOURCE

/*
 * Flags that work for fcntl(fd, F_SETFL, FXXXX)
 */
#define	RS_FAPPEND		_RS_FAPPEND
#define	RS_FSYNC		_RS_FSYNC
#define	RS_FASYNC		_RS_FASYNC
#define	RS_FNBIO		_RS_FNBIO
#define	RS_FNONBIO		_RS_FNONBLOCK	/* XXX fix to be NONBLOCK everywhere */
#define	RS_FNDELAY		_RS_FNDELAY

/*
 * Flags that are disallowed for fcntl's (FCNTLCANT);
 * used for opens, internal state, or locking.
 */
#define	RS_FREAD		_RS_FREAD
#define	RS_FWRITE		_RS_FWRITE
#define	RS_FMARK		_RS_FMARK
#define	RS_FDEFER		_RS_FDEFER
#define	RS_FSHLOCK		_RS_FSHLOCK
#define	RS_FEXLOCK		_RS_FEXLOCK

/*
 * The rest of the flags, used only for opens
 */
#define	RS_FOPEN		_RS_FOPEN
#define	RS_FCREAT		_RS_FCREAT
#define	RS_FTRUNC		_RS_FTRUNC
#define	RS_FEXCL		_RS_FEXCL
#define	RS_FNOCTTY		_RS_FNOCTTY

#endif	/* !_POSIX_SOURCE */

/* XXX close on exec request; must match UF_EXCLOSE in user.h */
#define	RS_FD_CLOEXEC	1	/* posix */

/* fcntl(2) requests */
#define	RS_F_DUPFD		0	/* Duplicate fildes */
#define	RS_F_GETFD		1	/* Get fildes flags (close on exec) */
#define	RS_F_SETFD		2	/* Set fildes flags (close on exec) */
#define	RS_F_GETFL		3	/* Get file flags */
#define	RS_F_SETFL		4	/* Set file flags */
#ifndef	_POSIX_SOURCE
#define	RS_F_GETOWN 	5	/* Get owner - for ASYNC */
#define	RS_F_SETOWN 	6	/* Set owner - for ASYNC */
#endif	/* !_POSIX_SOURCE */
#define	RS_F_GETLK  	7	/* Get record-locking information */
#define	RS_F_SETLK  	8	/* Set or Clear a record-lock (Non-Blocking) */
#define	RS_F_SETLKW 	9	/* Set or Clear a record-lock (Blocking) */
#ifndef	_POSIX_SOURCE
#define	RS_F_RGETLK 	10	/* Test a remote lock to see if it is blocked */
#define	RS_F_RSETLK 	11	/* Set or unlock a remote lock */
#define	RS_F_CNVT 		12	/* Convert a fhandle to an open fd */
#define	RS_F_RSETLKW 	13	/* Set or Clear remote record-lock(Blocking) */
#endif	/* !_POSIX_SOURCE */
#ifdef __CYGWIN__
#define	RS_F_DUPFD_CLOEXEC	14	/* As F_DUPFD, but set close-on-exec flag */
#endif

/* fcntl(2) flags (l_type field of flock structure) */
#define	RS_F_RDLCK		1	/* read lock */
#define	RS_F_WRLCK		2	/* write lock */
#define	RS_F_UNLCK		3	/* remove lock(s) */
#ifndef	_POSIX_SOURCE
#define	RS_F_UNLKSYS	4	/* remove remote locks for a given system */
#endif	/* !_POSIX_SOURCE */

#ifdef __CYGWIN__
/* Special descriptor value to denote the cwd in calls to openat(2) etc. */
#define RS_AT_FDCWD -2

/* Flag values for faccessat2) et al. */
#define RS_AT_EACCESS              1
#define RS_AT_SYMLINK_NOFOLLOW     2
#define RS_AT_SYMLINK_FOLLOW       4
#define RS_AT_REMOVEDIR            8
#endif

/*#include <sys/stdtypes.h>*/

/////////////////////////////////
/////////////////////////////////

//If we're actually compiling for a Rigel target, define non-namespaced
//versions of all the above macros
#ifdef __RIGEL__

#define O_ACCMODE RS_O_ACCMODE

/*
 * Flag values for open(2) and fcntl(2)
 * The kernel adds 1 to the open modes to turn it into some
 * combination of FREAD and FWRITE.
 */
#define	O_RDONLY RS_O_RDONLY
#define	O_WRONLY RS_O_WRONLY
#define	O_RDWR RS_O_RDWR	
#define	O_APPEND RS_O_APPEND
#define	O_CREAT RS_O_CREAT	
#define	O_TRUNC RS_O_TRUNC	
#define	O_EXCL RS_O_EXCL	
#define O_SYNC RS_O_SYNC	
/*	O_NDELAY	RS_O_FNDELAY 	set in include/fcntl.h */
/*	O_NDELAY	RS_O_FNBIO 		set in include/fcntl.h */
#define	O_NONBLOCK RS_O_NONBLOCK
#define	O_NOCTTY RS_O_NOCTTY
/* NOTE Rigel (as well as many Linux kernels, particularly pre-2.6.33)
   do not implement DSYNC and RSYNC semantics as defined by POSIX.
   glibc maps DSYNC and RSYNC to vanilla SYNC.  We do the same. */
#define O_DSYNC RS_O_DSYNC
#define O_RSYNC RS_O_RSYNC

/* For machines which care - */
#if defined (_WIN32) || defined (__CYGWIN__)
#define _FBINARY RS__FBINARY
#define _FTEXT RS__FTEXT
#define _FNOINHERIT RS__FNOINHERIT

#define O_BINARY RS_O_BINARY	RS__FBINARY
#define O_TEXT RS_O_TEXT		RS__FTEXT
#define O_NOINHERIT RS_O_NOINHERIT	RS__FNOINHERIT
/* O_CLOEXEC is the Linux equivalent to O_NOINHERIT */
#define O_CLOEXEC RS_O_CLOEXEC	RS__FNOINHERIT

/* The windows header files define versions with a leading underscore.  */
#define _O_RDONLY RS__O_RDONLY
#define _O_WRONLY RS__O_WRONLY
#define _O_RDWR RS__O_RDWR	
#define _O_APPEND RS__O_APPEND
#define _O_CREAT RS__O_CREAT
#define _O_TRUNC RS__O_TRUNC
#define _O_EXCL RS__O_EXCL	
#define _O_TEXT RS__O_TEXT	
#define _O_BINARY RS__O_BINARY
#define _O_RAW RS__O_RAW	
#define _O_NOINHERIT RS__O_NOINHERIT
#endif

#ifndef	_POSIX_SOURCE

/*
 * Flags that work for fcntl(fd, F_SETFL, FXXXX)
 */
#define	FAPPEND RS_FAPPEND
#define	FSYNC RS_FSYNC		
#define	FASYNC RS_FASYNC	
#define	FNBIO RS_FNBIO		
#define	FNONBIO RS_FNONBIO
#define	FNDELAY RS_FNDELAY	

/*
 * Flags that are disallowed for fcntl's (FCNTLCANT);
 * used for opens, internal state, or locking.
 */
#define	FREAD RS_FREAD
#define	FWRITE RS_FWRITE
#define	FMARK RS_FMARK
#define	FDEFER RS_FDEFER
#define	FSHLOCK RS_FSHLOCK
#define	FEXLOCK RS_FEXLOCK

/*
 * The rest of the flags, used only for opens
 */
#define	FOPEN RS_FOPEN	
#define	FCREAT RS_FCREAT
#define	FTRUNC RS_FTRUNC
#define	FEXCL RS_FEXCL	
#define	FNOCTTY RS_FNOCTTY

#endif	/* !_POSIX_SOURCE */

/* XXX close on exec request; must match UF_EXCLOSE in user.h */
#define	FD_CLOEXEC RS_FD_CLOEXEC

/* fcntl(2) requests */
#define	F_DUPFD RS_F_DUPFD
#define	F_GETFD RS_F_GETFD
#define	F_SETFD RS_F_SETFD
#define	F_GETFL RS_F_GETFL
#define	F_SETFL RS_F_SETFL
#ifndef	_POSIX_SOURCE
#define	F_GETOWN RS_F_GETOWN
#define	F_SETOWN RS_F_SETOWN
#endif	/* !_POSIX_SOURCE */
#define	F_GETLK RS_F_GETLK  
#define	F_SETLK RS_F_SETLK  
#define	F_SETLKW RS_F_SETLKW
#ifndef	_POSIX_SOURCE
#define	F_RGETLK RS_F_RGETLK
#define	F_RSETLK RS_F_RSETLK 
#define	F_CNVT RS_F_CNVT 	
#define	F_RSETLKW RS_F_RSETLKW
#endif	/* !_POSIX_SOURCE */
#ifdef __CYGWIN__
#define	F_DUPFD_CLOEXEC RS_F_DUPFD_CLOEXEC
#endif

/* fcntl(2) flags (l_type field of flock structure) */
#define	F_RDLCK RS_F_RDLCK
#define	F_WRLCK RS_F_WRLCK
#define	F_UNLCK RS_F_UNLCK
#ifndef	_POSIX_SOURCE
#define	F_UNLKSYS RS_F_UNLKSYS
#endif	/* !_POSIX_SOURCE */

#ifdef __CYGWIN__
/* Special descriptor value to denote the cwd in calls to openat(2) etc. */
#define AT_FDCWD RS_AT_FDCWD

/* Flag values for faccessat2) et al. */
#define AT_EACCESS RS_AT_EACCESS
#define AT_SYMLINK RS_AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_FOLLOW RS_AT_SYMLINK_FOLLOW
#define AT_REMOVEDIR RS_AT_REMOVEDIR
#endif /* #ifdef __CYGWIN__ */

#ifndef __CYGWIN__
/* file segment locking set data type - information passed to system by user */
struct flock {
	short	l_type;		/* F_RDLCK, F_WRLCK, or F_UNLCK */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to EOF */
	short	l_pid;		/* returned with F_GETLK */
	short	l_xxx;		/* reserved for future use */
};
#endif /* __CYGWIN__ */

#ifndef	_POSIX_SOURCE
/* extended file segment locking set data type */
struct eflock {
	short	l_type;		/* F_RDLCK, F_WRLCK, or F_UNLCK */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to EOF */
	short	l_pid;		/* returned with F_GETLK */
	short	l_xxx;		/* reserved for future use */
	long	l_rpid;		/* Remote process id wanting this lock */
	long	l_rsys;		/* Remote system id wanting this lock */
};
#endif	/* !_POSIX_SOURCE */


#include <sys/types.h>
#include <sys/stat.h>		/* sigh. for the mode bits for open/creat */

extern int open _PARAMS ((const char *, int, ...));
extern int creat _PARAMS ((const char *, mode_t));
extern int fcntl _PARAMS ((int, int, ...));
#ifdef __CYGWIN__
#include <sys/time.h>
extern int futimesat _PARAMS ((int, const char *, const struct timeval *));
extern int openat _PARAMS ((int, const char *, int, ...));
#endif

/* Provide _<systemcall> prototypes for functions provided by some versions
   of newlib.  */
#ifdef _COMPILING_NEWLIB
extern int _open _PARAMS ((const char *, int, ...));
extern int _fcntl _PARAMS ((int, int, ...));
#ifdef __LARGE64_FILES
extern int _open64 _PARAMS ((const char *, int, ...));
#endif
#endif

#endif /* #ifdef RIGEL */
 
#ifdef __cplusplus
}
#endif

#endif	/* ifndef __RIGEL_FCNTL_H__ */
