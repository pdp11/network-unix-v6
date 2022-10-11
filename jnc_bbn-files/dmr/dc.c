#
/*
 *  Written by John Lowry, 1975
 */

/*
 *   DC-11 driver
 */
#include "../h/param.h"
#include "../h/user.h"
#include "../h/old_tty.h"
#include "../h/proc.h"
#include "../h/conf.h"
#include "../h/buf.h"

/* base address */
#define	DCADDR	0174000
#define NDC11   1     /* was 14 */
#define DCPRI   5

/* Control bits */
#define	CDLEAD	01
#define	CARRIER	04
#define SPEED1  030   /* was 010 */
#define	STOP1	0400
#define READY   0200
#define	RQSEND	01
#define	PARITY	040
#define	ERROR	0100000
#define	CTRANS	040000
#define	RINGIND	020000
#define CTSEND  02
#define RCVFLG  02    /* same was WOPEN, which is not needed here */



struct dcregs {
	int dcrcsr;
	int dcrbuf;
	int dctcsr;
	int dctbuf;
};

#define PS 0177776
int ps;
struct
{       char *dc_buf;
	char *dc_bufp;
	int  dc_sent;
	int  dc_grabbed;
} dcb[4];       /* to be assigned system buffers */

struct
{       int dc_state;
	int dc_flags;
	int cur_buf;  /* which of dcb[*] has the last character */
	int dc_uid;
	char dc_poke;
	char dc_wakeup;
	int dc_nxmit;
} hdc11;

struct	tty dc11[NDC11];

int dcrstab[] {
	0,		/* 0 baud */
	0,		/* 50 baud */
	0,		/* 75 baud */
	0,		/* 110 baud */
	01101,		/* 134.5 baud: 7b/ch, speed 0 */
	0111,		/* 150 baud: 8b/ch, speed 1 */
	0,		/* 200 baud */
	0121,		/* 300 baud: 8b/ch, speed 2 */
	0,		/* 600 baud */
	0131,		/* 1200 baud */
	0,		/* 1800 baud */
	0,		/* 2400 baud */
	0,		/* 4800 baud */
	0,		/* 9600 baud */
	0,		/* X0 */
	0,		/* X1 */
};

int dctstab[] {
	0,		/* 0 baud */
	0,		/* 50 baud */
	0,		/* 75 baud */
	0,		/* 110 baud */
	0501,		/* 134.5 baud: stop 1 */
	0511,		/* 150 baud */
	0,		/* 200 baud */
	0521,		/* 300 baud */
	0,		/* 600 baud */
	0130,           /* 1200 baud:no RQSEND */
	0,		/* 1800 baud */
	0,		/* 2400 baud */
	0,		/* 4800 baud */
	0,		/* 9600 baud */
	0,		/* X0 */
	0,		/* X1 */
};

dcopen(dev, flag)
{
	register struct tty *rtp;
	register *addr;
	extern dcstart();
	int i;
	if (dev.d_minor > NDC11)     /* not >= since we got one and are */
	{       u.u_error = ENXIO;   /* using it two ways */
		return;
	}
	if (dev.d_minor == 1) /* half duplex */
	{
	    if ((hdc11.dc_state &ISOPEN ) || (rtp->t_state&ISOPEN))
	    {       u.u_error = EBUSY; /* must check both tables!!! */
		    return;
	    }

	    hdc11.dc_state = ISOPEN;
	    hdc11.dc_flags = EVENP;
	    hdc11.dc_uid = u.u_uid;

	    DCADDR -> dcrcsr = IENABLE|CDLEAD|SPEED1;
	    DCADDR -> dctcsr = IENABLE|SPEED1|RQSEND;
	    hdc11.cur_buf = 0;
	    hdc11.dc_poke = 0;
	    hdc11.dc_nxmit = 0;
	    for (i=0; i<4; i++)
		    dcb[i].dc_grabbed = 0;
	}
	else /* full duplex */
	{
	    rtp = &dc11[dev.d_minor];
	    rtp->t_addr = addr = DCADDR + dev.d_minor*8;
	    rtp->t_state =| WOPEN;
	    addr->dcrcsr =| IENABLE|CDLEAD;
	    if (((rtp->t_state&ISOPEN) == 0) && /* check both here, too */
			   ((hdc11.dc_state &ISOPEN) == 0))  {
		    rtp->t_erase = CERASE;
		    rtp->t_kill = CKILL;
		    addr->dcrcsr = IENABLE|CDLEAD|SPEED1;
		    addr->dctcsr = IENABLE|SPEED1|STOP1|RQSEND;
		    rtp->t_state = ISOPEN | WOPEN;
		    rtp->t_flags = ODDP|EVENP|ECHO;
	    }
	    if (addr->dcrcsr & CARRIER)
		    rtp->t_state =| CARR_ON;
	    while ((rtp->t_state & CARR_ON) == 0)
		    sleep(&rtp->t_rawq, TTIPRI);
	    ttyopen(dev, rtp);
	}
}

