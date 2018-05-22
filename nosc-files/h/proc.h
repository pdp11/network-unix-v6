/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct	proc
{
int dummy;/* DEBUG so old-style routines can look at this struct */
	char	p_stat;
	char	p_flag;
	char	p_pri;		/* priority, negative is high */
	char	p_sig;		/* signal number sent to this process */
	char	p_uid;		/* user id, used to direct tty signals */
	char	p_time;		/* resident time for scheduling */
	char	p_cpu;		/* cpu usage for scheduling */
	char	p_nice;		/* nice for cpu usage */
	int	p_pgrp;		/* name of process group leader */
	int	p_pid;		/* unique process id */
	int	p_ppid;		/* process id of parent */
	int	p_addr;		/* address of swappable image */
	int	p_size;		/* size of swappable image (*64 bytes) */
	int	p_wchan;	/* event process is awaiting */
	char	*p_textp;	/* pointer to text structure */
	int	p_clktim;	/* time to alarm clock signal */
};
#ifdef NPROC
struct proc proc[NPROC];
#endif NPROC

/* p_stat codes */
#define	SSLEEP	1		/* awaiting an event */
#define	SWAIT	2		/* (abandoned state) */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* p_flag codes */
#define	SLOAD	0001		/* in core */
#define	SSYS	0002		/* scheduling process */
#define	SLOCK	0004		/* process cannot be swapped */
#define	SSWAP	0010		/* process is being swapped out */
#define	STRC	0020		/* process is being traced */
#define	SWTED	0040		/* another tracing flag */
#define SNOSIG  0100            /* rand:bobg--process isn't to get quit or
				   interrupts from controlling tty */
