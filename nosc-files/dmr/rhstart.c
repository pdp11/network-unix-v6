#include "param.h"
#include "buf.h"
#include "systm.h"

/*
 * startup routine for RH controllers.
 */
#define IENABLE 0100
#define	RHWCOM	060
#define	RHRCOM	070
#define GO	01

rhstart(bp, devloc, devblk, abae)
struct buf *bp;
int *devloc, *abae;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
#ifndef CPU40		/* if it's not a /40 */
#ifndef CPU45		/* or a /45.... */
#ifndef CPU70		/* or a /70, include the test */
	if(cputype == 70)
#endif not CPU70
		*abae = rbp->b_xmem;
#endif not CPU45
#endif not CPU40
	*dp = devblk;			/* block address */
	*--dp = rbp->b_addr;		/* buffer address */
	*--dp = rbp->b_wcount;		/* word count */
	com = IENABLE | GO |
		((rbp->b_xmem & 03) << 8);
	if (rbp->b_flags&B_READ)	/* command + x-mem */
		com =| RHRCOM; else
		com =| RHWCOM;
	*--dp = com;
}
