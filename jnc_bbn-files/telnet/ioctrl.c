#
/*
 * IO control routines -- enter, delete, and run IO handlers
 */
/* Max number of simultaneous open files in a process */
#define NOFILE 40
/* Max number of fd-driven jobs */
#define MAXJOBS 16
/* Max number of other jobs */
#define MAXOTHERS 10

#define EINTR 4      /* Bad - luck error */
#define EBADDEV 62    /* Await not implemented */

/*
 * The job array is indexed by file descriptor.
 * A value of 0 means that no job has been assigned.
 * Note that these routines depend on j_rjobs and j_wjobs being 0 initially.
 */
struct jobs {
    int (*j_rjob)();
    int (*j_wjob)();
    int j_arg;
};
/*
 * The structure for the capacity call. There is an array of these,
 * one element per fd for which it is desired to take the capacity.
 * The fds are then used to index into the jobs array.
 */
struct capacities {
    int fdcapac;
    int c_read;
    int c_write;
};
/*
 * The structure for the non-capacity-controlled routines.
 * These routines operate entirely independently.
 */
struct others {
    int (*o_job)();
    int o_arg;
};

/* -------------------------- I O E N T E R ------------------------- */
/*
 * IoEnter(fd, rfunc, wfunc, arg) enters a file descriptor and its
 * read and write handlers into the "io queue". When there is activity
 * on the file descriptor, the handler is called with three arguments:
 * the file descriptor, the capacity, and the arg supplied in the enter
 * call. If the handler is 0, it is not called.
 * The handler must return 0 if it did work, or a nonzero value
 * if there was no work for it to do.
 */
IoEnter(fd, rfunc, wfunc, arg)
    int fd;
    int (*rfunc)();
    int (*wfunc)();
    int arg;
{
    extern int errno;
    struct jobs *jp;
    extern struct jobs jobs[];
    extern struct capacities capacities[];
    extern int njobs;
    extern int restart;

    if (fd < 0 || fd >= NOFILE)
        fatal(0, "IoEnter: bad fd %d\n", fd);
    jp = &jobs[fd];
    if (njobs >= MAXJOBS)
        fatal(0, "Unable to enter fd %d", fd);
    if (awtenb(fd) == -1)
        if (errno != EBADDEV)
            fatal(errno, "await-enabling %d", fd);
    capacities[njobs++].fdcapac = fd;
    jp->j_rjob = rfunc;
    jp->j_wjob = wfunc;
    jp->j_arg = arg;
    restart = 1;
    return;
}

/* -------------------------- I O X E N T E R ----------------------- */
/*
 * IoxEnter(funcp, arg) is similar to IoEnter but is designed for routines
 * for which taking the capacity of a fd is unnecessary or meaningless.
 * The routine will be called as follows:
 *     active_sw = (*funcp)(arg);
 * active_sw and arg are as for routines called by IoEnter.
 *
 * Note that routines supplied to IoxEnter will always be called whenever
 * there is activity on any other routine.
 */
IoxEnter(fp, arg)
    int (*fp)();
    int arg;
{
    extern int nothers;
    extern struct others others[];

    if (nothers >= MAXOTHERS)
        fatal(0, "Unable to enter Iox job");
    others[nothers].o_job = fp;
    others[nothers].o_arg = arg;
    nothers++;
}

/* -------------------------- I O D E L E T E ----------------------- */
/*
 * IoDelete(fd) deletes a file descriptor and its handler from the queue.
 * Note that, because the capacities structure is fed directly to the
 * capac system call, the structure must be kept "clean" (active entries
 * after the deleted one are moved up).
 */
