#
/*
 */

/*
 * TIU (DR11-B) interface to Spider
 */

#include "../../h/param.h"
#include "../../h/conf.h"
#include "../../h/user.h"
#include "../../h/buf.h"
#include "../../h/reg.h"
#include "../../h/inode.h"

#define	NCHAN	8

/* bits in tiuch flags */
#define	T_WRITR	01
#define	T_RAVL	02
#define	T_ERROR	04
#define	T_DONE	010
#define	T_OPEN	020
#define	T_EOF	040
#define	T_STOP	0100
#define	T_WRITW 0200

/* drst bits */
#define	T_TERROR 0100000
#define	T_REJ	04000
#define	T_IDLE	02000
#define	T_SON	01000

/* drdb bits */
#define	T_BKSTS	0100000
#define	T_SLSTS	040000
#define	T_WTSTS	010000
#define	T_TROUB	02000
#define	T_ODD	01000
#define	T_SIGNL	0400
#define	T_SELW	0200

#define	TIUPRI	(-1)
#define	TIULPRI	1
#define	TIUADDR	0172430

/* tiu command bits */
#define	IENABLE	0100
#define	GO	01
#define	STOP	0
#define	RCH	02
#define	RDC	04
#define	RNM	06
#define	WSB	010
#define	WCH	012
#define	WDC	014
#define	WDB	016

#define	SREAD	1
#define	SWRITE	2
#define	SWSIG	3
#define	SWDONE	4
#define	SSTOP	5
#define	SSEL	6

#define	TIMLIM	5

struct {
	int	drwc;
	int	drba;
	int	drst;
	int	drdb;
};
struct tiuch {
	char	t_flags;
	char	t_isig;
	char	t_osig;
	char	t_troub;
	char	*t_buffer;
};

struct tiuch tiu_dchan[NCHAN];
struct tiuch tiu_cchan[NCHAN];

struct tiu {
	char	t_state;
	char	t_chan;
	char	t_time;
	char	t_timo;
	char	t_nopen;
	struct	buf *t_actf;
	struct	buf *t_actl;
} tiu;

tiuopen(dev, flag)
{
	int tiutimeout();
	register struct tiuch *cp;
	register struct buf *bp;

	if ((cp = tiuptr(dev)) == NULL) {
		u.u_error = ENXIO;
		return;
	}
	if (cp->t_flags&T_OPEN) {
		u.u_error = EBUSY;
		return;
	}
	cp->t_flags = T_OPEN;
	cp->t_osig = 1;
	if (tiu.t_nopen++ == 0) {
		tiu.t_chan = -1;
		timeout(tiutimeout, 0, HZ);
	}
	cp->t_flags =| T_STOP;
	tiustart();
}

tiuclose(dev)
{
	register struct tiuch *cp;

	if ((cp = tiuptr(dev)) == 0)
		return;
	cp->t_flags =| T_STOP;
	tiustart();
	cp->t_flags =& T_STOP;
	tiu.t_nopen --;
}

tiuwrite(dev)
{
	register int n;
	register struct tiuch *cp;
	register struct buf *bp;

	if ((cp = tiuchan(dev)) == NULL)
		return;
	do {
		spl5();
		if ((cp->t_flags&T_WRITR) == 0) {
			cp->t_flags =| T_WRITW;	/* want select W */
			tiustart();
		}
		while ((cp->t_flags&(T_WRITR|T_ERROR|T_EOF)) == 0)
			sleep(cp, TIULPRI);
		spl0();
		if (tiucheck(cp))
			return;
		bp = getblk(NODEV);
		if (n = min(512, u.u_count))
			iomove(bp, 0, n, B_WRITE);
		if (u.u_count == 0 || u.u_error)
			bp->b_blkno = cp->t_osig;
		else
			bp->b_blkno = 0;
		bp->b_dev.d_minor = dev.d_minor;
		bp->b_wcount = n;
		bp->b_flags = B_WRITE;
		spl5();
		cp->t_flags =& ~T_DONE;
		tiustrategy(bp);
		while ((cp->t_flags&(T_DONE|T_ERROR|T_EOF)) == 0)
			sleep(cp, TIUPRI);
		spl0();
		brelse(bp);
		if (tiucheck(cp))
			return;
	} while (u.u_count);
}

