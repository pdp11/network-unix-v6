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
	int     u_utime[2];             /* this process user time */
	int     u_stime[2];             /* this process system time */
	int     u_cutime[2];            /* sum of children's utimes */
	int     u_cstime[2];            /* sum of children's stimes */
	int     u_tim1[2];              /* beginning time of process */
	int     u_tio[2];               /* tty I/O count */
	int     u_mem1[2];              /* cumulative memory*time usage */
	int     u_mem2[2];              /* same but for shared text */
	int     u_io[2];                /* disk I/O count */
	int	*u_ar0;			/* address of users saved R0 */
	int	u_prof[4];		/* profile arguments */
	char	u_intflg;		/* catch intr from sys */
	int	u_ttyp;			/* controlling tty pointer */
	int	u_ttyd;			/* controlling tty dev */
#ifdef  LCBA
	/*
	 * Address and descriptor software registers for mapped pages:
	 * addresses are stored relative to lcba_address.
	 */
	int     u_lcbma[16];
	int     u_lcbmd[16];
	/*
	 * Data for use by estabur to keep grow and break from extending
	 * data or stack segments into a mapped page; the page numbers in
	 * them start at 1 so zero means no mapped pages.
	 */
	char    u_maxlcb;               /* maximum data page */
	char    u_minlcb;               /* minimum data page */
	char    u_mintlcb;              /* minimum text page on sep */
	char    u_lcbflg;               /* this process has siezed lcba */
	char    u_clmin;                /* cache limits for use by */
	char    u_clmax;                /* f4p interface */
#endif
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
} u;

/* u_error codes */
#define	EFAULT	106
#define	EPERM	1
#define	ENOENT	2
#define	ESRCH	3
#define	EINTR	4
#define	EIO	5
#define	ENXIO	6
#define	E2BIG	7
#define	ENOEXEC	8
#define	EBADF	9
#define	ECHILD	10
#define	EAGAIN	11
#define	ENOMEM	12
#define	EACCES	13
#define	ENOTBLK	15
#define	EBUSY	16
#define	EEXIST	17
#define	EXDEV	18
#define	ENODEV	19
#define	ENOTDIR	20
#define	EISDIR	21
#define	EINVAL	22
#define	ENFILE	23
#define	EMFILE	24
#define	ENOTTY	25
#define	ETXTBSY	26
#define	EFBIG	27
#define	ENOSPC	28
#define	ESPIPE	29
#define	EROFS	30
#define	EMLINK	31
#define	EPIPE	32
#define ESNET	33
#ifdef NETWORK
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
#define EDTIMO  53              /* daemon timeout */
#define EDHDED  54              /* destination host dead */
/* end of the net-specific error codes					*/
#endif
/* Await and capacity error codes */
#define EBADCLASS 60		/* bad 'class' code of trap */
#define EAWTMAX  61             /* too many processes awaiting */
#define EBADDEV 62		/* bad device (not supported) */
#ifdef RMI
/* The rawmessage facility error codes */
#define ERAWINV     65  /* invalid parameters on open */
#define ERAWINO     66  /* no inodes available */
#define ERAWMDUP    67  /* hostmessagerange intersects existing one */
#define ERAWTAB     68  /* no space in raw connection table */
#define ERAWMLEN    69  /* message length < 2 or > (MAXMSG [+ RAWPAD]) */
#endif