IoDelete(fd)
    int fd;
{
    register int i;
    extern int errno;
    struct jobs *jp;
    extern struct jobs jobs[];
    extern struct capacities capacities[];
    extern int restart;
    extern int njobs;

    for (i = 0; i < njobs; i++)
        if (capacities[i].fdcapac == fd)
        {
            awtdis(fd);
            capacities[i].fdcapac = -1;
            for (; i < njobs-1; i++)
                capacities[i].fdcapac = capacities[i+1].fdcapac;
            njobs--;
            jp = &jobs[fd];
            jp->j_wjob = 0;
            jp->j_rjob = 0;
            jp->j_arg = 0;    /* Not that it matters... */
            restart = 1;
            return;
        }

    com_err(0, "IoDelete: cannot find %d\n", fd);
    return;
}

/* -------------------------- I O X D E L E T E --------------------- */
/*
 * IoxDelete(funcp, arg) is similar to IoDelete but is for routines entered
 * via IoxEnter.
 */
IoxDelete(fp, arg)
    int (*fp)();
    int arg;
{
    register int i;
    extern int nothers;
    extern struct others others[];

    for (i = 0; i < nothers; i++)
    {
	if ((others[i].o_job == fp) && (others[i].o_arg == arg))
	{
	    others[i].o_job = 0;
	    others[i].o_arg = 0;
	    for (nothers--; i < nothers; i++)
	    {
		others[i].o_job = others[i+1].o_job;
		others[i].o_arg = others[i+1].o_arg;
	    }
	    return;
	}
    }
    com_err(0, "IoxDelete: cannot find 0%o 0%o\n", fp, arg);
    return;
}

/* -------------------------- I O R U N --------------------------- */
/*
 * IoRun(awttim) is called to actually do the work. It does not return;
 * it continually does capacity calls and calls the appropriate
 * routines. When there is no work to do, it awaits for awttim seconds.
 */
IoRun(awttim)
    int awttim;
{
    int fd;
    int (*f)();
    int inactive, i;
    struct capacities *cp;
    struct jobs *jp;
    struct others *op;
    extern int errno;
    extern struct jobs jobs[];
    extern struct capacities capacities[];
    extern int njobs;
    extern int restart;
    extern struct others others[];
    extern int nothers;

    for (;;)
    {
	inactive = 1;
RETRY:
	restart = 0;
	errno = 0;

	if (capac(-1, capacities, njobs * sizeof(capacities[0])) == -1)
	    if (errno == EINTR)    /* Just a signal */
		goto RETRY;
	    else
		fatal(errno, "Getting capacities");

	for (cp = &capacities[0]; cp < &capacities[njobs]; cp++)
	{
	    fd = cp->fdcapac;
	    if (fd < 0) /* Replaced by negative of error code */
		fatal(-fd, "Getting a capacity, cannot continue");
	    jp = &jobs[fd];
	    if ((f = jp->j_wjob) && cp->c_write != 0)
	    {
		i = (*f)(fd, cp->c_write, jp->j_arg);
		if (inactive) inactive = i;
		if (restart) goto RETRY;     /* Func called IoEnter or IoDelete */
	    }
	    if ((f = jp->j_rjob) && cp->c_read != 0)
	    {
		i = (*f)(fd, cp->c_read, jp->j_arg);
		if (inactive) inactive = i;
		if (restart) goto RETRY;
	    }
	}

	for (op = &others[0]; op < &others[nothers]; op++)
	{
	    i = (*(op->o_job))(op->o_arg);
	    if (inactive) inactive = i;
	}

	if (inactive)
	{
	    if (await(awttim, 0, 0) == -1)
		if (errno != EINTR)	 /* Signal during await just takes us out */
		    fatal(errno, "in await");/* Any other problem and we die */
	}
    }
}

/* -------------------------- G L O B A L S ------------------------- */

struct jobs jobs[NOFILE];
struct capacities capacities[MAXJOBS];
int njobs 0;        /* Number of active entries in capacities array */
int restart 0;/* Set when IoEnter or IoDelete called -- makes IoRun retry */

struct others others[MAXOTHERS];
int nothers 0;     /* Number of active entries in others array */
