#
/*
 */

#include "../h/param.h"
#include "../h/user.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/systm.h"
#include "../h/proc.h"
#include "../h/seg.h"
#include "../h/inode.h"

/*
 * This is the set of buffers proper, whose heads
 * were declared in buf.h.  There can exist buffer
 * headers not pointing here that are used purely
 * as arguments to the I/O routines to describe
 * I/O to be done-- e.g. swbuf, just below, for
 * swapping.
 */
char	buffers[NBUF][514];
struct semaphore swap_sem
{
	NSWBUF,0
};
struct  buf	swapbufs[NSWBUF];

/*
 * Declarations of the tables for the magtape devices;
 * see bdwrite.
 */
int	tmtab;
int	httab;

/*
 * The following several routines allocate and free
 * buffers with various side effects.  In general the
 * arguments to an allocate routine are a device and
 * a block number, and the value is a pointer to
 * to the buffer header; the buffer is marked "busy"
 * so that no on else can touch it.  If the block was
 * already in core, no I/O need be done; if it is
 * already busy, the process waits until it becomes free.
 * The following routines allocate a buffer:
 *	getblk
 *	bread
 *	breada
 * Eventually the buffer must be released, possibly with the
 * side effect of writing it out, by using one of
 *	bwrite
 *	bdwrite
 *	bawrite
 *	brelse
 */

/*
 * Read in (if necessary) the block and return a buffer pointer.
 */
bread(dev, blkno)
{
	register struct buf *rbp;

	rbp = getblk(dev, blkno);
	if (rbp->b_flags&B_DONE)
		return(rbp);
	rbp->b_flags =| B_READ;
	rbp->b_wcount = -256;
	(*bdevsw[dev.d_major].d_strategy)(rbp);
	iowait(rbp);
	return(rbp);
}

/*
 * Read in the block, like bread, but also start I/O on the
 * read-ahead block (which is not allocated to the caller)
 */
breada(adev, blkno, rablkno)
{
	register struct buf *rbp, *rabp;
	register int dev;

	dev = adev;
	rbp = 0;
	if (!incore(dev, blkno)) {
		rbp = getblk(dev, blkno);
		if ((rbp->b_flags&B_DONE) == 0) {
			rbp->b_flags =| B_READ;
			rbp->b_wcount = -256;
			(*bdevsw[adev.d_major].d_strategy)(rbp);
		}
	}

	/*
	 * The following conditional has been changed to prevent breada's
	 * from being initiated if no buffers are available.  This should
	 * prevent a type of buffer deadlock.
	 */

	if (rablkno && !incore(dev, rablkno) &&
	    (bfreelist.av_forw != &bfreelist)) {	/* 1Dec78, cdh BBN */

		rabp = getblk(dev, rablkno);
		if (rabp->b_flags & B_DONE)
#ifdef BUFMOD
			brelse(rabp,&bfreelist);
#endif
#ifndef BUFMOD
			brelse(rabp);
#endif BUFMOD
		else {
			rabp->b_flags =| B_READ|B_ASYNC;
			rabp->b_wcount = -256;
			(*bdevsw[adev.d_major].d_strategy)(rabp);
		}
	}
	if (rbp==0)
		return(bread(dev, blkno));
	iowait(rbp);
	return(rbp);
}

/*
 * Write the buffer, waiting for completion.
 * Then release the buffer.
 */
bwrite(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register flag;


	rbp = bp;
	flag = rbp->b_flags;
	rbp->b_flags =& ~(B_READ | B_DONE | B_ERROR | B_DELWRI | B_AGE);
	rbp->b_wcount = -256;
	(*bdevsw[rbp->b_dev.d_major].d_strategy)(rbp);
	if ((flag&B_ASYNC) == 0) {
		iowait(rbp);
#ifdef BUFMOD
		brelse(rbp,&bfreelist);
#endif BUFMOD
#ifndef BUFMOD
		brelse(rbp);
#endif BUFMOD
	} else
	if (flag&B_DELWRI)
		rbp->b_flags =| B_AGE;
	else
		geterror(rbp);
}

/*
 * Release the buffer, marking it so that if it is grabbed
 * for another purpose it will be written out before being
 * given up (e.g. when writing a partial block where it is
 * assumed that another write for the same block will soon follow).
 * This can't be done for magtape, since writes must be done
 * in the same order as requested.
 */
