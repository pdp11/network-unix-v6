#

/*
 * RL01 disk driver
 *
 * Tom Ferrin - 19 July 78
 * UCSF Computer Graphics Laboratory
 *
 * This driver has now been running for several weeks on a dual drive 11/60.
 * It doesn't minimize seek scheduling & totally ignores DEC's bad block table.
 */

#include "param.h"
#include "buf.h"
#include "conf.h"
#include "user.h"

#ifndef NRL
#define	NRL	4
#endif not NRL
#ifndef RLADDR
#define	RLADDR	0174400
#endif not RLADDR
#define	NRLBLK	10220	/* block = 2 sectors = 512 bytes, last track reserved */

#define HNF	010000
#define OPI	02000
#define	CRDY	0200
#define	IENABLE	0100
#define RCOM	014
#define WCOM	012
#define RHCOM	010
#define SCOM	06
#define GSCOM	04
#define DRST	010
#define DSDIR	04
#define DSTAT	02
#define DMARK	01
#define VC	01000

#define rlexec(a,b) dp->rlcs=((a)<<8)|(b); while((dp->rlcs&CRDY)==0)

struct {
	int rlcs;
	int rlba;
	int rlda;
	int rlmp;
};

struct {
	char openf;	/* open flag */
	int cyltrk;	/* current cylinder & head select */
} rl01[NRL];

struct	devtab	rltab;
struct	buf	rrlbuf;

rlopen(dev, flag)
{
	register int unit, *dp, i;

	if (rl01[unit=dev.d_minor].openf == 0) {
		dp = RLADDR;
		spl5();
		dp->rlda = DRST|DSTAT|DMARK;
		rlexec(unit, GSCOM);
		rlexec(unit, RHCOM);
		i = dp->rlcs;
		spl0();
		if (i < 0) {
			u.u_error = EIO;
			return;
		}
		rl01[unit].cyltrk = dp->rlmp & ~077;
		rl01[unit].openf++;
	}
}

rlstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register int i;

	bp = abp;
	if (bp->b_flags&B_PHYS)
		mapalloc(bp);
	if (bp->b_dev.d_minor >= NRL || (i=bp->b_blkno) >= NRLBLK) {
		u.u_error = ENXIO;
		bp->b_flags =| B_ERROR;
		iodone(bp);
		return;
	}
	bp->av_back = (i/20)<<6 | (i%20)<<1;	/* desired address */
	bp->av_forw = 0;
	spl5();
	if (rltab.d_actf==0)
		rltab.d_actf = bp;
	else
		rltab.d_actl->av_forw = bp;
	rltab.d_actl = bp;
	if (rltab.d_active==0)
		rltran();
	spl0();
}

rltran()
{
	register struct buf *bp;
	register int i;

	if ((bp=rltab.d_actf) == 0)
		return;
	i = bp->av_back;
	i = (40-(i&077))*128;	/* words left on track */
	if (i >= -bp->b_wcount)
		bp->b_resid = 0;
	else {
		bp->b_resid = bp->b_wcount+i; /* words left for next transfer */
		bp->b_wcount = -i;
	}
	rlstart();
}

rlstart()
{
	register struct buf *bp;
	register int da, *dp;
	int unit, ca, rda;

	rltab.d_active++;
	bp = rltab.d_actf;
	unit = bp->b_dev.d_minor;
	da = bp->av_back;
	dp = &rl01[unit].cyltrk;
	if ((da&~077) != *dp) {
		ca = *dp & ~100;
		*dp = da & ~077;
		dp = RLADDR;
		rda = da & ~0177;
		da = (da&0100)>>2;
		if ((ca =- rda) < 0)
			ca = -ca|DSDIR;
		dp->rlda = da|ca|DMARK;
		rlexec(unit, SCOM);
	}
	dp = &RLADDR->rlmp;
	*dp = bp->b_wcount;
	*--dp = bp->av_back;
	*--dp = bp->b_addr;
	da = (bp->b_xmem&03)<<4 | unit<<8 | IENABLE;
	if (bp->b_flags&B_READ)
		da =| RCOM;
	else
		da =| WCOM;
	*--dp = da;
}

rlintr()
{
	register struct buf *bp;
	register int *dp, i;
	int n[2];

	if (rltab.d_active == 0)
		return;
	bp = rltab.d_actf;
	rltab.d_active = 0;
	if ((dp=RLADDR)->rlcs < 0) {	/* error bit */
		n[0] = dp->rlcs;
		i = bp->b_dev.d_minor;
		dp->rlda = DSTAT|DMARK;
		rlexec(i, GSCOM);
		n[1] = dp->rlmp;
		deverror(bp, n[0], n[1]);
		dp->rlda = DRST|DSTAT|DMARK;
		rlexec(i, GSCOM);
		if ((n[0]&(HNF|OPI))==(HNF|OPI) || (n[1]&VC)) {
			rlexec(i, RHCOM);
			if (dp->rlcs < 0)
				goto abort;
			rl01[i].cyltrk = dp->rlmp & ~077;
		}
		if (++rltab.d_errcnt <= 10) {
			rlstart();
			return;
		}
	  abort:
		bp->b_flags =| B_ERROR;
		bp->b_resid = 0;
	}
	rltab.d_errcnt = 0;
	if (bp->b_resid) {
		n[0] = bp->b_xmem;	/* more of previous transfer */
		n[1] = bp->b_addr;
		dpadd(n, -bp->b_wcount<<1);
		bp->b_xmem = n[0];
		bp->b_addr = n[1];
		bp->b_wcount = bp->b_resid;
		i = bp->av_back;
		bp->av_back = (i&~077) + 0100;
	} else {
		rltab.d_actf = bp->av_forw;
		iodone(bp);
	}
	rltran();
}

rlread(dev)
{

	physio(rlstrategy, &rrlbuf, dev, B_READ);
}

rlwrite(dev)
{

	physio(rlstrategy, &rrlbuf, dev, B_WRITE);
}
