#
/*
 * System Industries (SI) controller driver for CDC 9760/9762 disk.
 *
 */
#include "param.h"
#include "buf.h"
#include "conf.h"
#include "user.h"

#define	logical	char *		/* don't say logical i,j; !!! */

/*
 * Use 2 cells in struct buf for physical disk address pieces.
 */
#define	hdsec	av_back		/* head & sector */
#define	pcyl	b_resid		/* port & cylinder */

#define	SIADDR	0176700

struct siregs		/* device registers */
{	int sicnr;		/* control */
	int siwcr;		/* word count (not 2's complement!) */
	int sipcr;		/* port/cylinder */
	int sihsr;		/* head/sector */
	int simar;		/* memory address */
	int sierr;		/* errors & status */
/* unused for now...
	int sissr;
	int sisar;
	int sidbr;
	int sictr;
	int siscr;
 */
};
/*
 * sicnr layout
 */
#define	POFFSET	040000		/* offset+  (toward spindle) */
#define	MOFFSET	020000		/* offset- */
#define	ESTROBE	010000		/* strobe early */
#define	LSTROBE	004000		/* strobe late */
#define	FENABLE	001000		/* format enable */
#define	MARINH	000400		/* inhibit simar increment */
#define	RDY	000200		/* controller ready */
#define	IENABLE	000100		/* interrupt enable */
#define	MAREXT	000060		/* simar extension bits */
#define	VERIFY	000010		/* read or write verify function */
#define	READ	000004		/* read enable */
#define	WRITE	000002		/* write enable */
#define	GO	000001		/* initiate indicated function */
#define	RESET	0		/* clear simar & controller */

#ifdef SI9762
#define HEADS	19		/* # of heads (or surfaces) 9762 */
#define NSI	16		/* # of minor device slots per drive */
#endif 9762
#ifndef SI9762
#define HEADS	5		/* # of heads for 9760 */
#define	NSI	8		/* # of minor device slots per drive */
#endif 9760

#define	TRKSIZ	32		/* # of 512 byte sectors/track */
#define	CYLSIZ	(HEADS*TRKSIZ)

#define	RKSIZ	4872		/* # of sectors on a DEC RK05 drive */
#define	RKTRKS	153		/* # of tracks to simulate RK05 */

#define	MAXSIZ	0		/* 65,536 blocks on largest UNIX device */
#define	MAXTRKS	2048		/* # of tracks for largest UNIX device */

/*
 * Disk is organized to place tmp, swap, root, and user file systems
 * close together and near the center of the drive.
 */

struct siminor		/* layout of minor devices for each drive */
{	int trkoff;		/* starting track # (not cylinder!) */
	logical nblocks;	/* # of sectors in minor device */
} sisizes[NSI] {
#ifdef SI9762		/* disk layout for 300 Mb disk */
		/* this mapping is not correct, but it will do
		   as a filler untill a better one is defined.
		*/
	0*MAXTRKS+0*RKTRKS,	MAXSIZ,		/* si0 */
	1*MAXTRKS+0*RKTRKS,	MAXSIZ,		/* si1 */
	2*MAXTRKS+0*RKTRKS,	MAXSIZ,		/* si2 */
	3*MAXTRKS+0*RKTRKS,	RKSIZ,		/* si3 */
	3*MAXTRKS+1*RKTRKS,	RKSIZ,		/* si4 */
	3*MAXTRKS+2*RKTRKS,	RKSIZ,		/* si5 */
	3*MAXTRKS+3*RKTRKS,	RKSIZ,		/* si6 */
	3*MAXTRKS+4*RKTRKS,	RKSIZ,		/* si7 */
	3*MAXTRKS+5*RKTRKS,	RKSIZ,		/* si8 */
	3*MAXTRKS+6*RKTRKS,	RKSIZ,		/* si9 */
	3*MAXTRKS+7*RKTRKS,	MAXSIZ,		/* si10 */
	4*MAXTRKS+7*RKTRKS,	MAXSIZ,		/* si11 */
	5*MAXTRKS+7*RKTRKS,	MAXSIZ,		/* si12 */
	6*MAXTRKS+7*RKTRKS,	MAXSIZ,		/* si13 */
	7*MAXTRKS+7*RKTRKS,	MAXSIZ,		/* si14 */
	8*MAXTRKS+7*RKTRKS,	MAXSIZ,		/* si15 */
#endif 9762
#ifndef SI9762		/* disk layout for 80 Mb disk */
	0*HEADS,	40*CYLSIZ,	/* si0 */
	40*HEADS,	40*CYLSIZ,	/* si1 */
	80*HEADS,	40*CYLSIZ,	/* si2 */
	120*HEADS,	40*CYLSIZ,	/* si3 */
	160*HEADS,	80*CYLSIZ,	/* si4 */
	240*HEADS,	171*CYLSIZ,	/* si5 */
	80*HEADS,	80*CYLSIZ,	/* si6 -- overlays si[23] */
	0*HEADS,	411*CYLSIZ	/* si7 -- the entire disk */
#endif 9760
};

