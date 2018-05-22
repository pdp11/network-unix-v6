#
/*
 */

/*
 * general TTY subroutines
 */
#include "/usr/sys/h/param.h"
#include "/usr/sys/h/systm.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/tty1.h"
#include "/usr/sys/h/proc.h"
#include "/usr/sys/h/inode.h"
#include "/usr/sys/h/file.h"
#include "/usr/sys/h/reg.h"
#include "/usr/sys/h/conf.h"

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 */
char	maptab[];

/*
 * character map table for
 * ibm 2741 output
 */
char	xascii[];

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
	extern char mapibm[];
	int ctype;

	rtp = tp;
	c = ac&0177;
	/*
	 * binary output mode patch
	 * ignores everything, including timeouts
	 */
	if (rtp->t_speeds & BINARY) {
		putc(ac,&rtp->t_outq);
		return;
	}
	/*
	 * the loop around trick:  
	 * if the LOOP bit is set and 0200 of the data is set
	 * slough output back to the input channel
	 */
	if (rtp->t_speeds & LOOP && (ac&0200)) {
		putc(ac,&rtp->t_rawq);
		putc(0376,&rtp->t_rawq);
		rtp->t_delct++;
		wakeup(&rtp->t_rawq);
		return;
	}
	/*
	 * 2741 output translation
	 * convert to 6-bit code, merge with
	 * delay and column counting code
	 */
#ifdef	IBM
	if ((rtp->t_speeds&017) == IBM) {
		/*
		 * simulate carriage return for 2741's
		 * message is printed if outq should fill up
		 */
		if (c == '\r') {
			colp = &rtp->t_col;
			while(*colp) {
				if(putc(035,&rtp->t_outq)) {
					printf("ibmoq\n");
				}
				(*colp) --;
			}
		goto out;
		}
		/*
		 * Pick up 6-bit code.
		 * case indicated by upper two bits.
		 */
		c = xascii[c];
		if (!(c&CASE)) {		/* no case bits on escaped chars */
			ttyoutput('\\',tp);
			ttyoutput(mapibm[c],tp);
			return;
		}
		if (!(rtp->ibm & CASE & c))	/* shift case if bits don't agree */
			shiftcase(rtp);
		c =& 077;			/* strip off case bits */
		putc(c,&rtp->t_outq);		/* send char to driver */
		c = ac & 0177;			/* recover ASCII to do delays */
		goto out;
	}
#endif	IBM
	/*
	 * Ignore EOT in normal mode to avoid hanging up
	 * certain terminals.
	 */
	if (c==004 && (rtp->t_flags&RAW)==0)
		return;
	/*
	 * Turn tabs to spaces as required
	 * If anyone ever turns on tab expansion with
	 * a 2741 this code will have to be changed
	 * back to calling ttyoutput instead of directly
	 * accessing the outq as it now does.
	 */
	if (c=='\t' && rtp->t_flags&XTABS) {
#ifdef	COMPUTEK
		if (rtp->t_speeds&COMPUTEK) {
			do {
			   ttyoutput(' ',rtp);
			} while (rtp->t_col&07);
		}
		else
#endif	COMPUTEK
		do {
			if(putc( ' ', &rtp->t_outq ))
				return;
		} while( ++rtp->t_col&07 );
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
		if( 'A' <= c && c <='Z' && (rtp->t_flags & SEEMAP) )
			ttyoutput( '\\',rtp );
	}
	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
	if( c == '\n' && (rtp->t_flags&CRMOD) )
		ttyoutput('\r', rtp);
	if (rtp->t_speeds&INFOTON) {
		if (c=='~') {
			ttyoutput('\\',rtp);
			ttyoutput('^',rtp);
			return;
		}
	}
#ifdef	COMPUTEK
	if (rtp->t_speeds&COMPUTEK) {
		if (c == 0137) {
			ttyoutput(' ',rtp);
			--rtp->t_col;
		}
		putc(0,&rtp->t_outq);
		putc(0,&rtp->t_outq);
		putc(0,&rtp->t_outq);
	}
#endif	COMPUTEK
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
out:
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
		if(ctype == 1) { /* tty 37 */
			if( *colp )
				c = max(( *colp>>4 ) + 3,6 );
		} else
		if(ctype == 2) { /* vt05 */
			c = 6;
		}
#ifdef	IBM
		if ((tp->t_speeds&017) == IBM) {
			c = (*colp/10 +1)*7;
		}
#endif	IBM
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
#ifdef	IBM
		if ((tp->t_speeds&017) == IBM)
			c = 4;
#endif	IBM
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
	/*
	 * put out the delay.
	 * because delays >60 are converted into break
	 * signals for the 2741 (by the dh driver),
	 * timeouts > 60 must be done in pieces (sorry)
	 */
	if(c)
		if (c<60)
			putc(c|0200,&rtp->t_outq);
		else {
			while (c>=60) {
				c=- 59;
				putc(59|0200,&rtp->t_outq);
			}
			if (c) putc(c|0200,&rtp->t_outq);
		}
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
	if ((addr->tttcsr&DONE)==0 || tp->t_state&TIMEOUT)
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
 * called from device driver when it has no more
 * output characters.  used to synch half-duplex
 * terminals.
 */
ttstop(atp)
struct tty *atp;
{
register struct tty *tp;

