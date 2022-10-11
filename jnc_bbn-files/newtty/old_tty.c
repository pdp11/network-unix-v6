#
/*
 * general TTY subroutines
 * Added ttycap (general tty capac routine) 9/28/78 Dan Franklin
 * Added XON/XOFF handling - 11/20/78 Alan Nemeth
 * Suppressed Rand's ctrl-P at 9600 baud when nl1,2,3 set. BBN(dan)
 *
 * Modified by BBN(dan) August 26, 1979: removed routines common
 * to both old and new tty driver generics, put in systty.c.
 *
 * This file should be archived AFTER all the drivers,
 * while the NEW tty generics should be archived BEFORE all
 * the drivers.
 */
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/tty.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/reg.h"
#include "../h/conf.h"

/* The entire file is #ifdef'ed on the absence of NEWTTY. */

#ifndef NEWTTY

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 */
char	maptab[]
{
	000,000,000,000,004,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,'|',000,'#',000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	'@',000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};

/*
 * The actual structure of a clist block manipulated by
 * getc and putc (mch.s)
 */
struct cblock {
	struct cblock *c_next;
	char info[6];
};

/* The character lists-- space for 6*NCLIST characters */
struct cblock cfree[NCLIST];
/* List head for unused character blocks. */
struct cblock *cfreelist;

/*
 * structure of device registers for KL, DL, and DC
 * interfaces-- more particularly, those for which the
 * SSTART bit is off and can be treated by general routines
 * (that is, not DH).
 */
struct {
	int ttrcsr;
	int ttrbuf;
	int tttcsr;
	int tttbuf;
};

/*
 * routine called on first teletype open.
 * establishes a process group for distribution
 * of quits and interrupts from the tty.
 *
 * Modified 12/29/77 to initialize the t_itp pointer (for await) (BBN: jfh)
 *
 */
ttyopen(dev, atp)
struct tty *atp;
{
	register struct proc *pp;
	register struct tty *tp;
	extern int pgrpct;

	pp = u.u_procp;
	tp = atp;
	if(pp->p_pgrp == 0) {
		pp->p_pgrp = ++pgrpct;
		u.u_ttyp = tp;
		u.u_ttyd = dev;
		tp->t_pgrp = pgrpct;
	}
	tp->t_itp = 0;		/* initialize to no awaiters (BBN: jfh) */
	tp->t_state =& ~WOPEN;
	tp->t_state =| ISOPEN;
}

/*
 * Wait for output to drain, then flush input waiting.
 */
wflushtty(atp)
struct tty *atp;
{
	register struct tty *tp;

	tp = atp;
	spl5();
	tp->t_state =& ~XOFFHNG;        /* agn - XON/XOFF */
	ttstart(tp);                    /* agn - XON/XOFF */
	while (tp->t_outq.c_cc) {
		tp->t_state =| ASLEEP;
		sleep(&tp->t_outq, TTOPRI);
	}
	flushtty(tp);
	spl0();
}

/*
 * flush all TTY queues
 */
flushtty(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int sps;

	tp = atp;
	while (getc(&tp->t_canq) >= 0);
	while (getc(&tp->t_outq) >= 0);
	wakeup(&tp->t_rawq);
	wakeup(&tp->t_outq);
	sps = PS->integ;
	spl5();
	while (getc(&tp->t_rawq) >= 0);
	tp->t_delct = 0;
	PS->integ = sps;
}

/*
 * transfer raw input list to canonical list,
 * doing erase-kill processing and handling escapes.
 * It waits until a full line has been typed in cooked mode,
 * or until any character has been typed in raw mode.
 */
canon(atp)
struct tty *atp;
{
	register char *bp;
	char *bp1;
	register struct tty *tp;
	register int c;
	int cnt;

