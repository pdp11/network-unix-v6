#define	NSIG	20	/* # of Bell signals */
#define	NHSIG	1	/* # of Harvard signals */
#define		SIGHUP	1	/* hangup */
#define		SIGINT	2	/* interrupt (^C) */
#define		SIGQIT	3	/* quit (^B) */
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
#define		SIGALA 14	/* alarm clock */

/* harvard signals */

#define		SIGBRK (-1)	/* break (break) */