bdwrite(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register struct devtab *dp;

	rbp = bp;
	dp = bdevsw[rbp->b_dev.d_major].d_tab;
	if (dp == &tmtab || dp == &httab)
		bawrite(rbp);
	else {
		rbp->b_flags =| B_DELWRI | B_DONE;
#ifdef BUFMOD
		brelse(rbp,&bfreelist);
#endif
#ifndef BUFMOD
		brelse(rbp);
#endif BUFMOD
	}
}

/*
 * Release the buffer, start I/O on it, but don't wait for completion.
 */
bawrite(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	rbp->b_flags =| B_ASYNC;
	bwrite(rbp);
}

#ifndef BUFMOD
/*
 * release the buffer, with no I/O implied.
 */
brelse(bp)
struct buf *bp;
{
	register struct buf *rbp, **backp;
	register int sps;

	rbp = bp;
	if (rbp->b_flags&B_WANTED)
		wakeup(rbp);
	if (bfreelist.b_flags&B_WANTED) {
		bfreelist.b_flags =& ~B_WANTED;
		wakeup(&bfreelist);
	}
	if (rbp->b_flags&B_ERROR)
		rbp->b_dev.d_minor = -1;  /* no assoc. on error */
	sps = PS->integ;
	spl6();
	if (rbp->b_flags & B_AGE) {
		backp = &bfreelist.av_forw;
		(*backp)->av_back = rbp;
		rbp->av_forw = *backp;
		*backp = rbp;
		rbp->av_back = &bfreelist;
	} else {
		backp = &bfreelist.av_back;
		(*backp)->av_forw = rbp;
		rbp->av_back = *backp;
		*backp = rbp;
		rbp->av_forw = &bfreelist;
	}
	rbp->b_flags =& ~(B_WANTED|B_BUSY|B_ASYNC|B_AGE);
	PS->integ = sps;
}
#endif BUFMOD

#ifdef BUFMOD
/*
 * release the buffer, with no I/O implied.
 */
brelse(bp,freel)
struct buf *bp,*freel;
{
	register struct buf *rbp, **backp;
	register int sps;

	rbp = bp;
	if (rbp->b_flags&B_WANTED)
		wakeup(rbp);
	if (freel->b_flags&B_WANTED) {
		freel->b_flags =& ~B_WANTED;
		wakeup(freel);
	}
	if (rbp->b_flags&B_ERROR)
		rbp->b_dev.d_minor = -1;  /* no assoc. on error */
	sps = PS->integ;
	spl6();
	if (rbp->b_flags & B_AGE) {
		backp = &freel->av_forw;
		(*backp)->av_back = rbp;
		rbp->av_forw = *backp;
		*backp = rbp;
		rbp->av_back = freel;
	} else {
		backp = &freel->av_back;
		(*backp)->av_forw = rbp;
		rbp->av_back = *backp;
		*backp = rbp;
		rbp->av_forw = freel;
	}
	rbp->b_flags =& ~(B_WANTED|B_BUSY|B_ASYNC|B_AGE);
	PS->integ = sps;
}
#endif BUFMOD


/*
 * See if the block is associated with some buffer
 * (mainly to avoid getting hung up on a wait in breada)
 */
incore(adev, blkno)
{
	register int dev;
	register struct buf *bp;
	register struct devtab *dp;

	dev = adev;
	dp = bdevsw[adev.d_major].d_tab;
	for (bp=dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno==blkno && bp->b_dev==dev)
			return(bp);
	return(0);
}








/* turn off the DELWRI flag for a buffer if pipe reader has already read it.

   bbn:mek addition 9/13/78 */

bdfree(ap,position)
struct inode *ap; int position;

{	register struct buf *bp;
	register *ip,nb,nd;

	ip = ap;
	nd = ip->i_dev;

	if ( (nb = ip->i_addr[position] ) )
  		if ( ( bp = incore(nd,nb) ) )
	 		if ( bp->b_flags & B_DELWRI )
				bp->b_flags =& ~( B_DELWRI );
}







#ifndef BUFMOD

/*
 * Assign a buffer for the given block.  If the appropriate
 * block is already associated, return it; otherwise search
 * for the oldest non-busy buffer and reassign it.
 * When a 512-byte area is wanted for some random reason
 * getblk can be called
 * with device NODEV to avoid unwanted associativity.
 */
