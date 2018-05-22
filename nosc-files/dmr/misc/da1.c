#
/*
 * add check to see if system is built right : if swap or root is on
 * aed, then....
 */
/*
 * DA Converter Driver
 *
 * Modified 26Nov77 J.S.Kravitz to allow
 *
 * 1) Display list operation after particular stty call (eventuall, all will
 *    be display list operation, but for the moment, this is compatible
 *    with existing software.)
 * 2) Writes from multiple processes
 * 3) gtty returns various values, depending on last stty call
 *
 */

#include	"/usr/sys/h/param.h"
#include	"/usr/sys/h/buf.h"
#include	"/usr/sys/h/conf.h"
#include	"/usr/sys/h/user.h"
#include	"/usr/sys/h/da.h"

struct
{
	char	lo_byte;
	char	hi_byte;
};

struct buf	dapbuf;		/* for passing parameters between
				 * write and strategy routines with help
				 * from physio
struct	devtab	daotab;
 * 
				 */

struct buf	dabuf[2];	/* the buffer headers for ad and da;
				 * two for double-buffering
				 */

int	dabufn;			/* selects current dabuf */

#define	DABSIZE	2560		/* buffer size */

#define	DABLREC	(2560/256)	/* NOTE: NUMERATOR MUST = DABSIZE !!!
				 * this is the number of disk blocks per record
				 */

struct buf	*daorgbp;	/* will point to original buffer header */

char	*daebn;			/* block number of last block to be
				 *  transferred
				 */

int	daewc;			/* word count for last transfer */

struct
{
	int	word;
};

struct {
	int	adds;
	int	ader;
	int	adcs;
	int	adwc;
	int	adba;
	int	adca;
	int	adda;
};

#define	ADADDR	0176710

struct			/* global dec for the ad format structs */
{
	char	*nblocks;
	int	cyloff;
}
ad_sizes[8];


dastrategy(bufp)
struct buf	*bufp;
{
	register struct buf	*bp;
	register int	ub;		/* copy of u.u_base for getting
					 * params out of user space
					 */
	register int	ssplno;		/* holds index of 1st desired sample
					 * in 1st buffer, becomes virtual
					 * address of same later
					 */
	char	*t[4];			/* will hold params when we get them
					 * from user space
					 */

