#
/*
 */

/*
 * TM tape driver
 */

#include "param.h"
#include "buf.h"
#include "conf.h"
#include "user.h"

struct {
	int tmer;
	int tmcs;
	int tmbc;
	int tmba;
	int tmdb;
	int tmrd;
};

struct	devtab	tmtab;
struct	buf	rtmbuf;

#ifdef NOSC	/* extra flag in rtmbuf.b_flags to handle odd-length writes */
#define B_ODDLEN 0100000
#endif NOSC
#define B_SCOM	 0040000

char	t_openf[8];
char	*t_blkno[8];
char	*t_nxrec[8];

#define	TMADDR	0172520

#define NOREW	020

#define	GO	01
#define NOCMD	000
#define	RCOM	002
#define	WCOM	004
#define	WEOF	006
#define	SFORW	010
#define	SREV	012
#define	WIRG	014
#define	REW	016
#define	DENS	060000		/* 9-channel */
#define	IENABLE	0100
#define	CRDY	0200
#define GAPSD	010000
#define	TUR	1
#define	HARD	0102200	/* ILC, EOT, NXM */
#define	EOF	0040000

#define	SSEEK	1
#define	SIO	2
#define SCOM	3

tmopen(dev, flag)
{
	register dminor;

	dminor = dev.d_minor&07;
	if (t_openf[dminor])
		u.u_error = ENXIO;
	else {
		t_openf[dminor]++;
		t_blkno[dminor] = 0;
		t_nxrec[dminor] = 65535;
	}
}

tmclose(dev, flag)
{
	register int dminor;

	dminor = dev.d_minor&07;
	if (flag) {
		tcommand(dminor, WEOF);
		tcommand(dminor, WEOF);
		tcommand(dminor, dev&NOREW ? SREV : REW);
	} else {
		if((dev&NOREW)==0)
			tcommand(dminor, REW);
		else if(t_openf[dminor] != -1)
			tcommand(dminor, SFORW);
	}
	t_openf[dminor] = 0;
}

tcommand(dev, com)
{
	register struct buf *bp;

	bp = &rtmbuf;
	spl6();
	while(bp->b_flags & B_BUSY) {
		bp->b_flags =| B_WANTED;
		sleep(bp, PRIBIO);
	}
	spl0();
	bp->b_dev = dev;
	bp->b_resid = com;
	bp->b_blkno = 0;
	bp->b_flags = B_SCOM | B_BUSY | B_READ;
	tmqup(bp);
	iowait(bp);
	if(bp->b_flags & B_WANTED)
		wakeup(bp);
	bp->b_flags = 0;
	return(bp->b_resid);
}

tmstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char **p;

	bp = abp;
	if(bp->b_flags&B_PHYS)
		mapalloc(bp);
	p = &t_nxrec[bp->b_dev.d_minor&07];
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
	tmqup(bp);
}

tmqup(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	bp->av_forw = 0;
	spl5();
	if (tmtab.d_actf==0)
		tmtab.d_actf = bp;
	else
		tmtab.d_actl->av_forw = bp;
	tmtab.d_actl = bp;
	if (tmtab.d_active==0)
		tmstart();
	spl0();
}