dcclose(dev)
{       register int i;
	register struct tty *tp;
	if (dev.d_minor == 1) /* half duplex */
	{
	    hdc11.dc_state = 0;
	    DCADDR -> dcrcsr = 0;
	    DCADDR -> dcrcsr = CDLEAD;
	    for (i=0; i<4; i++)
		    if (dcb[i].dc_grabbed) {
#ifdef BUFMOD
			    brelse(dcb[i].dc_buf,&bfreelist);
#endif
#ifndef BUFMOD
			    brelse(dcb[i].dc_buf);
#endif BUFMOD
			    dcb[i].dc_buf = 0;
		    }
	}
	else /* full duplex */
	{
	    (tp = &dc11[dev.d_minor])->t_state = 0;
	    if (tp->t_flags&HUPCL)
		    tp->t_addr->dcrcsr =& ~CDLEAD;
	    wflushtty(tp);
	}
}

dcwrite(dev)                 /* Write grabs the system buffers for read */
{       register int i;      /* It is done here because it's too late   */
	register char *bp;   /* to do it if you're in read already.     */
			     /* Write uses only one buffer for itself   */
			     /* since it's very unlikely a user will    */
			     /* send more than 50 chars--much less 512  */
			     /* (Above refers to half duplex operation) */
	if (dev.d_minor == 1) /* half duplex */
	{
	    if (u.u_count==0)
		    return;
	    for (i=0; i<4; i++)
		    if (dcb[i].dc_grabbed==0)
		    {       dcb[i].dc_buf = getblk(NODEV);
			    dcb[i].dc_bufp = dcb[i].dc_buf -> b_addr;
			    dcb[i].dc_sent = 0;
			    dcb[i].dc_grabbed = 1;
		    }
	    while (hdc11.dc_nxmit)
	    {       hdc11.dc_wakeup++;
		    sleep (&hdc11.dc_wakeup, DCPRI);
	    }
	    dcb[0].dc_bufp = bp = dcb[0].dc_buf->b_addr;
	    hdc11.dc_nxmit = u.u_count > 512 ? 512 : u.u_count;
	    iomove(dcb[0].dc_buf, 0, hdc11.dc_nxmit, B_WRITE);
	    dcstart();
	}
	else /* full duplex */
	ttwrite(&dc11[dev.d_minor]);
}



dcxint(dev)
{
	register struct tty *tp;
	if (dev.d_minor == 1) /* half duplex */
	{
	    if (hdc11.dc_state & RCVFLG) /* last dcstart was for an eot */
	    {       DCADDR -> dctcsr =& ~RQSEND;
		    hdc11.cur_buf = 0;
		    dcb[0].dc_bufp = dcb[0].dc_buf -> b_addr;
	    }
	    else
		    dcstart();
	}
	else /* full duplex */
	{
	    ttstart(tp = &dc11[dev.d_minor]);
	    if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc == TTLOWAT)
		    wakeup(&tp->t_outq);
	}
}

dcstart ()
{       register int c;
	extern char partab[];
	if ((DCADDR->dctcsr&READY) == 0 ||
		(hdc11.dc_state&(RCVFLG & ISOPEN) != ISOPEN)) return;

	if ((DCADDR->dctcsr & CTSEND) == 0)
	{       timeout (&dcstart, 0, 12);
		return;
	}

	if (--hdc11.dc_nxmit >= 0)
	{       ps = PS -> integ;
		spl5();
		c = (*dcb[0].dc_bufp++);
		DCADDR -> dctbuf = c =| (partab[c]&0200);
		if ((c & 0177) == 004)
			hdc11.dc_state =| RCVFLG;
		PS->integ = ps;
	}
	else
	{       hdc11.dc_nxmit = 0;
		if (hdc11.dc_wakeup)
		{       hdc11.dc_wakeup = 0;
			wakeup (&hdc11.dc_wakeup);
		}
	}
}


