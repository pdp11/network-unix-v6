#
/*
 * TU16 driver by Larry Wehr  PY 1A111 x7982
 * Handles one TM02 controller, up to 4 TU16 transports
 * minor device classes:
 * bits 0,1: slave select
 * bit 2 off: rewind on close; on: position after first TM
 * bit 3 off: 800 bpi; on: 1600 bpi
 *
 * Modified by Rob Mathews (Stanford Univ. (415)497-1703 ) to
 * elaborate the error handling somewhat, to make cooked I/O
 * a little more intelligent about TM's, and to allow raw I/O
 * to read TM's as zero-length records.
 *
 */

#include "../h/param.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/user.h"

#define NUNIT 4

struct {
	int	htcs1, htwc, htba, htfc;
	int	htcs2, htds, hter, htas;
	int	htck, htdb, htmr, htdt;
	int	htsn, httc, htbae, htcs3;
};

struct	devtab	httab;
struct	buf	rhtbuf, chtbuf;

int	h_openf[NUNIT];		/*  0 if closed, httc contents if open,
				    RAWTM set iff TM on last raw read,
				    and top bit set if fatal error.  */
char	*h_blkno[NUNIT],
	*h_tmblk[NUNIT];

int htdebug;			/*  nonzero to debug  */


#define	HTADDR	0172440

/*  htcs1  */
#define	NOP	0
#define	GO	01
#define	REW	06
#define	DCLR	010
#define	ERASE	024
#define	WEOF	026
#define	SFORW	030
#define	SREV	032

#define	IENABLE	0100
#define	RDY	0200
#define	TRE	040000

/*  httc  */
#define FATERR	0100000		/*  Software bits  */
#define RAWTM	040000

#define	P800	01300		/* 800 + pdp11 mode */
#define	P1600	02300		/* 1600+pdp11 mode */

/*  htds  */
#define	TM	04
#define	PES	040
#define	DRY	0200
#define	EOT	02000
#define	WRL	04000
#define MOL	010000
#define PIP	020000
#define	ERR	040000

/*  hter  */
#define	HARD	064073  /* UNS|OPI|NEF|FMT|CPAR|DPAR|RMR|ILR|ILF */
#define	FCE	01000
#define	CS	02000
#define	COR	0100000

/*  htcs2  */
#define CHARD	077400			/* Hard controller errors. */

/*  Operation states  */
#define	SSEEK	1
#define	SIO	2
#define	SABORT	3
#define	SRETRY	4
#define	SCOM	5
#define	SATMK	6

htopen(dev, flag)
{
	register unit, ds;

	unit = dev&03;
	if (unit >= NUNIT || h_openf[unit]) {
		u.u_error = ENXIO;
		return;
	}
	h_openf[unit] = (dev&010 ? P1600 : P800)|unit;
	h_blkno[unit] = 0;
	h_tmblk[unit] = -1;
	ds = hcommand(dev, NOP);
	if ((ds&MOL)==0 || (flag && (ds&WRL)))
	  {	u.u_error = ENXIO;
		h_openf[unit] = 0;
	  }
}
htclose(dev, flag)
{	
	register int rdev, unit;

	rdev = dev;
	unit = rdev&03;
	if (flag) {
		hcommand(rdev, WEOF);
		hcommand(rdev, WEOF);
	}

	/*  Rewind if specified.  Otherwise, skip as required.  */
	hcommand(rdev,
		  !(rdev&04)? REW
			    : flag ? SREV
				   :  h_openf[unit]&RAWTM ? NOP
							  : SFORW);
	h_openf[unit] = 0;
}
hcommand(dev, com)
{
	register struct buf *bp;

	bp= &chtbuf;
	spl5();
		while(bp->b_flags&B_BUSY) {
			bp->b_flags =| B_WANTED;
			sleep(bp, PRIBIO);
		}
	spl0();
	bp->b_dev = dev;
	bp->b_resid = com;
	bp->b_blkno = 0;
	bp->b_flags = B_BUSY|B_READ;
	htstrategy(bp);
	iowait(bp);
	if(bp->b_flags&B_WANTED)
		wakeup(bp);
	bp->b_flags = 0;
	return(bp->b_resid);
}
htstrategy(abp)
struct buf *abp;
{	
	register struct buf *bp;
	register char **p;