	tp = atp;
	spl5();
	while (tp->t_delct==0) {
		if ((tp->t_state&CARR_ON)==0)
			return(0);
		sleep(&tp->t_rawq, TTIPRI);
	}
	spl0();
loop:
	bp = &canonb[2];
	while ((c=getc(&tp->t_rawq)) >= 0) {
		if (c==DELIMITER) {
			tp->t_delct--;
			/* so may receive more than one char in raw mode */
			if( tp->t_flags&RAW )
				continue;
			break;
		}
		if ((tp->t_flags&RAW)==0) {
			if (bp[-1]!='\\') {
                                if (c == tp->t_erase) {
					if (bp > &canonb[2])
						bp--;
					continue;
				}
				if (c==tp->t_kill)
					goto loop;
				if (c==CEOT)
					continue;
			} else
			if (maptab[c] && (maptab[c]==c || (tp->t_flags&LCASE))) {
				if (bp[-2] != '\\')
					c = maptab[c];
				bp--;
			}
		}
		*bp++ = c;
		if (bp>=canonb+CANBSIZ)
			break;
	}
	bp1 = bp;
	bp = &canonb[2];
	cnt = bp1 - bp;			/* calc num chars processed */
	c = &tp->t_canq;
	while (bp<bp1)
		putc(*bp++, c);
	return(cnt);
}

/*
 * Place a character on raw TTY input queue, putting in delimiters
 * and waking up top half as needed.
 * Also echo if required.
 * The arguments are the character and the appropriate
 * tty structure.
 *
 * Modified 12/28/77 to interface to awake (BBN: jfh)
 *
 */
ttyinput(ac, atp)
struct tty *atp;
{
	register int t_flags, c;
	register struct tty *tp;

	tp = atp;
	c = ac;
	t_flags = tp->t_flags;
	if ((c =& 0177) == '\r' && t_flags&CRMOD)
		c = '\n';
	if ((t_flags&RAW)==0 )
	  { if (c==CQUIT || c==CINTR) {
		signal(tp->t_pgrp, c==CINTR? SIGINT:SIGQIT);
		flushtty(tp);
		return;
		}
	    if ((t_flags&XONXOFF) == XONXOFF)           /* agn - XON/XOFF */
	      { if (c == CXOFF)                         /* agn - XON/XOFF */
		  { tp->t_state =| XOFFHNG;             /* agn - XON/XOFF */
		    return;                             /* agn - XON/XOFF */
		  }                                     /* agn - XON/XOFF */
		if (c == CXON)                          /* agn - XON/XOFF */
		  { tp->t_state =& ~XOFFHNG;            /* agn - XON/XOFF */
		    ttstart(tp);                        /* agn - XON/XOFF */
		    return;                             /* agn - XON/XOFF */
		  };                                    /* agn - XON/XOFF */
	      }                                         /* agn - XON/XOFF */
	  }
	if (tp->t_rawq.c_cc>=TTYHOG) {
		flushtty(tp);
		return;
	}
	if (t_flags&LCASE && c>='A' && c<='Z')
		c =+ 'a'-'A';
	putc(c, &tp->t_rawq);
	if (t_flags&RAW || c=='\n' || c==004) {
		wakeup(&tp->t_rawq);
		/* awaken process BBN(jfh) */
		if (tp->t_itp != 0) awake(tp->t_itp,0) ;
		if (putc(DELIMITER, &tp->t_rawq)==0)
			tp->t_delct++;
	}
	if ((t_flags&ECHO) && tp->t_speeds != ((4<<8)|4)) {
		ttyoutput(c, tp);
		ttstart(tp);
	}
}

/*
 * put character on TTY output queue, adding delays,
 * expanding tabs, and handling the CR/NL bit.
 * It is called both from the top half for output, and from
 * interrupt level for echoing.
 * The arguments are the character and the tty structure.
 */
ttyoutput(ac, tp)
struct tty *tp;
{
	register int c;
	register struct tty *rtp;
	register char *colp;
	int ctype;

	rtp = tp;
	c = ac&0177;
	/*
	 * Ignore EOT in normal mode to avoid hanging up
	 * certain terminals.
	 */
	/* The following two lines were commented out at Rand */

