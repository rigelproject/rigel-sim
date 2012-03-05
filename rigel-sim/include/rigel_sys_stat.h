#ifndef	__RIGEL_SYS_STAT_H__
#define	__RIGEL_SYS_STAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __RIGEL__
#include <_ansi.h>
#endif
#include <time.h>
#include <sys/types.h>

/* dj's stat defines _STAT_H_ */
#ifndef _STAT_H_

/* It is intended that the layout of this structure not change when the
   sizes of any of the basic types change (short, int, long) [via a compile
   time option].  */

#ifdef __RIGEL__

#ifdef __CYGWIN__
#include <cygwin/stat.h>
#ifdef _COMPILING_NEWLIB
#define stat64 __stat64
#endif
#else
struct	stat 
{
  dev_t		st_dev;
  ino_t		st_ino;
  mode_t	st_mode;
  nlink_t	st_nlink;
  uid_t		st_uid;
  gid_t		st_gid;
  dev_t		st_rdev;
  off_t		st_size;
#if defined(__rtems__)
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  blksize_t     st_blksize;
  blkcnt_t	st_blocks;
#else
  /* SysV/sco doesn't have the rest... But Solaris, eabi does.  */
#if defined(__svr4__) && !defined(__PPC__) && !defined(__sun__)
  time_t	st_atime;
  time_t	st_mtime;
  time_t	st_ctime;
#else
  time_t	st_atime;
  long		st_spare1;
  time_t	st_mtime;
  long		st_spare2;
  time_t	st_ctime;
  long		st_spare3;
  long		st_blksize;
  long		st_blocks;
  long	st_spare4[2];
#endif
#endif
};

#if defined(__rtems__)
#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec
#endif

#endif /* #ifdef __CYGWIN__ */

#endif /* #ifdef __RIGEL__ */

#define	  _RS_IFMT	0170000	/* type of file */
#define		_RS_IFDIR	0040000	/* directory */
#define		_RS_IFCHR	0020000	/* character special */
#define		_RS_IFBLK	0060000	/* block special */
#define		_RS_IFREG	0100000	/* regular */
#define		_RS_IFLNK	0120000	/* symbolic link */
#define		_RS_IFSOCK	0140000	/* socket */
#define		_RS_IFIFO	0010000	/* fifo */

#define 	RS_S_BLKSIZE  1024 /* size of a block */

#define	RS_S_ISUID		0004000	/* set user id on execution */
#define	RS_S_ISGID		0002000	/* set group id on execution */
#define	RS_S_ISVTX		0001000	/* save swapped text even after use */
#ifndef	_POSIX_SOURCE
#define	RS_S_IREAD		0000400	/* read permission, owner */
#define	RS_S_IWRITE 	0000200	/* write permission, owner */
#define	RS_S_IEXEC		0000100	/* execute/search permission, owner */
#define	RS_S_ENFMT 	0002000	/* enforcement-mode locking */
#endif	/* !_POSIX_SOURCE */

#define	RS_S_IFMT		_RS_IFMT
#define	RS_S_IFDIR		_RS_IFDIR
#define	RS_S_IFCHR		_RS_IFCHR
#define	RS_S_IFBLK		_RS_IFBLK
#define	RS_S_IFREG		_RS_IFREG
#define	RS_S_IFLNK		_RS_IFLNK
#define	RS_S_IFSOCK	_RS_IFSOCK
#define	RS_S_IFIFO		_RS_IFIFO

#ifdef _WIN32
/* The Windows header files define _S_ forms of these, so we do too
   for easier portability.  */
#define RS__S_IFMT		_RS_IFMT
#define RS__S_IFDIR	_RS_IFDIR
#define RS__S_IFCHR	_RS_IFCHR
#define RS__S_IFIFO	_RS_IFIFO
#define RS__S_IFREG	_RS_IFREG
#define RS__S_IREAD	0000400
#define RS__S_IWRITE	0000200
#define RS__S_IEXEC	0000100
#endif