	bp = abp;
	p = &h_tmblk[bp->b_dev&03];
	if (*p <= bp->b_blkno) {
		if (*p < bp->b_blkno) {
			bp->b_flags =| B_ERROR;
			iodone(bp);
			return;
		}
		if (bp->b_flags&B_READ) {	
			clrbuf(bp);
			iodone(bp);
			return;
		}
	}
	if ((bp->b_flags&B_READ)==0)
		*p = bp->b_blkno + 1;
	bp->av_forw = 0;
	spl5();
		if (httab.d_actf==0)
			httab.d_actf = bp;
		else
			httab.d_actl->av_forw = bp;
		httab.d_actl = bp;
		if (httab.d_active==0)
			htstart();
	spl0();
}
htstart()
{
	register struct buf *bp;
	register int unit;
	register char *blkno;

    loop:
	if ((bp = httab.d_actf) == 0)
		return;
	unit = bp->b_dev&03;
	HTADDR->htcs2 = 0;
	HTADDR->httc = h_openf[unit] =& ~RAWTM;

	/*  If rewinding, must await completion.  */
	if (HTADDR->htds&PIP)
	  {	HTADDR->htas = 1<<unit;
		HTADDR->htcs1 = TRE|IENABLE;
		return;
	  }
	HTADDR->htcs1 = TRE|DCLR|GO;
 	blkno = h_blkno[unit];
	if (bp == &chtbuf) {
		if (bp->b_resid==NOP) {
			bp->b_resid = HTADDR->htds;
			goto next;
		}
		httab.d_active = SCOM;
		HTADDR->htfc = 0;
		HTADDR->htcs1 = bp->b_resid|IENABLE|GO;
		return;
	}
	if(h_openf[unit]&FATERR)
	  goto abort;
	if (blkno == bp->b_blkno) {
		httab.d_active = SIO;
		rhstart(bp, &HTADDR->htfc, bp->b_wcount<<1, &HTADDR->htbae);
	} else {
		httab.d_active = SSEEK;
		if (blkno < bp->b_blkno) {
			HTADDR->htfc = blkno - bp->b_blkno;
			HTADDR->htcs1 = SFORW|IENABLE|GO;
		} else {
			HTADDR->htfc = bp->b_blkno - blkno;
			HTADDR->htcs1 = SREV|IENABLE|GO;
		}
	}
	return;
    abort:
	bp->b_flags =| B_ERROR;
    next:
	httab.d_active = 0;
	httab.d_actf = bp->av_forw;
	iodone(bp);
	goto loop;
}