	bp = bufp;			/* get pointer to param buffer */
	if (dastate & DADISPLAY)
	{
		/*
		 * we get here when a write is issued and the da-driver
		 * is in DISPLAY mode. At this point the user program is
		 * locked in core. If there is a pending request this
		 * request is queued and the program is put to sleep
		 * until the current, and this new request is satisfied.
		 * This allows cooperating user processes to provide
		 * 'Multiply-bufferred' data to be output.
		 */
		bp = bufp;
		bp->av_forw = 0;
		spl6();
		if (daotab.d_actf==0)
			daotab.d_actf = bp;
		else
			daotab.d_actl->av_forw = bp;
		daotab.d_actl = bp;
	
		/*
		 * If we get here, we are starting up interface cold.
		 * Clear the interface controller and call dastart for
		 * the real work
		 */
		DAADDR->dadrba = 0;			/* clear ba overflow if any */
		DAADDR->dadrst = ( DARSET1 | DARESET);		/* reset attention */
		dastate =& ~ ( DADONE | DAADWT | DACVWT );
		DAADDR->dadrst = DARESET;		/* reset reset attention */
		bp->b_flags =& ~B_DONE;		/* reset done to avoid trouble */
		if (daotab.d_active==0)
			dastart();
		iowait (bp);
	}
	else
	{
		daorgbp = bp;			/* remember for dastart's use later */
		if ( (-bp->b_wcount) < ((DABSIZE * 2) + 4) )	/* user's buffer too
								   small? */
		{
	badpar:		/* come here on bad param errors */
			bp->b_flags =| B_ERROR;		/* set buf's error bit */
			bp->b_error = EINVAL;		/* set error code */
			iodone(bp);			/* takes care of the rest */
			return;				/* we're all done */
		}
		ub = u.u_base;		/* get virtual address of user's write */
		t[0] = fuword(ub);	/* get 1st sample number (2 words) */
		t[1] = fuword(ub =+ 2);
		t[2] = fuword(ub =+ 2);	/* get last sample number (2 words) */
		t[3] = fuword(ub =+ 2);
		ssplno = t[1].lo_byte & 0377;	/* 1st sample # mod 256
						 * gives 1st sample # in 1st block
						 */
		dabuf[0].b_blkno = lshift(&t[0],-8);	/* 1st sample # div 256
							 * gives 1st block #
							 */
		t[1] = lrem(dabuf[0].b_blkno,DABLREC);	/* 1st block # mod blocks/rec
							 * gives residue of 1st block
							 * in 1st record
							 */
		dabuf[0].b_blkno =- t[1];	/* 1st block # - residue
						 * gives block # of 1st record
						 */
		ssplno =+ (t[1] <<8);		/* (1st sample # in 1st block)
						 * +(block residue * 256)
						 * gives 1st sample # in 1st record
						 */
			/* the following work similarly to the previous, but it
			 * is the quantities for the last sample we are computing;
			 * note that we compute a word count (negative).
			 */
		daewc = t[3].lo_byte & 0377;
		daebn = lshift(&t[2],-8);
		t[3] = lrem(daebn,DABLREC);
		daebn =- t[3];
		daewc = -(daewc + (t[3] << 8) );
	
		if ( daewc == 0 )		/* final word count = 0? */
		{
			daewc = -DABSIZE;	/* set to full buffer size */
			daebn =- (DABLREC);	/* decrease last block number
							   by blocks per buffer */
		}
		if (daebn <= dabuf[0].b_blkno)	/* last block # <= first? */
			goto	badpar;		/* process bad parameter */
		dabuf[0].b_wcount = dabuf[1].b_wcount = -DABSIZE;	/* word counts */
		dabuf[0].b_dev = dabuf[1].b_dev = DAADDEV;	/* devices */
		dabuf[0].b_error = dabuf[1].b_error = 0;	/* error codes */
		dabuf[0].b_addr = bp->b_addr + 010;		/* buffer addr (skip over
								   params ) */
		dabuf[0].b_xmem = bp->b_xmem;		/* copy xmem bits */
		if ( dabuf[0].b_addr < bp->b_addr )	/* did adding to addr overflow?
							*/
		{
			dabuf[0].b_xmem++;	/* increment xmem bits */
		}
		dabuf[0].b_flags = B_READ;		/* set read bit */
		dabuf[1].b_flags = B_READ;		/* in both buffers */
		dabuf[1].b_addr = dabuf[0].b_addr + (DABSIZE<<1);
							/* addr of 2nd buf is 1st +
							   buffer size */
		dabuf[1].b_xmem = dabuf[0].b_xmem;	/* copy xmem bits */
		if ( dabuf[1].b_addr < dabuf[0].b_addr ) /* did addr overflow? */
		{
			dabuf[1].b_xmem++;	/* increment xmem bits */
		}
		DAADDR->dadrba = 0;			/* clear ba overflow if any */
		DAADDR->dadrst = ( DARSET1 | DARESET);		/* reset attention */
		daadstrategy(&dabuf[0]);		/* start 1st disk transfer */
		dabufn = 0;			/* set buffer switch */
		/*
		 * reset done, diskwaiting and converter waiting flags
		 */
		dastate =& ~ ( DADONE | DAADWT | DACVWT );
		iowait(&dabuf[0]);		/* wait for 1st disk read */
		dastate =| DAADLOCK;			/* set ad lockout of other requests */
		spl1();				/* let interrupts happen */
		/*
		 * wait until all aed requests are done, and then swipe
		 * controller
		 */
		while ( (daadtab.d_active | daadtab.d_actf) != 0 );
		dastate =| DAADISW | DASEEK;
		DAADDR->dadrst = DARESET;		/* reset reset attention */
		ub = u.u_base;			/* set back to beg of buffer */
		for ( ssplno =+ ub ; --ssplno >= ub ; )		/* loop thru virtual
								   addresses of undesired
								   samples in 1st buffer,
								   if any */
			suword(ssplno,0);	/* set undesired samples to zero */
		dabuf[1].b_blkno = dabuf[0].b_blkno + (DABLREC);	/* block number for 2nd block */
		dadskstart(&dabuf[1]);		/* start read of 2nd block */
		sleep (&dastate, PRIBIO);	/* wait til done */
		spl6();				/* no interrupts during dastart */
		bp->b_flags =& ~B_DONE;		/* reset done to avoid trouble */
		dastart();	/* start conversion */
		sleep (&dastate.hibyte, PRIBIO);	/* wait til done */
		iodone(bp);	/* io is finished */
	}	/* end of regular/display mode */
}


