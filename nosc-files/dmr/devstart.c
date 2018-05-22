#include "param.h"
#include "buf.h"

/*
 * Device start routine for disks
 * and other devices that have the register
 * layout of the older DEC controllers (RF, RK, RP, TM)
 */
#define	IENABLE	0100
#define	WCOM	02
#define	RCOM	04
#define	GO	01
devstart(bp, devloc, devblk, hbcom)
struct buf *bp;
int *devloc;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
	*dp = devblk;			/* block address */
	*--dp = rbp->b_addr;		/* buffer address */
	*--dp = rbp->b_wcount;		/* word count */
	com = (hbcom<<8) | IENABLE | GO |
		((rbp->b_xmem & 03) << 4);
	if (rbp->b_flags&B_READ)	/* command + x-mem */
		com =| RCOM;
	else
		com =| WCOM;
	*--dp = com;
}
