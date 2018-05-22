/*
 * The user structure.
 * One allocated per process.
 * Contains all per process data
 * that doesn't need to be referenced
 * while the process is swapped.
 * The user block is USIZE*64 bytes
 * long; resides at virtual kernel
 * loc 140000; contains the system
 * stack per user; is cross referenced
 * with the proc structure for the
 * same process.
 */
/* N.B. : This user struct is modified from standard unix for accounting stuff
          used at Illinois. The error numbers are appended to for Illinois 
          assignment of some error codes, particularly for network related errs.*/
struct user
{
	int	u_rsav[2];		/* save r5,r6 when exchanging stacks */
	int	u_fsav[25];		/* save fp registers */
					/* rsav and fsav must be first in structure */
	char	u_segflg;		/* flag for IO; user or kernel space */
	char	u_error;		/* return error code */
	char	u_uid;			/* effective user id */
	char	u_gid;			/* effective group id */
	char	u_ruid;			/* real user id */
	char	u_rgid;			/* real group id */
	int	u_procp;		/* pointer to proc structure */
	char	*u_base;		/* base address for IO */
	char	*u_count;		/* bytes remaining for IO */
	char	*u_offset[2];		/* offset in file for IO */
	int	*u_cdir;		/* pointer to inode of current directory */
	char	u_dbuf[DIRSIZ];		/* current pathname component */
	char	*u_dirp;		/* current pointer to inode */
	struct	{			/* current directory entry */
		int	u_ino;
		char	u_name[DIRSIZ];
	} u_dent;
	int	*u_pdir;		/* inode of parent directory of dirp */
	int	u_uisa[16];		/* prototype of segmentation addresses */
	int	u_uisd[16];		/* prototype of segmentation descriptors */
	int	u_ofile[NOFILE];	/* pointers to file structures of open files */
	int	u_arg[5];		/* arguments to current system call */
	int	u_tsize;		/* text size (*64) */
	int	u_dsize;		/* data size (*64) */
	int	u_ssize;		/* stack size (*64) */
	int	u_sep;			/* flag for I and D separation */
	int	u_qsav[2];		/* label variable for quits and interrupts */
	int	u_ssav[2];		/* label variable for swapping */
	int	u_signal[NSIG];		/* disposition of signals */
#ifndef DPTIME
	int	u_utime;		/* this process user time */
	int	u_stime;		/* this process system time */
#endif
#ifdef DPTIME
	int	u_utime[2];		/* this process user time JSK */
	int	u_stime[2];		/* this process system time JSK */
#endif
	int	u_cutime[2];		/* sum of childs' utimes */
	int	u_cstime[2];		/* sum of childs' stimes */
	int	u_syscalls;		/* hi count of system calls JSK */
	int	*u_ar0;			/* address of users saved R0 */
	int	u_prof[4];		/* profile arguments */
	char	u_intflg;		/* catch intr from sys */
	char	u_hsyscalls;		/* hi part of system calls JSK */
	int	u_ttyp;			/* controlling tty pointer */
	int	u_ttyd;			/* controlling tty dev */
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
};
#ifndef NO_U	/* Added--RJB (CAC) 1977 April 30 */
struct user u;
#endif

/* u_error codes */
/*  defined value:              Note in errlst.c: */
#define EPERM	1		/*"Not super-user"*/
#define ENOENT  2		/*"No such file or directory"*/
#define ESRCH	3		/*"No such process"*/
#define EINTR	4		/*"Interrupted system call"*/
#define EIO	5		/*"I/O error"*/
#define ENXIO	6		/*"No such device or address"*/
#define E2BIG	7		/*"Arg list too long"*/
#define ENOEXEC 8		/*"Exec format error"*/
#define EBADF	9		/*"Bad file number"*/
#define ECHILD  10		/*"No children"*/
#define EAGAIN  11		/*"No more processes"*/
#define ENOMEM  12		/*"Not enough core"*/
#define EACCES  13		/*"Permission denied"*/
				/*"Error 14" not defined*/
#define ENOTBLK 15		/*"Block device required"*/
#define EBUSY	16		/*"Mount device busy"*/
#define EEXIST  17		/*"File exists"*/
#define EXDEV	18		/*"Cross-device link"*/
#define ENODEV  19		/*"No such device"*/
#define ENOTDIR 20		/*"Not a directory"*/
#define EISDIR  21		/*"Is a directory"*/
#define EINVAL  22		/*"Invalid argument"*/
#define ENFILE  23		/*"File table overflow"*/
#define EMFILE  24		/*"Too many open files"*/
#define ENOTTY  25		/*"Not a typewriter"*/
#define ETXTBSY 26		/*"Text file busy"*/
#define EFBIG	27		/*"File too large"*/
#define ENOSPC  28		/*"No space left on device"*/
#define ESPIPE  29		/*"Illegal seek"*/
#define EROFS	30		/*"Read-only file system"*/
#define EMLINK  31		/*"Too many links"*/
#define EPIPE	32		/*"Broken Pipe"*/
				/* error 33 undefined			*/
#define ETOLONG 34		   /*"??ETOLONG??"*/
#define EDELAY	35		/* RJB (CAC) Dec 7 '77  ll tm driver error */
				/* needed on code imported from linc.labs */
				/*error 36 not used yet*/
#define	EACCT	37		/*??EACCT?? Illinois accounting */
				/*error 38 not used yet*/
				/*error 39 not used yet*/

/* here begins the error codes defined for network functions		*/
#define ENOTNET 40		/*"Not a network file"*/ /*Illinois*/
#define ENCP2   41		/*"NCP already opened"*/ /*Illinois*/
#define ENCPIO  42		/*"NCP IO error"*/   /*Illinois*/
#define ENCPINV 43		/*"NCP Invalid parameter"*/ /*Illinois*/
#define	ENCPNO	44		/* NCP not opened	*/	/* Illinois */
#define	ESNET	45		/*"Seek on net file"*/	/* Illinois */
#define	EMNETF	46		/* too many net files open */	/* Illinois */
#define	ENCPBUF	47		/* NCP out of buffers	*/
#define EDDWN	48		/* NCP daemon was killed */
#define EDAEIO	49		/* NCP daemon I/O error */
#define EDINV	50		/* invalid argument to daemon */
#define EDNORES	51		/* no resource in daemon */
#define	ENCPKNI	52		/* NCP Kernel not in system	*/
/* end of the net-specific error codes					*/


#define	EFAULT	106
