/*	vtimes.h	4.2	81/02/19	*/
/*
 * Structure returned by vtimes() and in vwait().
 * In vtimes() two of these are returned, one for the process itself
 * and one for all its children.  In vwait() these are combined
 * by adding componentwise (except for maxrss, which is max'ed).
 */
struct vtimes {
	int	vm_utime;		/* user time (60'ths) */
	int	vm_stime;		/* system time (60'ths) */
	/* divide next two by utime+stime to get averages */
	unsigned vm_idsrss;		/* integral of d+s rss */
	unsigned vm_ixrss;		/* integral of text rss */
	int	vm_maxrss;		/* maximum rss */
	int	vm_majflt;		/* major page faults */
	int	vm_minflt;		/* minor page faults */
	int	vm_nswap;		/* number of swaps */
	int	vm_inblk;		/* block reads */
	int	vm_oublk;		/* block writes */
};

#ifdef KERNEL
struct vtimes zvms;		/* an empty (componentwise 0) structure */
#endif