tiuread(dev)
{
	register int n;
	register struct tiuch *cp;
	register struct buf *bp;

	if ((cp = tiuchan(dev)) == NULL)
		return;
	spl5();
	while ((cp->t_flags&(T_RAVL|T_ERROR|T_EOF))==0)
		sleep(cp, TIULPRI);
	spl0();
	if (tiucheck(cp))
		return;
	bp = getblk(NODEV);
	bp->b_flags = B_READ;
	bp->b_dev.d_minor = dev.d_minor;
	cp->t_flags =& ~T_DONE;
	tiustrategy(bp);
	while ((cp->t_flags&(T_DONE|T_ERROR|T_EOF)) == 0)
		sleep(cp, TIUPRI);
	spl0();
	if (cp->t_isig = bp->b_blkno)
		cp->t_flags =& ~T_RAVL;
	if (tiucheck(cp) == 0) {
		if (n = min(bp->b_wcount, u.u_count))
			iomove(bp, 0, n, B_READ);
	}
	brelse(bp);
}

tiucheck(acp)
struct tiuch *acp;
{
	register struct tiuch *cp;

	cp = acp;
	if (cp->t_flags & (T_EOF | T_ERROR)) {
		if (cp->t_flags&T_ERROR)
			u.u_error = EIO;
		return(1);
	}
	return(0);
}

tiustart()
{
	register struct buf *bp;
	register i;

	if (tiu.t_state)
		return;
	for (i=0; i<NCHAN; i++) {
		if (tiucntrl(i, &tiu_dchan[i])
		 || tiucntrl(i+64, &tiu_cchan[i]))
		return;
	}
	if ((bp = tiu.t_actf)==0)
		return;
	if (stiuchan(bp->b_dev.d_minor) == 0)
		return;
	TIUADDR->drba = bp->b_addr;
	if (bp->b_flags&B_READ) {
		TIUADDR->drwc = -257;
		tiu.t_state = SREAD;
		TIUADDR->drst = IENABLE|RDC|GO;
	} else {
		tiu.t_state = SWRITE;
		if ((TIUADDR->drwc = -(bp->b_wcount>>1))==0) {
			tiuintr();
			return;
		}
		TIUADDR->drst = IENABLE|WDC|GO;
	}
	tiu.t_time = TIMLIM;
}

tiucntrl(chan, acp)
struct tiuch *acp;
{
	register struct tiu *cp;

	cp = acp;
	if (cp->t_flags&T_STOP) {
		if (stiuchan(chan+0400)) {
			cp->t_flags =& ~T_STOP;
			tiu.t_state = SSTOP;
			tiu.t_time = TIMLIM;
		}
		return(1);
	}
	if (cp->t_flags&T_WRITW) {
		if (stiuchan(chan+0200)) {
			cp->t_flags =& ~T_WRITW;
			tiu.t_time = TIMLIM;
		}
		return(1);
	}
	return(0);
}

stiuchan(ac)
{
	register int c;

	c = ac;
	if (c != tiu.t_chan) {
		if ((TIUADDR->drst&T_IDLE)==0 || (TIUADDR->drdb&T_SLSTS)==0) {
			tiu.t_state = SSEL;
			tiu.t_time = TIMLIM;
			return(0);
		}
		tiu.t_chan = c&0177;
		TIUADDR->drdb = c;
		TIUADDR->drst = IENABLE|WCH|GO;
	}
	return(1);
}

tiutimeout()
{
	tiu.t_time--;
	if (tiu.t_time == 0) {
		tiu.t_timo++;
		tiuintr();
	} else if (tiu.t_time < 0)
		tiu.t_time = 0;
	if (tiu.t_nopen)
		timeout(tiutimeout, 0, HZ);
}

