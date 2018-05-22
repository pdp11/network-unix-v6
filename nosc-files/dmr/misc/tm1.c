#

/*
 * TM tape driver
 *
 *	As modified by the naval post graduate school
 *	and then demodified slightly by UCLA-ATS (so
 *	it would compile under normal c and be less
 *	installation dependent
 *	further mods by R Balocca UoI
 */

#include "/usr/sys/h/param.h"
#include "/usr/sys/h/buf.h"
#include "/usr/sys/h/conf.h"
#include "/usr/sys/h/user.h"

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

char	t_openf[8];
char	*t_blkno[8];
char	*t_nxrec[8];

#define	TMADDR	0172520
#define	GO	01
#define	RCOM	02
#define	WCOM	04
#define	WEOF	06
#define	SFORW	010
#define	SREV	012
#define	WIRG	014
#define	REW	016
#define	DENS	060000		/* 9-channel */
#define	IENABLE	0100
#define	CRDY	0200
#define	GAPSD  010000
#define	TUR	1
#define 	WRL	4
#define	HARD	0102200	/* ILC, EOT, NXM */
#define	EOF	0040000
#define	SSEEK	1
#define	SIO	2

tmopen(dev, flag)
{
    register int dminor;
    register int i;
    extern lbolt;

	dminor = dev.d_minor;

/*	Open drive (if not already open) and check for
 *	mountedness, writeabilty etc.
 */
	if (t_openf[dminor])
		u.u_error = ENXIO;
	else {
		t_openf[dminor]++;
	for(i=0; i<4; i++)	/* try three times */
	{   while (tmtab.d_active || (TMADDR->tmcs & CRDY)==0)
                sleep(&lbolt,1);
	    TMADDR->tmcs = 060000 | (dminor<<8);
	    if(TMADDR->tmer & TUR)
		if(!flag || !(TMADDR->tmer&WRL))
		{   t_blkno[dminor] = 0;
                    t_nxrec[dminor] = 65535;
                    return;
                }
            else sleep(&lbolt,1);
        }
	u.u_error = ENXIO;
/*	tape drive can't be opened now, reset to not open */
        t_openf[dminor] = 0;
	}
}

tmclose(dev, flag)
{
	register int dminor;

	dminor = dev.d_minor;
	t_openf[dminor] = 0;
	if (flag)
		tcommand(dminor, WEOF); /* do it twice? */
	tcommand(dminor, REW);
}

/*
 * tmcont - added by npgs allows the user to pass a command using
 *	    the sgtty mechanism for raw i/o
 *	    The first argument word will be interpreted as a
 *		command and ored into the command register
 *		(but it can not be a read, write or writeirg).
 *	    The second argument word will be placed into the
 *		byte count register (after being negated) prior
 *		to the issueing of the command. (added by Mark
 *		Kampe at ATS)
 */

tmcont(dev,v)
{

	register com;

	com = u.u_arg[0];
	if(com<WEOF || com>REW) 
	{	u.u_error = EINVAL;	/* no reads or writes allowed */
		return;
	}
	TMADDR->tmbc = -u.u_arg[1];
	tcommand(dev.d_minor,com);
	sleep(&tmtab,1);	/* and wait for command to finish */
}

tcommand(unit, com)
{

	while (tmtab.d_active || (TMADDR->tmcs & CRDY)==0)
		sleep(&tmtab, 1);
	TMADDR->tmcs = IENABLE|DENS|com|GO | (unit<<8);
}

tmstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char **p;

	bp = abp;
	if(bp->b_flags&B_PHYS)
		mapalloc(bp);
	p = &t_nxrec[bp->b_dev.d_minor];
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
	unit = bp->b_dev.d_minor;
	blkno = t_blkno[unit];
	if (t_openf[unit] < 0 || (TMADDR->tmcs & CRDY)==0) {
		bp->b_flags =| B_ERROR;
		tmtab.d_actf = bp->av_forw;
		iodone(bp);
		goto loop;
	}
	com = (unit<<8) | ((bp->b_xmem&03)<<4) | IENABLE|DENS;
	if (blkno != bp->b_blkno) {
		tmtab.d_active = SSEEK;
		if (blkno < bp->b_blkno) {
			com =| SFORW|GO;
			TMADDR->tmbc = blkno - bp->b_blkno;
		} else {
			if (bp->b_blkno == 0 /*|| we are going back more than half way into the woods... */ )
				com =| REW|GO;
			else {
				com =| SREV|GO;
				TMADDR->tmbc = bp->b_blkno - blkno;
			}
		}
		TMADDR->tmcs = com;
		return;
	}
	tmtab.d_active = SIO;
	TMADDR->tmbc = bp->b_wcount << 1;
	TMADDR->tmba = bp->b_addr;		/* core address */
	TMADDR->tmcs = com | ((bp->b_flags&B_READ)? RCOM|GO:
	    ((tmtab.d_errcnt)? WIRG|GO: WCOM|GO));
}

tmintr()
{
	register struct buf *bp;
	register int unit;

	wakeup(&tmtab);
	if ((bp = tmtab.d_actf)==0)
		return;
	unit = bp->b_dev.d_minor;
	if (TMADDR->tmcs < 0) {		/* error bit */
/*
		deverror(bp, TMADDR->tmer, 0);
 */
		while(TMADDR->tmrd & GAPSD) ; /* wait for gap shutdown */
		if ((TMADDR->tmer&(HARD|EOF))==0 && tmtab.d_active==SIO) {
			if (++tmtab.d_errcnt < 10) {
				t_blkno[unit]++;
				tmtab.d_active = 0;
				tmstart();
				return;
			}
		} else
			if(bp != &rtmbuf && (TMADDR->tmer&EOF)==0)
				t_openf[unit] = -1;
		bp->b_flags =| B_ERROR;
		tmtab.d_active = SIO;
	}
	if (tmtab.d_active == SIO) {
		tmtab.d_errcnt = 0;
		t_blkno[unit]++;
		tmtab.d_actf = bp->av_forw;
		tmtab.d_active = 0;
		iodone(bp);
		bp->b_resid = TMADDR->tmbc;
	} else
		t_blkno[unit] = bp->b_blkno;
	tmstart();
}

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

	unit = dev.d_minor;
	a = lshift(u.u_offset, -9);
	t_blkno[unit] = a;
	t_nxrec[unit] = ++a;
}