dastart()
{
	register struct buf	*dabp,		/* will point to buffer now
						   going to converter */
				*adbp;		/* will point to buffer now
						   going to disk */
	register int	ub;		/* copy of u.u_base for getting


	if (dastate & DADISPLAY)
	{
		/*
		 * this is where the display list is acutally
		 * interpreted. One by one, requests are removed from
		 * daotab, and processed. When completed, a wakeup is
		 * performed on the current entry so that the associated
		 * program will wake up (asleep in the strategy routine
		 * if not the fist transfer)
		 */
		if ((dabp = daotab.d_actf) == 0)
			return;
		daotab.d_active++;
		/*
		 * start output with current  buffer
		 */
		if ( dastate & DADONE )		/* all done? */
		{
			DAADDR->dadrst =& ~DAINTENB;	/* shut off converter interrupts */
			wakeup (bp);	/* wake user on it */
			return;				/* that's all */
		}
		bp = bufp;	/* get pointer to buffer */
		t[0] = fuword(u.u_base);	/* proto get */
		/*
		 * start up transfer
		 */
		DAADDR->dadrwc = ;	/* set word count in interface */
		DAADDR->dadrba = ;	/* set buffer address in interface */
		/*
		 * set interface status: extended mem bits, interrupt enable
		 * reset attention, set go
		 */
		DAADDR->dadrst = ( (dabp->b_xmem << DAMEMSHIFT) & DAXMEM)
				| (DARESET | DAINTENB | DAGO);
	}
	else
	{
		if ( daorgbp->b_flags & B_DONE )	/* already done? */
			return;
		if ( dastate & DADONE )		/* all done? */
		{
			DAADDR->dadrst =& ~DAINTENB;	/* shut off converter interrupts */
			/*
			 * restore disk interrupt handling to normal and re-enable
			 * other's use of the disk
			 */
			dastate =& ~ (DAADISW | DAADLOCK);
			wakeup (&dastate.hibyte);	/* wake waiters on it */
			return;				/* that's all */
		}
		dabp = &dabuf[dabufn];		/* point to current buffer from disk */
		DAADDR->dadrwc = dabp->b_wcount;	/* set word count in interface */
		DAADDR->dadrba = dabp->b_addr;	/* set buffer address in interface */
		/*
		 * set interface status: extended mem bits, interrupt enable
		 * reset attention, set go
		 */
		DAADDR->dadrst = ( (dabp->b_xmem << DAMEMSHIFT) & DAXMEM)
				| (DARESET | DAINTENB | DAGO);
		adbp = &dabuf[dabufn =^ 1];	/* point to other buffer */
		/* next block number is previous + blocks per record;
		 * is this <= last block number?
		 */
		if ( (adbp->b_blkno = dabp->b_blkno + (DABLREC)) <= daebn )
		{
			if ( adbp->b_blkno == daebn )	/* last read? */
				adbp->b_wcount = daewc;	/* set word count */
			adbp->b_flags = B_READ;		/* set read */
			dadskstart(adbp);		/* start read */
		}
		else		/* block number > last block number */
		{
			dastate =| DADONE;	/* set done flag */
		}
	}
}