getblk(dev, blkno)
{
	register struct buf *bp;
	register struct devtab *dp;
	extern lbolt;

	if(dev.d_major >= nblkdev)
		panic("blkdev");

    loop:
	if (dev < 0)
		dp = &bfreelist;
	else {
		dp = bdevsw[dev.d_major].d_tab;
		if(dp == NULL)
			panic("devtab");
		for (bp=dp->b_forw; bp != dp; bp = bp->b_forw) {
			if (bp->b_blkno!=blkno || bp->b_dev!=dev)
				continue;
			spl6();
			if (bp->b_flags&B_BUSY) {
				bp->b_flags =| B_WANTED;
				sleep(bp, PRIBIO+1);
				spl0();
				goto loop;
			}
			spl0();
			notavail(bp);
			return(bp);
		}
	}
	spl6();
	if (bfreelist.av_forw == &bfreelist) {
		bfreelist.b_flags =| B_WANTED;
		sleep(&bfreelist, PRIBIO+1);
		spl0();
		goto loop;
	}


	/* bbn:mek--- avoid choosing a delayed-write buffer if possible.
			(10/12/78) */
	for ( bp = bfreelist.av_forw; bp != &bfreelist; bp = bp->av_forw )
		if ( ! ( bp->b_flags & B_DELWRI ) ) break;
	if ( bp == &bfreelist )		/* no luck! take the next free buffer */
		bp = bfreelist.av_forw;
	spl0();

	notavail(bp);
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags =| B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	bp->b_flags = B_BUSY;
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	return(bp);
}
#endif BUFMOD

#ifdef BUFMOD

/* getblk and getsblk are different "entry points" to essentially the
 * same routine. They differ only in that getblk gets a free buffer block
 * from bfreelist whereas getsblk takes its from sbfreel.
 */

getblk(dev, blkno)
{
	gblk(&bfreelist,dev,blkno);
}

getsblk(dev, blkno)
{
	gblk(&sbfreel,dev,blkno);
}

gblk(freel,dev, blkno)
struct buf *freel;
{
	register struct buf *bp;
	register struct devtab *dp;
	extern lbolt;

	if(dev.d_major >= nblkdev)
		panic("blkdev");

    loop:
	if (dev < 0)
		dp = freel;
	else {
		dp = bdevsw[dev.d_major].d_tab;
		if(dp == NULL)
			panic("devtab");
		for (bp=dp->b_forw; bp != dp; bp = bp->b_forw) {
			if (bp->b_blkno!=blkno || bp->b_dev!=dev)
				continue;
			spl6();
			if (bp->b_flags&B_BUSY) {
				bp->b_flags =| B_WANTED;
				sleep(bp, PRIBIO+1);
				spl0();
				goto loop;
			}
			spl0();
			notavail(bp);
			return(bp);
		}
	}
	spl6();
	if (freel->av_forw == freel) {
		freel->b_flags =| B_WANTED;
		sleep(freel, PRIBIO+1);
		spl0();
		goto loop;
	}


	/* bbn:mek--- avoid choosing a delayed-write buffer if possible.
			(10/12/78) */
	for ( bp = freel->av_forw; bp != freel; bp = bp->av_forw )
		if ( ! ( bp->b_flags & B_DELWRI ) ) break;
	if ( bp == freel )         /* no luck! take the next free buffer */
		bp = freel->av_forw;
	spl0();

	notavail(bp);
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags =| B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	bp->b_flags = B_BUSY;
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	return(bp);
}
#endif BUFMOD

/*
 * Wait for I/O completion on the buffer; return errors
 * to the user.
 */
iowait(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	spl6();
	while ((rbp->b_flags&B_DONE)==0)
		sleep(rbp, PRIBIO);
	spl0();
	geterror(rbp);
}

/*
 * Unlink a buffer from the available list and mark it busy.
 * (internal interface)
 */
notavail(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register int sps;

	rbp = bp;
	sps = PS->integ;
	spl6();
	rbp->av_back->av_forw = rbp->av_forw;
	rbp->av_forw->av_back = rbp->av_back;
	rbp->b_flags =| B_BUSY;
	PS->integ = sps;
}

/*
 * Mark I/O complete on a buffer, release it if I/O is asynchronous,
 * and wake up anyone waiting for it.
 */