tmstart()
{
	register struct buf *bp;
	register int com;
	int unit;
	register char *blkno;

    loop:
	if ((bp = tmtab.d_actf) == 0)
		return;
	com = ((unit=bp->b_dev.d_minor&07)<<8) | ((bp->b_xmem & 03) << 4) | DENS;
	if(bp->b_flags&B_SCOM) {
		if(bp->b_resid == NOCMD) {
			/* don't want IENABLE bit or else */
			/* interrupt will occur immediately */
			TMADDR->tmcs = com;
			bp->b_resid = TMADDR->tmer;
			tmdone();
			goto loop;
		}
		tmtab.d_active = SCOM;
		TMADDR->tmbc = 1;	/* i.e. infinity, or until eof */
		TMADDR->tmcs = bp->b_resid | com | IENABLE | GO;
		return;
	}
	if (t_openf[unit] < 0 || (TMADDR->tmcs & CRDY)==0) {
		bp->b_flags =| B_ERROR;
		tmdone();
		goto loop;
	}
	blkno = t_blkno[unit];
	if (blkno != bp->b_blkno) {
		tmtab.d_active = SSEEK;
		if (blkno < bp->b_blkno) {
			com =| SFORW|IENABLE|GO;
			TMADDR->tmbc = blkno - bp->b_blkno;
		} else {
			com =| SREV|IENABLE|GO;
			TMADDR->tmbc = bp->b_blkno - blkno;
		}
		TMADDR->tmcs = com;
		return;
	}
	tmtab.d_active = SIO;
	TMADDR->tmbc = bp->b_wcount << 1;
#ifdef NOSC
	if(bp->b_flags & B_ODDLEN) TMADDR->tmbc++;
#endif NOSC
	TMADDR->tmba = bp->b_addr;		/* core address */
	TMADDR->tmcs = com | ((bp->b_flags&B_READ)? RCOM|IENABLE|GO:
	    ((tmtab.d_errcnt)? WIRG|IENABLE|GO: WCOM|IENABLE|GO));
}

tmdone()
{
	register struct buf *bp;

	bp = tmtab.d_actf;
	tmtab.d_errcnt = 0;
	tmtab.d_actf = bp->av_forw;
	tmtab.d_active = 0;
	iodone(bp);
}

tmintr()
{
	register struct buf *bp;
	register int unit;

	if ((bp = tmtab.d_actf)==0)
		return;
	unit = bp->b_dev.d_minor&07;
	/* ignore errors on SCOM */
	if (tmtab.d_active == SCOM) {
		bp->b_resid = TMADDR->tmbc -1;	/* return skip count */
		tmdone();
		tmstart();
		return;
	}
	if (TMADDR->tmcs < 0) {		/* error bit */
		while(TMADDR->tmrd & GAPSD) ; /* wait for gap shutdown */
		if ((TMADDR->tmer&(HARD|EOF))==0 && tmtab.d_active==SIO) {
			if (++tmtab.d_errcnt < 10) {
				t_blkno[unit]++;
				tmtab.d_active = 0;
				tmstart();
				return;
			}
/*
			deverror(bp, TMADDR->tmer, 0);
 */
		} else
			if(bp != &rtmbuf && (TMADDR->tmer&EOF)==0)
				t_openf[unit] = -1;
/*
			else
				deverror(bp, TMADDR->tmer, 0);
 */
		bp->b_flags =| B_ERROR;
		tmtab.d_active = SIO;
	}
	if (tmtab.d_active == SIO) {
		t_blkno[unit]++;
		bp->b_resid = TMADDR->tmbc;
		tmdone();
	} else
		t_blkno[unit] = bp->b_blkno;
	tmstart();
}
#ifndef NOSC
tmread(dev)
{
	tmphys(dev);
	physio(tmstrategy, &rtmbuf, dev, B_READ);
	u.u_count = -rtmbuf.b_resid;
}

tmwrite(dev)
{
	tmphys(dev);
	physio(tmstrategy, &rtmbuf, dev, B_WRITE);
	u.u_count = 0;
}

tmphys(dev)
{
	register unit, a;

	unit = dev.d_minor&07;
	a = lshift(u.u_offset, -9);
	t_blkno[unit] = a;
	t_nxrec[unit] = ++a;
}
#endif NOSC
#ifdef NOSC
		/* Special NOSC code to permit odd-length reads and writes */
tmread(dev)
{
	physio(tmstrategy, &rtmbuf, dev, tmphys(dev)|B_READ);
	u.u_count = -rtmbuf.b_resid;
}

tmwrite(dev)
{
	physio(tmstrategy, &rtmbuf, dev, tmphys(dev)|B_WRITE);
	u.u_count = 0;
}

tmphys(dev)
{
	register unit, a;

	unit = dev.d_minor&07;
	a = lshift(u.u_offset, -9);
	t_blkno[unit] = a;
	t_nxrec[unit] = ++a;
	if(u.u_count & 01) {
		u.u_count++;
		return (B_ODDLEN);
	}
	return 0;
}
#endif NOSC
