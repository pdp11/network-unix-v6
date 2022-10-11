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
	char	*qlink;		/* link for queueing processes */
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
	int	*p_textp;	/* pointer to text structure */
	int	p_clktim;	/* time to alarm clock signal */
	/* next two entries support itime sys call (BBN:JFH 5/78) */
	int     *p_itima;       /* user address for itime counter */
	int     p_itime[2];     /* for saving time when process is stopped */
} proc[NPROC];

/* structures for queueing them */
struct
{
	char *qlink;
};

struct procq
{
	int  sem_count; /* semaphore count for list access */
	char *sem_q;    /* queue of processes waiting for this list */
	char *qhead;	/* head for the list itself */
};

struct procq Free_proc; /* list of free process entries */
struct procq Run_proc;	/* list of runable processes */
struct procq Swap_ready;/* queue of processes to be swapped in */

struct semaphore
{
	int     sem_count;      /* number of people waiting if any */
	char	*sem_q;		/* place to let people wait */
};

/* stat codes */
#define	SSLEEP	1		/* awaiting an event */
#define	SWAIT	2		/* (abandoned state) */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* flag codes */
#define	SLOAD	01		/* in core */
#define	SSYS	02		/* scheduling process */
#define	SLOCK	04		/* process cannot be swapped */
#define	SSWAP	010		/* process is being swapped out */
#define	STRC	020		/* process is being traced */
#define	SWTED	040		/* another tracing flag */
#define SNOSIG  0100            /* rand:bobg--process isn't to get quit or
				   interrupts from controlling tty */

struct proc *MAXPROC;		/* addr of highest process used */