dadskstart(bp)
struct buf	*bp;
{
	register char	*p1,	/* scratch for computations */
			*p2;

	p1 = bp->b_blkno;	/* get block # */
	p2 = lrem(p1,10);	/* sector */
	p1 = ldiv(p1,10);	/* cylinder and track */
	ADADDR->adda = (p1 % 20) << 8 | p2;
	devstart(bp,&ADADDR->adca, ad_sizes[DAADDEV].cyloff +(p1/20),0);
}

daintr()
{
		register struct buf *bp;
	
		if (daotab.d_active == 0)
			return;
		bp = daotab.d_actf;
		daotab.d_active = 0;
		/*
		 * error jazz
		 */
		if (harderror)
		{
			bp->b_flags =| B_ERROR;
		}
		daotab.d_errcnt = 0;
		daotab.d_actf = bp->av_forw;
		/*
		 * iodone, which is normally done here, should be in dastart,
		 * since we can do several transfers from the same buffer.
		 */
		dastart();
	}
	if ( dastate & DADONE )	/* all done? */
	{
		dastart();	/* final processing */
		return;
	}
	if ( DAADDR->dadrst < 0 )	/* error? */
	{
		/*
		 * no error bits set and buf addr = 0?
		 */
		if ( ( (DAADDR->dadrst & DAEBITS) | DAADDR->dadrba) == 0 )
		{
			/*
			 * reset error condition and re-init
			 */
			DAADDR->dadrba = 0;	/* yes, 0 = 0, resets error */
			DAADDR->dadrst = ((DAADDR->dadrst + DAINCMEM) & DAXMEM)
					 | (DARESET | DAINTENB | DAGO);
			return;		/* that's all */
		}
		/*
		 * set error bit in original request
		 */
		daorgbp->b_flags =| B_ERROR;
		dastate =| DADONE;		/* set done flag */
		dastart();			/* process done */
		return;
	}
	if (	dastate & DADISPLAY
	||	dastate & DAADWT )		/* disk waiting? */
	{
		dastate =& ~ DAADWT;	/* reset flag */
		dastart();		/* start conversion */
	}
	else				/* last read not complete */
	{
		dastate =| DACVWT;	/* set converter waiting flag */
	}
}

dadskint()
{
	spl6();					/* lock out converter ints */
	if ( dastate & DASEEK )			/* seeking 2nd block? */
	{
		dastate =& ~ DASEEK;		/* reset flag */
		wakeup (&dastate);		/* wake waiter */
		return;
	}
	if ( dastate & DADONE )			/* all done? */
		return;
	if ( ADADDR->adcs < 0 )			/* disk error? */
	{
		daorgbp->b_flags =| B_ERROR;	/* set error flag in buf */
		dastate =| DADONE;		/* set done flag */
		dastart();			/* process done */
		return;
	}
	if ( dastate & DACVWT )			/* converter waiting? */
	{
		dastate =& ~ DACVWT;		/* reset flag */
		dastart();			/* and start converter */
	}
	else
	{
		dastate =| DAADWT;		/* else set disk wait flag */
	}
}

dawrite(dev)
{
	physio (dastrategy, &dapbuf, dev, B_WRITE);
}

dasgtty(dev,vec)
int	*vec;
{
	register int	*v;

	v = vec;
	if ( v != 0 )				/* gtty */
	{
		switch (dastate & DARETMODES)
		{
		case DANORM:
			*v++ = DAADDR->dadrwc;
			*v++ = DAADDR->dadrba;
			*v++ = DAADDR->dadrst;
			return;

		case DARETSTATE:
			*v++ = 0;
			*v++ = 0;
			*v++ = 0;
			return;
		}
	}
	switch (u.u_arg[0])
	{
	case 0:
		DAADDR->dadrst = u.u_arg[1];
		return;

	case 1:
		/*
		 * damn well better not be a transfer running now
		 */
		dastate = (dastate & ~DASETMODES)
			| (u.u_arg[1] & DASETMODES);
		return;
	}
}

daopen()
{
}

daclose()
{
}