     /* if (c==004 && (rtp->t_flags&RAW)==0)    */
     /*         return;                         */
	/*
	 * Turn tabs to spaces as required
	 */
	if (c=='\t' && rtp->t_flags&XTABS) {
		do
			/* changed from ttyoutput call sfh */
			if( putc( ' ',&rtp->t_outq ) )
				return;
		while( ++rtp->t_col&07 );
		return;
	}
	/*
	 * for upper-case-only terminals,
	 * generate escapes.
	 */
	if (rtp->t_flags&LCASE) {
		colp = "({)}!|^~'`";
		while(*colp++)
			if(c == *colp++) {
				ttyoutput('\\', rtp);
				c = colp[-2];
				break;
			}
		if ('a'<=c && c<='z')
			c =+ 'A' - 'a';
	}
	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
	if (c=='\n' && rtp->t_flags&CRMOD)
		ttyoutput('\r', rtp);
	if (putc(c, &rtp->t_outq))
		return;
	/*
	 * Calculate delays.
	 * The numbers here represent clock ticks
	 * and are not necessarily optimal for all terminals.
	 * The delays are indicated by characters above 0200,
	 * thus (unfortunately) restricting the transmission
	 * path to 7 bits.
	 */
	colp = &rtp->t_col;
	ctype = partab[c];
	c = 0;

	switch (ctype&077) {

	/* ordinary */
	case 0:
		(*colp)++;

	/* non-printing */
	case 1:
		break;

	/* backspace */
	case 2:
		if (*colp)
			(*colp)--;
		break;

        /* newline */
        case 3:
                ctype = (rtp->t_flags >> 8) & 03;
                if (ctype == 0) {
                    if (rtp->t_speeds == ((13<<8)|13))
                            putc('\020', &rtp->t_outq);
                } else if(ctype == 1) { /* tty 37 */
                        if (*colp)
                                c = max((*colp>>4) + 3, 6);
                } else if (ctype == 2) { /* vt05 */
                        c = 6;
                } else if (ctype == 3) { /* ECD Translex */
                        c = 1;
                }
                *colp = 0;
                break;

        /* tab */
        case 4:
		ctype = (rtp->t_flags >> 10) & 03;
		if(ctype == 1) { /* tty 37 */
			c = 1 - (*colp | ~07);
			if(c < 5)
				c = 0;
		}
		*colp =| 07;
		(*colp)++;
		break;

	/* vertical motion */
	case 5:
		if(rtp->t_flags & VTDELAY) /* tty 37 */
			c = 0177;
		break;

	/* carriage return */
	case 6:
		ctype = (rtp->t_flags >> 12) & 03;
		if(ctype == 1) { /* tn 300 */
			c = 5;
		} else
		if(ctype == 2) { /* ti 700 */
			c = 10;
		}
		*colp = 0;
	}
	if(c)
		putc(c|0200, &rtp->t_outq);
}

/*
 * Restart typewriter output following a delay
 * timeout.
 * The name of the routine is passed to the timeout
 * subroutine and it is called during a clock interrupt.
 */
ttrstrt(atp)
{
	register struct tty *tp;

	tp = atp;
	tp->t_state =& ~TIMEOUT;
	ttstart(tp);
}

/*
 * Start output on the typewriter. It is used from the top half
 * after some characters have been put on the output queue,
 * from the interrupt routine to transmit the next
 * character, and after a timeout has finished.
 * If the SSTART bit is off for the tty the work is done here,
 * using the protocol of the single-line interfaces (KL, DL, DC);
 * otherwise the address word of the tty structure is
 * taken to be the name of the device-dependent startup routine.
 */
ttstart(atp)
struct tty *atp;
{
	register int *addr, c;
	register struct tty *tp;
	int sps;
	struct { int (*func)(); };