iodone(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	if(rbp->b_flags&B_MAP)
		mapfree(rbp);
	rbp->b_flags =| B_DONE;
#ifdef PASYNC
	if (rbp -> b_flags & B_PASYNC) {/* jsq BBN 2-26-79 */
                rbp -> b_flags =& ~(B_BUSY|B_WANTED|B_PASYNC);
                wakeup(rbp);
        } else
#endif
	if (rbp->b_flags&B_ASYNC)
#ifdef BUFMOD
		brelse(rbp,&bfreelist);
#endif BUFMOD
#ifndef BUFMOD
		brelse(rbp);
#endif BUFMOD
	else {
		rbp->b_flags =& ~B_WANTED;
		wakeup(rbp);
	}
}

/*
 * Zero the core associated with a buffer.
 */
clrbuf(bp)
int *bp;
{
	register *p;
	register c;

	p = bp->b_addr;
	c = 256;
	do
		*p++ = 0;
	while (--c);
}

/*
 * Initialize the buffer I/O system by freeing
 * all buffers and setting all device buffer lists to empty.
 */
binit()
{
	register struct buf *bp;
	register struct devtab *dp;
	register int i;
	struct bdevsw *bdp;

	bfreelist.b_forw = bfreelist.b_back =
	    bfreelist.av_forw = bfreelist.av_back = &bfreelist;
	for (i=0; i<NBUF; i++) {
		bp = &buf[i];
		bp->b_dev = -1;
		bp->b_addr = buffers[i];
		bp->b_back = &bfreelist;
		bp->b_forw = bfreelist.b_forw;
		bfreelist.b_forw->b_back = bp;
		bfreelist.b_forw = bp;
		bp->b_flags = B_BUSY;
#ifdef BUFMOD
		brelse(bp,&bfreelist);
#endif
#ifndef BUFMOD
		brelse(bp);
#endif BUFMOD
	}
	i = 0;
	for (bdp = bdevsw; bdp->d_open; bdp++) {
		dp = bdp->d_tab;
		if(dp) {
			dp->b_forw = dp;
			dp->b_back = dp;
		}
		i++;
	}
	nblkdev = i;
}

#ifdef BUFMOD
	/* this routine initializes the control blocks of the NKS
	 * system buffers
	 */

bsinit(bufadr)
{
	register struct buf *bp;
	register int i;

	sbfreel.b_forw = sbfreel.b_back =
	    sbfreel.av_forw = sbfreel.av_back = &sbfreel;
	for (i=0; i<NSBUF; i++) {
		bp = &sbuf[i];
		bp->b_dev = -1;
		bp->b_paddr = bufadr;
		bp->b_addr = bufadr<<6;
		bp->b_xmem = bufadr>>(16-6);
		bufadr =+ (512>>6);
		bp->b_back = &sbfreel;
		bp->b_forw = sbfreel.b_forw;
		sbfreel.b_forw->b_back = bp;
		sbfreel.b_forw = bp;
		bp->b_flags = B_BUSY;
		brelse(bp,&sbfreel);
	}
}

#endif BUFMOD

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

/*
 * startup routine for RH controllers.
 */
#define	RHWCOM	060
#define	RHRCOM	070

rhstart(bp, devloc, devblk, abae)
struct buf *bp;
int *devloc, *abae;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
	if(cputype == 70)
		*abae = rbp->b_xmem;
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

/*
 * 11/70 routine to allocate the
 * UNIBUS map and initialize for
 * a unibus device.
 * The code here and in
 * rhstart assumes that an rh on an 11/70
 * is an rh70 and contains 22 bit addressing.
 *
 * BBN: mek (5/1/79) - compile only for 11/70's.
 */


#ifdef CPU70
int	maplock;
char *ub_bufp[MAPSIZE - MAPLOW];  /* pointer to buffer holding ub piece */
char ub_size[MAPSIZE - MAPLOW];  /* this big with base at index + MAPLOW */
#endif


/* The following mapalloc and mapfree routines allow multiple processes
 * to use different segments of the unibus map concurrently.
 *              S.Y. Chiu : BBN 26Apr79
 */

mapalloc (abp)
struct buf *abp;
{
#ifdef CPU70
	register struct buf *bp;
	register i;
	register size;
	int base;
	int saveps;
	long a;

	if (cputype != 70) return;

	bp = abp;

