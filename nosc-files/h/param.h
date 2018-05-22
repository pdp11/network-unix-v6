/*
 * Conditional compile switches
 */

#define DNTTY		/* Compiling DN TTY code for baudot support */
#define CPU40		/* CPU must be 40, 45, or 70 */
/* We don't have floating point....
#define FLOATPT		/* floating point unit present on system */

#define NOSC		/* for local mods that reduce space */
#define NETSYS		/* Include NCP resident code in kernel */
#define CONSIZE 16	/* only 16 connections permitted */
/*#define VDHSYS	$ /*do this if you want a Very Distant Host kernel*/
/* Network interface is IMP11A.....
#define VDHC		/* our VDH is a VDH11-C */
/* use asynchronous tty echoing
#define	CANBSIZ	256	/* synchronous echoing (max size of tty line) */
/* no parallel swapping for the time being ....
#define NSWBUF	2	/* number of buffer headers for swapping */
#define	MSGBUFS	30	/* Characters saved from error messages */
#define DPTIME		/* Double precision time keeping */
#define NDH11	16	/* we support 16 DH lines */
#define NPTY	2	/* we support 2 psuedo-ttys */
/* IMP11A mods not ready yet....
#define UCBUFMOD	/* include the U.Calgary buffer modifications */
/* no high-resolution timing needed....
#define FASTIME		/* maintain line-frequency timer */
/*#define RKDUMP 4000	/* use RK0 swap area for dump device */
/*#define TUDUMP 0	/* use TU10 drive 0 for dump device */
/*#define NODUMP    	/* don't include code to dump core */
/* no accounting for the time being....
#define ACCTSYS		/* Include U.Illinois accounting */

/*
 * tunable variables
 */

#define	NBUF	8		/* size of buffer cache */
#define	NINODE	60		/* number of in core inodes */
#define	NFILE	70		/* number of in core file structures */
#define	NMOUNT	2		/* number of mountable file systems */
#define	NEXEC	1		/* number of simultaneous exec's */
#define	MAXMEM	(64*32)		/* max core per process - first # is Kw */
#define	SSIZE	20		/* initial stack size (*64 bytes)--unused */
#define	SINCR	2		/* increment of stack (*64 bytes) */
#define	NOFILE	15		/* max open files per process */
#define	CMAPSIZ	100		/* size of core allocation area--fn of NPROC */
#define	SMAPSIZ	100		/* size of swap allocation area--fn of NPROC */
#define	NCALL	20		/* max simultaneous time callouts */
#define	NPROC	35		/* max number of processes */
#define	NTEXT	20		/* max number of pure texts */
#define	NCLIST	100		/* max total clist size */
#define	HZ	60		/* Ticks/second of the clock */
#define NICEFENCE 30 /* temp until I find where it belongs */

/* actually the next define should be in user.h, but since we don't have
 * recursive includes, we can't expect the ACCSYS definition before the
 * if test ...
 */
#ifdef ACCTSYS
#define DPTIME	/* accounting expects a double precision time in the u */
#endif

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
#define		SIGINR	15	/* Network Interrupt Receiver sig */
#define		SIGSPA	16	/* SPare signal A (unnamed)	*/
#define		SIGSPB	17	/* SPare signal B (unnamed)	*/
#define		SIGSPC	18	/* SPare signal C (unnamed)	*/
#define		SIGSPD	19	/* SPare signal D (unnamed)	*/

/*
 * fundamental constants
 * cannot be changed
 */

#define	USIZE	16		/* size of user block (*64) */
#define	NULL	0
#define	NODEV	(-1)
#define	ROOTINO	1		/* i number of all roots */
#define	DIRSIZ	14		/* max characters per directory */

#ifndef ASSEMBLY	/* don't include this stuff in assembly modules */
/*
 * structure to access an
 * integer in bytes
 */
struct
{
	char	lobyte;
	char	hibyte;
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

/*
 * Waiting for version 7
 */
#define UNSIGN char *

#endif