htintr()
  {
	register struct buf *bp;
	register int unit, state;

	if ((bp = httab.d_actf)==0) return;
	unit = bp->b_dev&03;
	state = httab.d_active;

/*  A very useful tool for debugging........................  */
	if( htdebug != 0 )
	  printf("\nbn%d cb%d\nds%o er%o fc%o s%o\n",
		  bp->b_blkno, h_blkno[unit],
		  HTADDR->htds, HTADDR->hter, HTADDR->htfc, state);
/*  ........................................................  */

	/*  Handle errors and TM's whenever not backing
	    over one (cooked TM recovery).		*/
	if ((HTADDR->htcs1&TRE) ||
	    (HTADDR->htds&EOT)  ||	/*  for now...  */
	    (HTADDR->htds&TM && state!=SATMK))

	  /*  Screen out fatal errors (tape position lost).  */
	  if (HTADDR->htcs2&CHARD ||
	      HTADDR->hter&HARD   ||
	      HTADDR->htds&EOT    ||	/* for now... */
	      state==SSEEK && (HTADDR->hter&~FCE)!=0 ||
	      state!=SIO && state!=SCOM && state!=SSEEK)

	    {	/*  Fatal.  Abort all operations and chain ahead.  */
		h_openf[unit] =| FATERR;
		state = SABORT;			/* for below */

		deverror(bp, HTADDR->hter, HTADDR->htcs2);
		bp->b_flags =| B_ERROR;
	    }

	  /*  Nonfatal.  Deal with it.  */
	   else switch(state)
	    {
		case SIO:
			/*  Screen out actual errors:  anything if
			    writing; any remaining controller errors;
			    if reading, things not corrected; if
			    reading raw, not FCE's; and in any event
			    no TM-caused FCE's.			*/
			if ((bp->b_flags&B_READ)==0 ||
			    HTADDR->htcs2.hibyte    ||
			    HTADDR->hter&~(HTADDR->htds&PES ? CS|COR : 0)
				&~(bp== &rhtbuf || HTADDR->htds&TM ? FCE : 0))

			  /*  It's an error.  Retry if not too many.  */
			  if (httab.d_errcnt++ < 10)
			    {	httab.d_active = SRETRY;
				HTADDR->htcs1 = TRE|DCLR|GO;
				HTADDR->htfc = -1;
				HTADDR->htcs1 = SREV|IENABLE|GO;
				return;
			    }

			   /*  Too many errors.  */
			   else
			    {	deverror(bp, HTADDR->hter, HTADDR->htcs2);
				bp->b_flags =| B_ERROR;
				break;
			    }

			/*  Not actually an error.  Get out of here
			    unless cooked TM, which is handled as 
			    for a seek.  Note: NRZ TM causes fc
			    to be set to 2!!!			*/

			/*  If raw read, all is okay as is.  */
			if (!(HTADDR->htds&TM)) break;

			/*  If raw TM, zero fc (to be sure).  Otherwise,
			    set it to 1 for the seek logic.	*/
			if ((HTADDR->htfc = bp== &rhtbuf ? 0 : 1)==0)
			  {	h_openf[unit] =| RAWTM;
				break;
			  }

		case SSEEK:

			/*  Must be cooked (no SSEEK on raw) TM on a
			    forward skip (FCE above).  Confine tape
			    position to before the TM.		*/
			h_blkno[unit] = bp->b_blkno + HTADDR->htfc - 1;
			bp->b_flags =| B_ERROR;

			httab.d_active = SATMK;
			HTADDR->htcs1 = TRE|DCLR|GO;
			HTADDR->htfc = -1;
			HTADDR->htcs1 = SREV|IENABLE|GO;
			return;

		case SCOM:
			/*  Ignore everything.  */
			;
	    };

	/*  Either no error or completely processed error.  */
	switch(state)
	  {
	    case SRETRY:
		if ((bp->b_flags&B_READ)==0)
		  {	httab.d_active = SSEEK;
			HTADDR->htcs1 = ERASE|IENABLE|GO;
			return;
		  }

	    case SSEEK:
		h_blkno[unit] = bp->b_blkno;
		break;


	    case SIO:
		h_blkno[unit]++;

	    case SCOM:
	    case SABORT:
	    case SATMK:
		httab.d_active = httab.d_errcnt = 0;
		httab.d_actf = bp->av_forw;
		bp->b_resid = HTADDR->htfc;
		iodone(bp);
	  };

	/*  Start an operation (perhaps the old same one).  */
	htstart();
  }

htread(dev)
{
	htphys(dev);
	physio(htstrategy, &rhtbuf, dev, B_READ);
	u.u_count = u.u_arg[1] - rhtbuf.b_resid;
}

htwrite(dev)
{
	htphys(dev);
	physio(htstrategy, &rhtbuf, dev, B_WRITE);
	u.u_count = 0;
}

htphys(dev)
{
	register unit, a;

	unit = dev&03;
	a = lshift(u.u_offset, -9);
	h_blkno[unit] = a;
	h_tmblk[unit] = ++a;
}