struct devtab sitab;
struct buf sibuf;		/* for raw i/o */


sistrategy(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	if (bp->b_flags&B_PHYS)
		mapalloc(bp);
	if (siaddr(bp))		/* check transfer & form disk address */
	{	iodone(bp);
		return;
	}
	bp->av_forw = 0;
	spl5();		/* link buffer into I/O queue */
	if (sitab.d_actf == 0)
		sitab.d_actf = bp;
	else
		sitab.d_actl->av_forw = bp;
	sitab.d_actl = bp;
	if (sitab.d_active == 0)
		sistart();
	spl0();
}


int siaddr(abp)
struct buf *abp;
/*
 * Compute port/cylinder and head/sector disk addresses
 * and place in bp->pcyl and bp->hdsec.  A non-zero return value
 * indicates that the transfer specified by b_dev, b_blkno, and
 * b_wcount is invalid, and pcyl & hdsec are left untouched.
 */
{
	register struct buf *bp;
	register struct siminor *mp;
	register logical blkcnt;	/* # of blocks in transfer */
	int blkoff;
	int i;

/*
 * Validate the transfer; check minor device existence & extent.
 */
	bp = abp;
	i = bp->b_dev.d_minor&(NSI-1);
	mp = &sisizes[i];
	if ((mp->nblocks != 0 && bp->b_blkno >= mp->nblocks) ||
	    (blkcnt = (255 - bp->b_wcount)/256) == 0 ||
	    (bp->b_blkno != 0 && blkcnt > mp->nblocks - bp->b_blkno) ||
	    (bp->b_blkno == 0 && mp->nblocks != 0 && blkcnt > mp->nblocks) )
	{	bp->b_flags =| B_ERROR;
		bp->b_error = ENXIO;
		bp->b_resid = bp->b_wcount;
		return(1);
	}
/*
 * Form port/cylinder and head/sector disk addresses.
 */
	blkoff = (mp->trkoff%HEADS)*TRKSIZ + lrem(bp->b_blkno, CYLSIZ);
	bp->pcyl = (mp->trkoff/HEADS) + ldiv(bp->b_blkno, CYLSIZ) +
	    blkoff/CYLSIZ + (bp->b_dev.d_minor/NSI << 10);
	bp->hdsec = blkoff%CYLSIZ;
	return(0);
}


sistart()
{
	register struct buf *bp;
	register int *rp;		/* for siregs */
	int cmd, m;

	if ((bp = sitab.d_actf) == 0)
		return;
	sitab.d_active++;
	cmd = ((bp->b_xmem&03) << 4)
	      |((bp->b_flags&B_READ)? READ:WRITE);

	rp = &SIADDR->simar;

	SIADDR->sicnr = RESET;
	*rp   =  bp->b_addr;		/* simar */
	*--rp =  bp->hdsec;		/* sihsr */
	*--rp =  bp->pcyl;		/* sipcr */
	*--rp =  -bp->b_wcount;		/* siwcr */
	*--rp =  IENABLE|cmd|GO;	/* sicnr */
}


siintr()
{
	register struct buf *bp;

	if (sitab.d_active == 0)
		return;
	bp = sitab.d_actf;
	sitab.d_active = 0;
	if (SIADDR->sierr < 0)	/* error summary */
	{	deverror(bp, SIADDR->sierr, SIADDR->sicnr);
		if (++sitab.d_errcnt <= 10)
		{	sistart();
			return;
		}
		bp->b_flags =| B_ERROR;
	}
	sitab.d_errcnt = 0;
	sitab.d_actf = bp->av_forw;
	bp->b_resid = 1 + SIADDR->siwcr;   /* reads back as 1's complement */
	iodone(bp);
	sistart();
}


siread(dev)
int dev;
{
	physio(&sistrategy, &sibuf, dev, B_READ);
}


siwrite(dev)
int dev;
{
	physio(&sistrategy, &sibuf, dev, B_WRITE);
}