#define	RS_S_IRWXU 	(RS_S_IRUSR | RS_S_IWUSR | RS_S_IXUSR)
#define		RS_S_IRUSR	0000400	/* read permission, owner */
#define		RS_S_IWUSR	0000200	/* write permission, owner */
#define		RS_S_IXUSR 0000100/* execute/search permission, owner */
#define	RS_S_IRWXG		(RS_S_IRGRP | RS_S_IWGRP | RS_S_IXGRP)
#define		RS_S_IRGRP	0000040	/* read permission, group */
#define		RS_S_IWGRP	0000020	/* write permission, grougroup */
#define		RS_S_IXGRP 0000010/* execute/search permission, group */
#define	RS_S_IRWXO		(RS_S_IROTH | RS_S_IWOTH | RS_S_IXOTH)
#define		RS_S_IROTH	0000004	/* read permission, other */
#define		RS_S_IWOTH	0000002	/* write permission, other */
#define		RS_S_IXOTH 0000001/* execute/search permission, other */

#ifndef _POSIX_SOURCE
#define RS_ACCESSPERMS (RS_S_IRWXU | RS_S_IRWXG | RS_S_IRWXO) /* 0777 */
#define RS_ALLPERMS (RS_S_ISUID | RS_S_ISGID | RS_S_ISVTX | RS_S_IRWXU | RS_S_IRWXG | RS_S_IRWXO) /* 07777 */
#define RS_DEFFILEMODE (RS_S_IRUSR | RS_S_IWUSR | RS_S_IRGRP | RS_S_IWGRP | RS_S_IROTH | RS_S_IWOTH) /* 0666 */
#endif

#define	RS_S_ISBLK(m)	(((m)&_RS_IFMT) == _RS_IFBLK)
#define	RS_S_ISCHR(m)	(((m)&_RS_IFMT) == _RS_IFCHR)
#define	RS_S_ISDIR(m)	(((m)&_RS_IFMT) == _RS_IFDIR)
#define	RS_S_ISFIFO(m)	(((m)&_RS_IFMT) == _RS_IFIFO)
#define	RS_S_ISREG(m)	(((m)&_RS_IFMT) == _RS_IFREG)
#define	RS_S_ISLNK(m)	(((m)&_RS_IFMT) == _RS_IFLNK)
#define	RS_S_ISSOCK(m)	(((m)&_RS_IFMT) == _RS_IFSOCK)

#if defined(__CYGWIN__)
/* Special tv_nsec values for futimens(2) and utimensat(2). */
#define RS_UTIME_NOW	-2L
#define RS_UTIME_OMIT	-1L
#endif

///////////////////////////////////////////////
///////////////////////////////////////////////

#ifdef __RIGEL__

#define S_BLKSIZE RS_S_BLKSIZE

#define S_ISUID RS_S_ISUID
#define S_ISGID RS_S_ISGID
#define S_ISVTX RS_S_ISVTX
#ifndef _POSIX_SOURCE
#define S_IREAD RS_S_IREAD
#define S_IWRITE RS_S_IWRITE
#define S_EXEC RS_S_IEXEC
#define S_ENFMT RS_S_ENFMT
#endif  /* !_POSIX_SOURCE */

#define S_IFMT RS_S_IFMT
#define S_IFDIR RS_S_IFDIR
#define S_IFCHR RS_S_IFCHR
#define S_IFBLK RS_S_IFBLK  
#define S_IFREG RS_S_IFREG
#define S_IFLNK RS_S_IFLNK
#define S_IFSOCK RS_S_IFSOCK
#define S_IFIFO RS_S_IFIFO

#ifdef _WIN32
/* The Windows header files define _S_ forms of these, so we do too
   for easier portability.  */
