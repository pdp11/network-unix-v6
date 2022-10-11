/*
 */

/*
 *  Parameters dependent upon desired pieces
 */

#define SMALL   0               /* BBN: mek (5/3/79 ) - create a small UNIX.
				   leave out code in: proc_util.c,
						      sys4.c,      sysent.c */

/* #define CPU70   0    */           /* define for 11/70 (used by tu16
					tape driver and bio.c */


/* #define NETWORK 0  *//* include NETWORK code used by both NCP and RMI */
			/* SEE netparam.h for NETWORK specific defines */
#ifdef NETWORK
#define GATEWAY 0               /* include GATEWAY code */
#define NCP     0               /* include NCP specific code */
#define RMI     0               /* include RMI specific code */
#endif NETWORK

/* #define SMDATE  0 */         /* dont include smdate trap */
/* #define LCBA 1    */         /* don't include LCBA code */

/* #define BUFMOD  0    */      /* introduce non-kernel space (NKS)
				 * system buffers. These are currently
				 * used only by the network.
				 */

#define NDC11   0               /* DC-11 handler parameter */
#define NKL11   5               /* KL-11 handler parameters */
#define NDL11   1
#define CAP     0		/* 0 => 96-char printer */
#define PASYNC  0               /* asynchronous physio (buf.h, bio.c/iodone
							bio.c/physio only) */
#define NBUF   10               /* size of buffer cache */
#define NINODE 130              /* number of incore inodes */
#define NFILE  90               /* number of incore file structures */
#define NMOUNT  2               /* number of mountable file systems */
#define SSIZE  20		/* initial stack size (*64 bytes) */
#define SINCR  20		/* increment of stack (*64 bytes) */
#define NOFILE 60               /* max open files per process */
#define NPROC  28               /* max number of processes */
#define NCLIST 120              /* max total clist size */
#define MAXSYSC 76              /* number of entries in sysent - 1 */

/*
 * tunable variables
 */

#define NSWBUF	2		/* number of buffer headers for swapping */
#define	NEXEC	3		/* number of simultaneous exec's */
#define	MAXMEM	(64*32)		/* max core per process - first # is Kw */
#define CANBSIZ 120             /* max size of typewriter line */
#define CMAPSIZ 264     /**/    /* size of core allocation area */
#define SMAPSIZ 264     /**/    /* size of swap allocation area */
#define	NCALL	20		/* max simultaneous time callouts */
#define NTEXT   30      /**/    /* max number of pure texts */
#define	HZ	60		/* Ticks/second of the clock */

/*
 * priorities
 * probably should not be
 * altered too much
 */

#define	PSWP	-100
#define	PINOD	-90
#define	PRIBIO	-50
#define	PPIPE	1
#define	PWAIT	40
#define	PSLEP	90
#define	PUSER	100

/*
 * signals
 * dont change
 */

#define	NSIG	20
#define		SIGHUP	1	/* hangup */
#define		SIGINT	2	/* interrupt (rubout) */
#define		SIGQIT	3	/* quit (FS) */
#define		SIGINS	4	/* illegal instruction */
#define		SIGTRC	5	/* trace or breakpoint */
#define		SIGIOT	6	/* iot */
#define		SIGEMT	7	/* emt */
#define		SIGFPT	8	/* floating exception */
#define		SIGKIL	9	/* kill */
#define		SIGBUS	10	/* bus error */
#define		SIGSEG	11	/* segmentation violation */
#define		SIGSYS	12	/* sys */
#define		SIGPIPE	13	/* end of pipe */
#define		SIGCLK	14	/* alarm clock */
#ifdef NETWORK
#define		SIGINR	15	/* Network Interrupt Receiver sig */
#endif

/*
 * fundamental constants
 * cannot be changed
 */

#define	USIZE	16		/* size of user block (*64) */
#define	NULL	0
#define	NODEV	(-1)
#define	ROOTINO	1		/* i number of all roots */
#define	DIRSIZ	14		/* max characters per directory */

/*
 * structure to access an
 * integer in bytes
 */
struct
{
	char	lobyte;
	char	hibyte;
};

struct {        /* structure for taking a long apart into ints */
	int     hiword;
	int     loword;
};
/*
 * structure to access an integer
 */
struct
{
	int	integ;
};

/*
 * Certain processor registers
 */
#define PS	0177776
#define KL	0177560
#define SW	0177570