	tp = atp;
#ifdef	IBM
	if ((tp->t_speeds&017) == IBM) {
		if(!(tp->ibm&LOCK)) {
			tp->ibm=& ~ CHANGE;
			return;
		}
		if(tp->ibm & CHANGE) {
			return;
		}
		if (tp->ibm & WANT) {
			return;
		}
		tp->ibm =| CHANGE;
		putc(CRC,&tp->t_outq);
		putc(0214,&tp->t_outq);
		ttstart(tp);
	}
#endif	IBM
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
	register int c;

	tp = atp;
	cnt = 0;
	if ((tp->t_state&CARR_ON)==0)
		return( 0 );
	if (tp->t_canq.c_cc || canon(tp)) {
		while (tp->t_canq.c_cc && passc(c=getc(&tp->t_canq))>=0) {
			cnt++;
		}
	}
	return( cnt );
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

	tp = atp;
	if ((tp->t_state&CARR_ON)==0)
		return;
	/*
	 * Output is held up if BLOCK bit is set and
	 * tty is not in raw mode.
	 */
	if (tp->t_speeds&BLOCK && !(tp->t_flags&RAW)) {
		sleep(&tp->t_speeds,TTOPRI);
	}
	/*
	 * On 2741's the output side must check to see
	 * if the keyboard is locked and lock it if it
	 * isn't.
	 */
#ifdef	IBM
	if ((tp->t_speeds&017) == IBM) {
	   tp->ibm =| WANT;
	   if (tp->ibm & CHANGE) {
		sleep(&tp->ibm,TTOPRI);
	   }
	   if (!(tp->ibm & LOCK)) {
		putc(0326,&tp->t_outq);		/* send a break signal */
		putc(0212,&tp->t_outq);		/* delay while gears turn */
		putc(CRD,&tp->t_outq);		/* grab line by sending CRD */
		putc(LC,&tp->t_outq);		/* force lower case */
		tp->ibm =& ~ CASE;
		tp->ibm =| 0100;
		tp->ibm =| LOCK;
	   }
	}
#endif	IBM
	/*
	 * copy chars from user to ttyoutput.
	 * hold up if output blocks and it's not a 2741
	 */
	while ((c=cpass())>=0) {
		spl5();
		if (tp->t_speeds&BLOCK
#ifdef	IBM
		&& (tp->t_speeds&017)!=IBM
#endif	IBM
		&& !(tp->t_flags&RAW)
		) {
			sleep(&tp->t_speeds,TTOPRI);
		}
		while (tp->t_outq.c_cc > TTHIWAT) {
			ttstart(tp);
			tp->t_state =| ASLEEP;
			sleep(&tp->t_outq, TTOPRI);
		}
		spl0();
		ttyoutput(c, tp);
	}
	ttstart(tp);
#ifdef	IBM
	if((tp->t_speeds&017) == IBM) {
		tp->ibm =& ~  WANT;
	}
#endif	IBM
}

/*
 * Common code for gtty and stty functions on typewriters.
 * If v is non-zero then gtty is being done and information is
 * passed back therein;
 * if it is zero stty is being done and the input information is in the
 * u_arg array.
 * Since the erase char is hard-coded for 2741's,
 * access to the programmable erase char has been
 * perverted so the 2741 code can use this byte for
 * state bits. This is also utilized when a 2741 dials
 * in because getty will do a stty call before data
 * gets transfered.  However, the open routines will
 * stick CERASE in the erase byte.  This is noticed
 * and changed to ULOCK which organizes the 2741
 * stuff properly.
 */
ttystty(atp, av)
int *atp, *av;
{
	register  *tp, *v;

	tp = atp;
	if(v = av) {
		/*
		 * J.S.Kravitz 18Mar78 Don't let user see internal
		 * state bits... hi might try to send them back to us !
		 * (it actually happened, blocking output ! )
		 */
		*v++ = tp->t_speeds & ~ (CDUMP & BLOCK);
		v->lobyte = CERASE;
#ifdef	IBM
		if ((tp->t_speeds&017) == IBM) {
			v->lobyte = IBMERASE;
		}
		else
#endif	IBM
			v->lobyte = tp->t_erase;
		v->hibyte = tp->t_kill;
		v[1] = tp->t_flags;
		/* Temporary rand change for empty */
		u.u_arg[4] = (tp->t_canq.c_cc==0 &&
			tp->t_delct==0 && tp->t_state&CARR_ON);
		return(1);
	}
	wflushtty(tp);
	v = u.u_arg;
	/*
	 * J.S.Kravitz 18Mar78 Time was that if a user's output was blocked
	 * when he did the gtty, a subsequent stty with the same speedword
	 * would re-block output. We don't send that bit up to the user
	 * anymore, but just to protect a user from his own stupidity:
	 */
	tp->t_speeds = (*v++ & ~ (BLOCK|CDUMP));
#ifdef	IBM
	if ((tp->t_speeds&017) == IBM)  {
		if (tp->ibm == CERASE) {
			tp->ibm = ULOCK;
		}
	}
	else
#endif	IBM
		tp->t_erase = v->lobyte;
	tp->t_kill = v->hibyte;
	tp->t_flags = v[1];
	return(0);
}

#ifdef	IBM
/*
 * invert the current case of a 2741
 */
shiftcase(atp)
{
register struct tty *tp;
register int c,s;

	tp=atp;
	c = s = tp->ibm;
	s =& ~ CASE;
	if (c & 0100) {
		putc(UC,&tp->t_outq);
		s =| 0200;
	}
	else {
		putc(LC,&tp->t_outq);
		s =| 0100;
	}
	tp->ibm = s;
}
#endif	IBM
