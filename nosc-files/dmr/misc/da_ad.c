#
/*
 */

/*
 * AD disk driver with changes for da driver
 */

#include "/usr/sys/h/param.h"
#include "/usr/sys/h/buf.h"
#include "/usr/sys/h/conf.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/da.h"

struct {
	int	adds;
	int	ader;
	int	adcs;
	int	adwc;
	int	adba;
	int	adca;
	int	adda;
};

struct devtab daadtab;		/* aed device driver table */


#define	ADADDR	0176710
#define	NAD	2

struct {
	char	*nblocks;
	int	cyloff;
} ad_sizes[] {
	-1,	0,		/* cyl 0 thru 327 (jsk) */
	-1,	328,		/* cyl 328 thru 655  (jsk) */
	24000,	286,		/* cyl 286 thru 405 JSK */
	-1,	0,		/* cyl 0 thru 327 */
	35000,	0,		/* cyl 0 thru 174 */
	5000,	175,		/* cyl 175 thru 199 */
	58600,	200,		/* cyl 200 thru 492 */
	33000,	493,		/* cyl 493 thru 657 */
};

struct	devtab	daadtab;
struct	buf	radbuf;

#define	GO	01
#define	RESET	0
#define	HSEEK	014

#define	IENABLE	0100
#define	READY	0200

#define	SUFU	01000
#define	SUSU	02000
#define	SUSI	04000
#define	HNF	010000

/*
 * Use av_back to save track+sector,
 * b_resid for cylinder.
 */

#define	trksec	av_back
#define	cylin	b_resid

daadstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char *p1, *p2;

	bp = abp;
	if(bp->b_flags&B_PHYS)
		mapalloc(bp);
	p1 = &ad_sizes[bp->b_dev.d_minor&07];
	if (bp->b_dev.d_minor >= (NAD<<3) ||
	    bp->b_blkno >= p1->nblocks) {
		bp->b_flags =| B_ERROR;
		iodone(bp);
		return;
	}
	while ( dastate & DAADLOCK)			/* loop while locked out */
		sleep (&dastate.hibyte, PRIBIO );	/* wait til unlocked */
	bp->av_forw = 0;
	bp->cylin = p1->cyloff;
	p1 = bp->b_blkno;
	p2 = lrem(p1, 10);
	p1 = ldiv(p1, 10);
	bp->trksec = (p1%20)<<8 | p2;
	bp->cylin =+ p1/20;
	spl5();
	if ((p1 = daadtab.d_actf)==0)
		daadtab.d_actf = bp;
	else {
		for (; p2 = p1->av_forw; p1 = p2) {
			if (p1->cylin <= bp->cylin
			 && bp->cylin <  p2->cylin
			 || p1->cylin >= bp->cylin
			 && bp->cylin >  p2->cylin) 
				break;
		}
		bp->av_forw = p2;
		p1->av_forw = bp;
	}
	if (daadtab.d_active==0)
		daadstart();
	spl0();
}

daadstart()
{
	register struct buf *bp;

	if ((bp = daadtab.d_actf) == 0)
		return;
	daadtab.d_active++;
	ADADDR->adda = bp->trksec;
	devstart(bp, &ADADDR->adca, bp->cylin, bp->b_dev.d_minor>>3);
}

daadintr()
{
	register struct buf *bp;
	register int ctr;
/* JSK */	int timectr;

	if ( dastate & DAADISW)			/* da switch? */
	{
		dadskint();		/* use da interrupt routine */
		goto done;
	}
	if (daadtab.d_active == 0)
		goto done;
	bp = daadtab.d_actf;
	daadtab.d_active = 0;
	if (ADADDR->adcs < 0) {		/* error bit */
		deverror(bp, ADADDR->ader, ADADDR->adds);
		if(ADADDR->adds & (SUFU|SUSI|HNF))
		do {
			ADADDR->adcs.lobyte = RESET|GO;
			ADADDR->adcs.lobyte = HSEEK|GO;
			ctr = 0;
			timectr = 20;	/* this should be at least 2 sec */
			while (	(ADADDR->adds&SUSU)	/* wait til done or */
			&& 	(--ctr || --timectr)	); /* 2 secs pass */
		} while (ADADDR->adds & (SUSU|SUFU) );
		ADADDR->adcs = RESET|GO;
		ctr = 0;
		while ((ADADDR->adcs&READY) == 0 && --ctr);
		if (++daadtab.d_errcnt <= 10) {
			daadstart();
			goto done;
		}
		bp->b_flags =| B_ERROR;
	}
	daadtab.d_errcnt = 0;
	daadtab.d_actf = bp->av_forw;
	bp->b_resid = ADADDR->adwc;
	iodone(bp);
	daadstart();
	done:
	;
}

daadread(dev)
{

	if(daadphys(dev))
	physio(daadstrategy, &radbuf, dev, B_READ);
}

daadwrite(dev)
{

	if(daadphys(dev))
	physio(daadstrategy, &radbuf, dev, B_WRITE);
}

daadphys(dev)
{
	register c;

	c = lshift(u.u_offset, -9);
	c =+ ldiv(u.u_count+511, 512);
	if(c > ad_sizes[dev.d_minor & 07].nblocks) {
		u.u_error = ENXIO;
		return(0);
	}
	return(1);
}