dcrint(dev)
{
	register struct tty *tp;
	register int c;         /* rint reads until it gets an <eot> or */
	register int csr;       /* until it runs out of buffer space.   */
				/* It tells read it's through by the    */
				/* wakeup on dc11.dc_poke.(It won't run */
				/* out of space if you grab 4 buffers.) */
				/* (Above refer to half duplex operation)*/

	if (dev.d_minor == 1) /* half duplex */
	{
	    c = DCADDR -> dcrbuf;
	    csr = DCADDR -> dcrcsr;
	    if ((csr & ERROR) == 0)     /* not ctrans, overrun, or ring */
	    {       csr =& PARITY;
		    if (csr&&(hdc11.dc_flags&ODDP) ||
			    !csr&&(hdc11.dc_flags&EVENP))
		    {       if (dcb[hdc11.cur_buf].dc_bufp <
					 dcb[hdc11.cur_buf].dc_buf->b_addr + 512)
			    {       *dcb[hdc11.cur_buf].dc_bufp++ = (c & 0177);
			    }
			    else if (hdc11.cur_buf < 3)
			    {       dcb[hdc11.cur_buf++].dc_sent = 1;
				    *dcb[hdc11.cur_buf].dc_bufp++ = (c & 0177);
				    ++hdc11.dc_poke;
				    wakeup (&hdc11.dc_poke);
			    }
			    else c = 004;

			    if ((c & 0177) == 004)  /* turn the line around */
			    {       DCADDR -> dctcsr =| RQSEND;
				    hdc11.dc_state =& ~RCVFLG;
				    dcb[hdc11.cur_buf].dc_sent = 1;
				    ++hdc11.dc_poke;
				    wakeup (&hdc11.dc_poke);
			    }
		    }
	    }
	}
	else /* full duplex */
	{

	    tp = &dc11[dev.d_minor];
	    c = tp->t_addr->dcrbuf;
	    /*
	     * If carrier is off, and an open is not in progress,
	     * knock down the CD lead to hang up the local dataset
	     * and signal a hangup.
	     */
	    if (((csr = tp->t_addr->dcrcsr) & CARRIER) == 0) {
		    if ((tp->t_state&WOPEN) == 0) {
			    tp->t_addr->dcrcsr =& ~CDLEAD;
			    if (tp->t_state & CARR_ON)
				    signal(tp, SIGHUP);
			    flushtty(tp);
		    }
		    tp->t_state =& ~CARR_ON;
		    return;
	    }
	    if (csr&ERROR || (tp->t_state&ISOPEN)==0) {
		    if (tp->t_state&WOPEN && csr&CARRIER)
			    tp->t_state =| CARR_ON;
		    wakeup(tp);
		    return;
	    }
	    csr =& PARITY;
	    if (csr&&(tp->t_flags&ODDP) || !csr&&(tp->t_flags&EVENP))
		    ttyinput(c, tp);
	}
}

dcread(dev)     /* read wakes up each time a system buffer becomes full */
{       register char *bp, **epp;
	int count;
	register int i;

	if (dev.d_minor == 1)  /* half duplex */
	{
	    while (hdc11.dc_poke == 0)
		    sleep (&hdc11.dc_poke, DCPRI);

	    for (i=0; i <= hdc11.cur_buf;i++)
		if (dcb[i].dc_sent==1)
		{   count = ((i==hdc11.cur_buf) ?
			    dcb[i].dc_bufp - (dcb[i].dc_buf->b_addr) :512);
		    iomove (dcb[i].dc_buf, 0, count, B_READ);
		    ++dcb[i].dc_sent;
		}
	    hdc11.dc_poke = 0;

	    if (!(hdc11.dc_state & RCVFLG))         /* xmit state; clean up */
		    for (i=0; i<4; i++) {          /* and release buffers  */
#ifdef BUFMOD
			    brelse(dcb[i].dc_buf,&bfreelist);
#endif
#ifndef BUFMOD
			    brelse(dcb[i].dc_buf); /* until the next write */
#endif BUFMOD
			    dcb[i].dc_buf = 0;     /* call grabs 'em again */
			    hdc11.cur_buf = 0;
			    dcb[i].dc_sent = 0;
			    dcb[i].dc_grabbed = 0;
		    }
	}
	else /* full duplex */
		ttread(&dc11[dev.d_minor]);
}


dcsgtty(dev, av)
int *av;
{
	register struct tty *tp;
	register r;
	if (dev.d_minor == 1)  /* half duplex */
	{       /* modes for nytib cemented in */
	}
	else /* full duplex */
	{
	    tp = &dc11[dev.d_minor];
	    if (ttystty(tp, av))
		    return;
	    if (r = dcrstab[tp->t_speeds.lobyte&017])
		    tp->t_addr->dcrcsr = r;
	    else
		    tp->t_addr->dcrcsr =& ~CDLEAD;
	    if (r = dctstab[tp->t_speeds.hibyte&017])
		    tp->t_addr->dctcsr = r;
	}
}