tiuintr()
{
	register struct buf *bp;
	register struct tiuch *cp;
	struct tiuch *lastcp;
	register int s;

	s = tiu.t_state;
	if ((s==SSTOP || s==SSEL) && tiu.t_timo==0
	 && ((TIUADDR->drst&T_IDLE)==0 || (TIUADDR->drdb&T_SLSTS)==0))
		return;
	tiu.t_time = 0;
	cp = tiuptr(tiu.t_chan);
	tiu.t_state = 0;
	bp = NULL;
	if (s && s!=SSTOP && s!=SSEL) {
		bp = tiu.t_actf;
		if (bp==NULL || cp==NULL) {
			tiuerr(-1, 0);
			goto done;
		}
	}
	if ((TIUADDR->drst&(T_TERROR|T_REJ)) || tiu.t_timo) {
		tiuerr(tiustop(), 040+s+040*(tiu.t_timo!=0));
		goto done;
	}
	if (TIUADDR->drdb&T_TROUB) {
		tiuerr(tiustop(), TIUADDR->drdb);
		goto done;
	}
	switch (s) {

	case SSTOP:
		tiustop();
		goto done;

	case SREAD:
		if ((TIUADDR->drdb&T_SIGNL) == 0) {
			tiuerr(tiustop(), 10);
			goto done;
		}
		s = 513 + (TIUADDR->drwc<<1);
		if ((TIUADDR->drdb&T_ODD) != 0)
			s++;
		bp->b_wcount = s;
		bp->b_blkno = (TIUADDR->drdb).lobyte;
		s = SREAD;
		goto done;

	case SWRITE:
		if (bp->b_wcount&01) {
			TIUADDR->drdb = bp->b_addr[bp->b_wcount-1];
			TIUADDR->drst = IENABLE|WDB|GO;
			if ((TIUADDR->drst & T_IDLE)==0) {
				tiu.t_state = SWSIG;
				tiu.t_time = TIMLIM;
				return;
			}
		}

	case SWSIG:
		cp->t_flags =& ~T_WRITR;
		TIUADDR->drdb = bp->b_blkno;
		TIUADDR->drst = IENABLE|WSB|GO;
		if ((TIUADDR->drst & T_IDLE)==0) {
			tiu.t_state = SWDONE;
			tiu.t_time = TIMLIM;
			return;
		}

	done:
	case SWDONE:
	case SSEL:
		if (bp && tiu.t_actf)
			tiu.t_actf = bp->av_forw;
		if (cp) {
			wakeup(cp);
			cp->t_flags =| T_DONE;
		}

	default:
		if (TIUADDR->drdb&T_WTSTS && cp) {
			cp->t_flags =| T_WRITR;
			wakeup(cp);
		}
		lastcp = cp;
		while (TIUADDR->drdb&T_BKSTS) {
			TIUADDR->drst = IENABLE|RCH|GO;
			if ((cp = tiuptr(TIUADDR->drdb)) == NULL)
				tiuerr(-1, 0);
			else if (s==0 || lastcp != cp) {
				cp->t_flags =| (TIUADDR->drdb&T_SELW)!=0?
					T_RAVL:T_WRITR;
				wakeup(cp);
			}
		}
		tiustart();
	}
}

tiustop()
{
	register int lastchan;

	lastchan = tiu.t_chan;
	tiu.t_chan = -1;
	tiu.t_state = 0;
	TIUADDR->drst = IENABLE|STOP|GO;
	return(lastchan);
}

tiuerr(chan, acode)
{
	register struct tiuch *cp;
	register code;

	tiu.t_timo = 0;
	code = acode;
	if ((cp = tiuptr(chan)) != NULL)
		tiucherr(cp, code);
	else {
		tiu.t_state = 0;
		tiu.t_actf = 0;
		for (cp = tiu_dchan; cp < &tiu_dchan[2*NCHAN]; cp++)
			tiucherr(cp, code);
	}
}

tiucherr(acp, code)
struct tiuch *acp;
{
	register struct tiuch *cp;

	cp = acp;
	cp->t_flags =& ~(T_WRITR|T_RAVL);
	cp->t_flags =| T_ERROR|T_DONE;
	cp->t_troub = code;
	wakeup(cp);
}

tiuchan(dev)
{
	register struct tiuch *cp;

	if ((cp = tiuptr(dev)) == NULL) {
		u.u_error = ENXIO;
		return(NULL);
	}
	if (cp->t_flags&(T_ERROR|T_EOF)) {
		if (cp->t_flags&T_ERROR)
			u.u_error = EIO;
		return(NULL);
	}
	return(cp);
}

tiustrategy(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	bp->av_forw = NULL;
	if (tiu.t_actf==NULL) {
		tiu.t_actf = bp;
		tiustart();
	} else
		tiu.t_actl->av_forw = bp;
	tiu.t_actl = bp;
}

snstat(dev, v)
{
	register struct tiu *cp;
	register int channo;

	channo = dev.d_minor;
	if ((cp = tiuptr(channo))==NULL)
		return;
	switch (u.u_arg[1]) {

	/* set signal byte */
	case 0:
		cp->t_osig = fubyte(u.u_arg[0]);
		return;

	/* get signal byte */
	case 1:
		suword(u.u_arg[0], cp->t_isig);
		cp->t_isig = 0;
		return;

	/* get channel # */
	case 2:
		suword(u.u_arg[0], channo);
		return;

	/* get trouble code */
	case 3:
		suword(u.u_arg[0], cp->t_troub);
		cp->t_troub = 0;
		cp->t_flags =& ~T_ERROR;
		return;

	/* clear EOF request */
	case 4:
		if (channo >= 64)
			cp = tiuptr(channo - 64);
		cp->t_flags =& ~T_EOF;
		return;

	/* set EOF request */
	case 5:
		if (channo >= 64)
			cp = tiuptr(channo - 64);
		cp->t_flags =| T_EOF;
		wakeup(cp);
		return;

	}
	u.u_error = EINVAL;
}

tiuptr(dev)
{
	register struct tiuch *cp;
	register int d;

	d = dev.d_minor & 0177;
	if (d<NCHAN)
		return(&tiu_dchan[d]);
	if (d>=64 && d<NCHAN+64)
		return(&tiu_cchan[d-64]);
	return(NULL);
}