	tp = atp;
	sps = PS->integ;
	spl5();
	addr = tp->t_addr;
	if (tp->t_state&SSTART) {
		(*addr.func)(tp);
		goto out;
	}
	if ((addr->tttcsr&DONE)==0 ||
			tp->t_state&(TIMEOUT|XOFFHNG))  /* agn -XON/XOFF */
		goto out;
	if ((c=getc(&tp->t_outq)) >= 0) {
		if (c<=0177)
			addr->tttbuf = c | (partab[c]&0200);
		else {
			timeout(ttrstrt, tp, c&0177);
			tp->t_state =| TIMEOUT;
		}
	}
	out:
		PS->integ = sps;
}

/*
 * Called from device's read routine after it has
 * calculated the tty-structure given as argument.
 * The pc is backed up for the duration of this call.
 * In case of a caught interrupt, an RTI will re-execute.
 */
ttread(atp)
struct tty *atp;
{
	register struct tty *tp;
	register cnt;

	tp = atp;
	cnt = 0;
	if ((tp->t_state&CARR_ON)==0)
		goto out;
	if (tp->t_canq.c_cc || (cnt = canon(tp)))
		while (tp->t_canq.c_cc && passc(getc(&tp->t_canq))>=0);
		dpadd(u.u_tio,cnt);     /* rand:greep - tty accounting */
out:    return( cnt );
}

/*
 * Called from the device's write routine after it has
 * calculated the tty-structure given as argument.
 */
ttwrite(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int c;
	register int count;

	count = 0;
	tp = atp;
	if ((tp->t_state&CARR_ON)==0)
		return;
	while ((c=cpass())>=0) {
		spl5();
		count++;
		while (tp->t_outq.c_cc > TTHIWAT) {
			ttstart(tp);
			tp->t_state =| ASLEEP;
			sleep(&tp->t_outq, TTOPRI);
		}
		spl0();
		ttyoutput(c, tp);
	}
	ttstart(tp);
	dpadd(u.u_tio,count);   /* rand:greep - tty accounting */
}

/*
 * Common code for gtty and stty functions on typewriters.
 * If v is non-zero then gtty is being done and information is
 * passed back therein;
 * if it is zero stty is being done and the input information is in the
 * u_arg array.
 */
ttystty(atp, av)
int *atp, *av;
{
	register  *tp, *v;

	tp = atp;
	if(v = av) {
		*v++ = tp->t_speeds;
		v->lobyte = tp->t_erase;
		v->hibyte = tp->t_kill;
		v[1] = tp->t_flags;
		return(1);
	}
	wflushtty(tp);
	v = u.u_arg;
	tp->t_speeds = *v++;
	tp->t_erase = v->lobyte;
	tp->t_kill = v->hibyte;
	tp->t_flags = v[1];
	return(0);
}

/* -------------------------- T T Y C A P ---------------------- */

/*
 * Note: this routine was generated by tracing the behavior of ttread and
 * ttwrite under all possible conditions. Hence it is useful for any char
 * mode device which uses them.
 * Originally coded Sept 27 1978 Dan Franklin @ BBN
 */

ttycap(atp, v)
   struct tty *atp;
   int *v;
   {
   register struct tty *tp;

   tp = atp;
   if ((tp->t_state & CARR_ON) == 0) /* Carrier gone? */
      {
      *v++ = -1;
      *v = -1;
      return;  /* That's all, folks. */
      }

   if (tp->t_canq.c_cc)
      *v = tp->t_canq.c_cc;
   else if (tp->t_delct == 0)
      *v = 0;
   else
/*
 * This is count of all available data.
 * (rawqsize minus the DELIMITERs put in by ttyinput.)
 * Less than this may be returned, depending on canonicalization
 */
      *v = tp->t_rawq.c_cc - tp->t_delct;

   v++;

   if (tp->t_outq.c_cc > TTHIWAT) /* Write will definitely block */
      *v = 0;
   else
/*
 * This is how much room is actually left before ttwrite blocks.
 * If tabs are being expanded, or padding put out, or LF mapped to
 * CR-LF, or chars echoed, etc., a write of this value will block.
 */
      *v = TTHIWAT - tp->t_outq.c_cc + 1;
   }

#endif