	bp -> b_flags =| B_MAP; /* ubmap piece and say we claim another */
	size = ((( -(bp -> b_wcount) + 07777) >> 12) & 07);/* words to pages */
	saveps = PS->integ;
	spl6();
	while (( base = malloc(ubmap, size)) == 0) {  /* while size */
		maplock =| B_WANTED;    /* ubmap pages not available, sleep */
		sleep (&maplock, PSWP);
	}
	PS->integ = saveps;
	a.loword = bp -> b_addr;        /* get address from buffer for ub */
	a.hiword = bp -> b_xmem;        /* put in long for easy incrementing*/
	bp -> b_xmem = base >> 3;       /* make starting unibus address */
	bp -> b_addr = base << 13;      /* map into base ub register */
	i = base - MAPLOW;              /* base to ubbuf index */
	ub_bufp[i] = bp;                /* base for bp is saved by index */
	ub_size[i] = size;              /* of storage */
	base =<< 1;                     /* convert to word offset into */
	size = (size << 1) + base;      /* unibus map */
	for (i = base; i < size; i =+ 2, a =+ 020000) {  /* assign */
		UBMAP -> r[i] = a.loword;
		UBMAP -> r[i + 1] = a.hiword;
	}
#endif
}


mapfree (abp)
struct buf *abp;
{
#ifdef CPU70
	register struct buf *bp;
	register i, j;

	bp = abp;
	if (!(bp -> b_flags & B_MAP)) return;   /* just in case */
	bp -> b_flags =& ~B_MAP;
	for ( i = 0; i < MAPSIZE - MAPLOW; i++)
		if (ub_bufp[i] == bp) break;
	ub_bufp[i] = 0;   /* ubpage no longer has buffer attached */
	j = ub_size[i];   /* get size */
	i =+ MAPLOW;      /* and base */
	mfree (ubmap, j, i);    /* free it */
	/* mapfree doesn't bother to clear the affected pages in UBMAP */
	if (maplock & B_WANTED) wakeup (&maplock);      /* wakeup waiters */
	maplock = 0;
#endif
}



/********* code commented out
mapalloc(abp)
struct buf *abp;
{
	register i, a;
	register struct buf *bp;

	if(cputype != 70)
		return;
	spl6();
	while(maplock&B_BUSY) {
		maplock =| B_WANTED;
		sleep(&maplock, PSWP+1);
	}
	maplock =| B_BUSY;
	spl0();
	bp = abp;
	bp->b_flags =| B_MAP;
	a = bp->b_xmem;
	for(i=16; i<32; i=+2)
		UBMAP->r[i+1] = a;
	for(a++; i<48; i=+2)
		UBMAP->r[i+1] = a;
	bp->b_xmem = 1;
}

mapfree(bp)
struct buf *bp;
{

	bp->b_flags =& ~B_MAP;
	if(maplock&B_WANTED)
		wakeup(&maplock);
	maplock = 0;
}

********** end of commented out code *********/


/*
 * swap I/O
 */
swap(blkno, coreaddr, count, rdflg)
{
	register struct buf *swbuf;
	register int *fp;

	pee( &swap_sem );
	swbuf = (swapbufs[0].b_flags&B_BUSY) ? &swapbufs[1] : &swapbufs[0];
	/* Do a little consistency checking */
	if( swbuf->b_flags&B_BUSY )
		panic("SWAPBUFF");
	spl6();
	fp = &swbuf->b_flags;
	*fp = B_BUSY | B_PHYS | rdflg;
	swbuf->b_dev = swapdev;
	swbuf->b_wcount = - (count<<5); /* 32 w/block */
	swbuf->b_blkno = blkno;
	swbuf->b_addr = coreaddr<<6;    /* 64 b/block */
	swbuf->b_xmem = (coreaddr>>10) & 077;
	(*bdevsw[swapdev>>8].d_strategy)(swbuf);
	spl6();
	while((*fp&B_DONE)==0)
		sleep(fp, PSWP);
	spl0();
	*fp =& ~(B_BUSY|B_PHYS);
	vee( &swap_sem );
	return(*fp&B_ERROR);
}

/*
 * make sure all write-behind blocks
 * on dev (or NODEV for all)
 * are flushed out.
 * (from umount and update)
 */
bflush(dev)
{
	register struct buf *bp;

loop:
	spl6();
	for (bp = bfreelist.av_forw; bp != &bfreelist; bp = bp->av_forw) {
		if (bp->b_flags&B_DELWRI && (dev == NODEV||dev==bp->b_dev)) {
			bp->b_flags =| B_ASYNC;
			notavail(bp);
			bwrite(bp);
			goto loop;
		}
	}
	spl0();
}

/*
 * Raw I/O. The arguments are
 *	The strategy routine for the device
 *	A buffer, which will always be a special buffer
 *	  header owned exclusively by the device for this purpose
 *	The device number
 *	Read/write flag
 * Essentially all the work is computing physical addresses and
 * validating them.
 */