#define _S_IFMT RS__S_IFMT
#define _S_IFDIR RS__S_IFDIR
#define _S_IFCHR RS__S_IFCHR
#define _S_IFIFO RS__S_IFIFO
#define _S_IFREG RS__S_IFREG
#define _S_IREAD RS__S_IREAD
#define _S_IWRITE RS__S_IWRITE
#define _S_IEXEC RS__S_IEXEC
#endif

#define S_IRWXU RS_S_IRWXU
#define S_IRUSR RS_S_IRUSR
#define S_IWUSR RS_S_IWUSR
#define S_IXUSR RS_S_IXUSR
#define S_IRWXG RS_S_IRWXG
#define S_IRGRP RS_S_IRGRP
#define S_IWGRP RS_S_IWGRP
#define S_IXGRP RS_S_IXGRP
#define S_IRWXO RS_S_IRWXO
#define S_IROTH RS_S_IROTH
#define S_IWOTH RS_S_IWOTH
#define S_IXOTH RS_S_IXOTH

#ifndef _POSIX_SOURCE
#define ACCESSPERMS RS_ACCESSPERMS
#define ALLPERMS RS_ALLPERMS
#define DEFFILEMODE RS_DEFFILEMODE
#endif

#define S_ISBLK(m) RS_S_ISBLK(m)
#define S_ISCHR(m) RS_S_ISCHR(m)
#define S_ISDIR(m) RS_S_ISDIR(m)
#define S_ISFIFO(m) RS_S_ISFIFO(m)
#define S_ISREG(m) RS_S_ISREG(m)
#define S_ISLNK(m) RS_S_ISLNK(m)
#define S_ISSOCK(m) RS_S_ISSOCK(m)

#if defined(__CYGWIN__)
/* Special tv_nsec values for futimens(2) and utimensat(2). */
#define UTIME_NOW RS_UTIME_NOW
#define UTIME_OMIT RS_UTIME_OMIT
#endif

int	_EXFUN(chmod,( const char *__path, mode_t __mode ));
int     _EXFUN(fchmod,(int __fd, mode_t __mode));
int	_EXFUN(fstat,( int __fd, struct stat *__sbuf ));
int	_EXFUN(mkdir,( const char *_path, mode_t __mode ));
int	_EXFUN(mkfifo,( const char *__path, mode_t __mode ));
int	_EXFUN(stat,( const char *__path, struct stat *__sbuf ));
mode_t	_EXFUN(umask,( mode_t __mask ));

#if defined (__SPU__) || defined(__rtems__) || defined(__CYGWIN__) && !defined(__INSIDE_CYGWIN__)
int	_EXFUN(lstat,( const char *__path, struct stat *__buf ));
int	_EXFUN(mknod,( const char *__path, mode_t __mode, dev_t __dev ));
#endif

#if defined (__CYGWIN__) && !defined(__INSIDE_CYGWIN__)
int	_EXFUN(fchmodat, (int, const char *, mode_t, int));
int	_EXFUN(fstatat, (int, const char *, struct stat *, int));
int	_EXFUN(mkdirat, (int, const char *, mode_t));
int	_EXFUN(mkfifoat, (int, const char *, mode_t));
int	_EXFUN(mknodat, (int, const char *, mode_t, dev_t));
int	_EXFUN(utimensat, (int, const char *, const struct timespec *, int));
int	_EXFUN(futimens, (int, const struct timespec *));
#endif

/* Provide prototypes for most of the _<systemcall> names that are
   provided in newlib for some compilers.  */
#ifdef _COMPILING_NEWLIB
int	_EXFUN(_fstat,( int __fd, struct stat *__sbuf ));
int	_EXFUN(_stat,( const char *__path, struct stat *__sbuf ));
#ifdef __LARGE64_FILES
struct stat64;
int	_EXFUN(_fstat64,( int __fd, struct stat64 *__sbuf ));
#endif
#endif

#endif /* #ifdef __RIGEL__ */

#endif /* !_STAT_H_ */
#ifdef __cplusplus
}
#endif
#endif /* __RIGEL_SYS_STAT_H__ */