physio(strat, abp, dev, rw)
struct buf *abp;
int (*strat)();
{
	register struct buf *bp;
	register char *base;
	register int nb;
	int ts;
#ifdef PASYNC
	int async;      /* jsq BBN 2-26-79 */
#endif
	bp = abp;
	base = u.u_base;
#ifdef PASYNC                           /* jsq BBN 2-26-79 */
	async = rw & B_PASYNC;          /* are we not sleeping? */
	if (async && (rw & B_READ)) goto bad;   /* can't read, then */
#endif
	/*
	 * Check odd base, odd count:  address wraparound checked below
	 */
	if (base&01 || u.u_count&01)
		goto bad;
	ts = (u.u_tsize+127) & ~0177;
	if (u.u_sep)
		ts = 0;
	nb = (base>>6) & 01777;
	/*
	 * Check overlap with text. (ts and nb now
	 * in 64-byte clicks)
	 */
	if (nb < ts)
		goto bad;
	/*
	 * Check for address wraparound.
	 * Check that transfer is either entirely in the
	 * data or in the stack: that is, either
	 * the end is in the data or the start is in the stack
	 */
	if ((base >= (base + u.u_count)) ||
	  ((((base+u.u_count)>>6)&01777) >= ts+u.u_dsize
	    && nb < 1024-u.u_ssize))
#ifndef LCBA
		goto bad;
#endif
#ifdef  LCBA
		/*
		 * Maybe it's an lcb:  base must be in a mapped
		 * page; the page must be read/write access if doing
		 * a read from a device; and the memory area must not
		 * extend past the end of the lcba (though it may go
		 * past the part mapped into the page).
		 */
		{
		    if (u.u_lcbflg == 0) goto bad;
		    ts = (nb >> 7) + (u.u_sep? 8: 0);   /* find base page */
		    if (!u.u_lcbmd[ts]) goto bad;       /* is it mapped? */
		    if (((u.u_lcbmd[ts] >> 8) & 127)    /* does area mapped */
			< (nb & 0177))     /* include base? */
			    goto bad;
		    if ((rw & B_READ) && ((u.u_lcbmd[ts] & 07) != RW))
			    goto bad;   /* right access?*/
		    if ((u.u_lcbma[ts] +
		      (((u.u_count + 63) >> 6) & 01777))
			> lcba_size)    /* area to transfer too big? */
			    goto bad;
		}
		/*
		 * Note actual address computation below is from hardware
		 * registers, so no changes are needed.
		 */
#endif
	spl6();
	while (bp->b_flags&B_BUSY) {
		bp->b_flags =| B_WANTED;
		sleep(bp, PRIBIO+1);
	}
	bp->b_flags = B_BUSY | B_PHYS | rw;
	bp->b_dev = dev;
	/*
	 * Compute physical address by simulating
	 * the segmentation hardware.
	 */
	bp->b_addr = base&077;
	base = (u.u_sep? UDSA: UISA)->r[nb>>7] + (nb&0177);
	bp->b_addr =+ base<<6;
	bp->b_xmem = (base>>10) & 077;
	bp->b_blkno = lshift(u.u_offset, -9);
	bp->b_wcount = -((u.u_count>>1) & 077777);
	bp->b_error = 0;
#ifdef PASYNC
	if (!async) {           /* jsq BBN 2-26-79 */
#endif
		u.u_procp->p_flag =| SLOCK;
		(*strat)(bp);
		spl6();
		while ((bp->b_flags&B_DONE) == 0)
			sleep(bp, PRIBIO);
		u.u_procp->p_flag =& ~SLOCK;
		if (bp->b_flags&B_WANTED)
			wakeup(bp);
		spl0();
		bp->b_flags =& ~(B_BUSY|B_WANTED);
		u.u_count = (-bp->b_resid)<<1;
		geterror(bp);
#ifdef PASYNC
	} else {                /* jsq BBN 2-26-79 */
		(*strat)(bp);
		spl0();
	}
#endif
	return;
    bad:
	u.u_error = EFAULT;
}

/*
 * Pick up the device's error number and pass it to the user;
 * if there is an error but the number is 0 set a generalized
 * code.  Actually the latter is always true because devices
 * don't yet return specific errors.
 */
geterror(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	if (bp->b_flags&B_ERROR)
		if ((u.u_error = bp->b_error)==0)
			u.u_error = EIO;
}
