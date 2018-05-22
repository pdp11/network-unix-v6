#
/* Revision - New handshake handler */

/*
 * general TTY subroutines
 */
#include "param.h"
#include "systm.h"
#include "user.h"
/*#include "userx.h"*/
#include "tty.h"
#include "proc.h"
/*#include "procx.h"*/
#include "inode.h"
/*#include "inodex.h"*/
#include "file.h"
/*#include "filex.h"*/
#include "reg.h"
#include "conf.h"

#define	DLDELAY	4

/*
 * The actual structure of a clist block manipulated by
 * getc and putc (mch.s)
 */
struct cblock {
	struct cblock *c_next;
	char info[6];
};
/* Structure to select one char out of queue */
struct {
	char	q_char;
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
 */
ttyopen(dev, atp)
struct tty *atp;
{
	register struct proc *pp;
	register struct tty *tp;

	pp = u.u_procp;
	tp = atp;
	if(pp->p_pgrp == 0) {
		pp->p_pgrp = pp->p_pid;
		u.u_ttyp = tp;
		u.u_ttyd = dev;
		tp->t_pgrp = pp->p_pid;
	}

	if ((tp->t_state & ISOPEN) == 0)
	    {
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
		tp->t_quit = CQUIT;
		tp->t_intr = CINTR;
		tp->t_EOT = CEOT;

		tp->t_asize = TTACK;
		tp->t_enq = CENQ;
		tp->t_ack = CACK;
		tp->t_esc = CESC;
		tp->t_cntla = CSPEC;
		tp->t_freeze = CFREEZE;
		tp->t_thaw = CTHAW;
		tp->t_hold = CHOLD;
		tp->t_release = CREL;
		tp->t_retype = CRETYPE;
		tp->t_clear = CCLEAR;
		tp->t_silent = CSILENT;
		tp->t_atime = 60;
		tp->t_alwat = (tp->t_asize&0377) >> 1;
	    }

	tp->t_state =& ~WOPEN;
	tp->t_state =| ISOPEN;

}


/*
 * The routine implementing the gtty system call.
 * Just call lower level routine and pass back values.
 */
gtty()
{
	int v[3];
	register *up, *vp;

	vp = v;
	sgtty(vp);
	if (u.u_error)
		return;
	up = u.u_arg[0];
	suword(up, *vp++);
	suword(++up, *vp++);
	suword(++up, *vp++);
}

/*
 * The routine implementing the stty system call.
 * Read in values and call lower level.
 */
stty()
{
	register int *up;

	up = u.u_arg[0];
	u.u_arg[0] = fuword(up);
	u.u_arg[1] = fuword(++up);
	u.u_arg[2] = fuword(++up);
	sgtty(0);
}


/*
 * Stuff common to stty and gtty.
 * Check legality and switch out to individual
 * device routine.
 * v  is 0 for stty; the parameters are taken from u.u_arg[].
 * c  is non-zero for gtty and is the place in which the device
 * routines place their information.
 */
sgtty(v)
int *v;
{
	register struct file *fp;
	register struct inode *ip;

	if ((fp = getf(u.u_ar0[R0])) == NULL)
		return;
	ip = fp->f_inode;
	if ((ip->i_mode&IFMT) != IFCHR) {
		u.u_error = ENOTTY;
		return;
	}
	(*cdevsw[ip->i_addr[0].d_major].d_sgtty)(ip->i_addr[0], v);
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
	while (tp->t_outq.c_cc) {
		tp->t_state =| ASLEEP;
		sleep(&tp->t_outq, TTOPRI);
	}
	flushtty(tp);
	spl0();
}


/*
 * Initialize clist by freeing all character blocks, then count
 * number of character devices. (Once-only routine)
 */
cinit()
{
	register int ccp;
	register struct cblock *cp;
	register struct cdevsw *cdp;

	ccp = cfree;
	for (cp=(ccp+07)&~07; cp <= &cfree[NCLIST-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
	}
	ccp = 0;
	for(cdp = cdevsw; cdp->d_open; cdp++)
		ccp++;
	nchrdev = ccp;
}


/*
 * flush all TTY queues
 */
flushtty(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int sps;

	sps = PS->integ;
	spl5();
	tp = atp;
	while (getc(&tp->t_canq) >= 0);
	while (getc(&tp->t_outq) >= 0);
	wakeup(&tp->t_canq);
	wakeup(&tp->t_outq);
	PS->integ = sps;
}



/*
 * Place a character on can TTY input queue, putting in delimiters
 * and waking up top half as needed.
 * Also echo if required.
 * The arguments are the character and the appropriate
 * tty structure.
 */

/* Echo places a character on the appropriate tty's output queue, and starts
 * it up. Also, some processing to output certain control characters is done */

echo(ac,atp)
register struct tty *atp;
{
	if (ac < 0 || ((atp->t_flags & ECHO) == 0)) return;
	if (ac <= 037 && (ac < 07 || ac > 012))
	    {
		echo ('^',atp);
		ac =+ 0100;
	    }
	ttyoutput(ac,atp);
	ttstart(atp);
}

echostr(s,tp)
register char *s;
register struct tty *tp;
{
	if ((tp->t_flags & ECHO) == 0) return;
	while(*s) putc(*s++,&tp->t_outq);
	ttstart(tp);
}

ttcursor(x,y,tp)
register struct tty *tp;
{
	if ((tp->t_flags & ECHO) == 0) return;
	echostr("\033Y",tp);		/* escape sequence */
	putc(x+040,&tp->t_outq);	/* row */
	putc(y+040,&tp->t_outq);	/* col */
	putc(0201,&tp->t_outq);		/* vt52 delay */
	ttstart(tp);
	tp->t_col = y;
}

ttretype(tp)
register struct tty *tp;
{
	register cptr;

	cptr = tp->t_llf;		/* start from end of last line */

/*	if ((cptr = tp->t_llf) == 0)
		cptr = tp->t_canq.c_cf; */

	if (tp->t_canq.c_cc)
		while (cptr != tp->t_canq.c_cl) {
			if ((cptr & 07) == 0)
				if ( (cptr = &((cptr - 010)->c_next->info[0]))
					== tp->t_canq.c_cl )
					break;
			echo(cptr->q_char,tp);
			cptr++;
		}
}

char *tterase "\b \b";
char *ttclear "\033H\033J";
char *ttforw "\033C";
char *ttkill "\033K";

ttyinput(ac,atp)
struct tty *atp;
{
	register int t_flags, c;
	register struct tty *tp;
	int	cptr;
	int	cano;			/* type of terminal */

/*	tk_nin =+ 1;*/
	tp = atp; c=ac&0377; t_flags = tp->t_flags;	/* Load registers */

	if ((tp->t_flag2 & LITIN) == 0) c =& 0177;

	if (tp->t_flag2 & (RACK | WACK))
	  {	if ((tp->t_flag2 & WACK) && (c == (tp->t_ack & 0377)))
		    {
			tp->t_acnt = tp->t_asize;
			wakeup(&tp->t_acnt);
			return;
		    }

		if ((tp->t_flag2 & RACK) && (c == (tp->t_enq & 0377)))
		    {
			if ((tp->t_canq.c_cc <= tp->t_alwat) || /* ack if possible */
			  (tp->t_llf == tp->t_canq.c_cf))
			    {
				putc(tp->t_ack,&tp->t_outq);
				ttstart(tp);
			    }
			else
				tp->t_state =| ENQRCV;
			return;
		    }

		if (tp->t_state & ESCRCV)
		    {
			tp->t_state =& ~ESCRCV;
			switch(c)
			    {
				case CESC1:	c = tp->t_enq; break;
				case CESC2:	c = tp->t_ack; break;
			    }
		    }
		else if (c == (tp->t_esc & 0377))
		    {
			tp->t_state =| ESCRCV;
			return;
		    }
	  }
	if ((tp->t_flag2 & WHOLD) && !(tp->t_state & HELD)
		&& (tp->t_canq.c_cc >= tp->t_asize))
	    {						/* send a HOLD character */
		tp->t_state =| HELD;
		putc(tp->t_hold, &tp->t_outq);
		ttstart(tp);
	    }
	if (tp->t_flag2 & LITIN)			/* Literal input */
	    {
		if (tp->t_canq.c_cc >= TTYHOG) return;
		putc(c,&tp->t_canq);
		tp->t_llf = tp->t_canq.c_cl;		/* set llf */
		wakeup(&tp->t_canq);			/* wake up!! */
		return;
	    }

	if (t_flags & CRMOD)
		if (c == '\r') c = '\n';
		else if (c == '\n') c = '\r';

	if (((c==tp->t_intr) || (c==tp->t_quit) || (c==tp->t_cntla))
		&& ((tp->t_flag2 & EEI) || ((t_flags & RAW) == 0)))
	    {
		if (c == tp->t_intr)
			signal (tp->t_pgrp,SIGINT);
		else
			signal (tp->t_pgrp, c==tp->t_quit?SIGQIT:SIGSPC);
		flushtty (tp);
		tp->t_state =& ~FROZEN;			/* signals thaw tty */
		echo(c,tp);
		echo('\n',tp);
		return;
	    }

	if (tp->t_canq.c_cc >= TTYHOG) {
		if (t_flags & RAW) return;
		if ((c != tp->t_erase) && (c != tp->t_kill)) return;
	}

	switch(tp->t_cano) {
		case TC_GT40:
		case TC_TERM:
		case TC_VT52:
			cano = 1;
			break;
		default:
			cano = 0;
			break;
	}
	if (t_flags & LCASE && c >= 'A' && c <= 'Z')	/* Convert to lower case if necessary */
		c =+ 'a' - 'A' ;
	if (t_flags & RAW)
	    {
		putc(c,&tp->t_canq);
	    }
	else if (c == tp->t_erase)
	    {
		c = -1;
		if ((tp->t_llf != tp->t_canq.c_cl ) &&
		    ((c = unputc(&tp->t_canq)) >= 0))
			if (cano) {	/* special terminals */
				switch (partab[c] & 077) {
				case 1:		/* non - printing */
					if (c <= 012 && c >= 07) break;
				case 5:
				case 6:	
					echostr(tterase,tp);
					tp->t_col--;
				case 0:		/* normal */
					echostr(tterase,tp);
					tp->t_col--;
					break;
				case 2:
					echostr(ttforw,tp);
					tp->t_col++;
					break;
				case 4:
					ttcursor(0136,tp->t_delpos,tp);
					ttretype(tp);
					break;
				}
			c = -1;
					
			/* normal printing terminal delete */
			} else
				if ((tp->t_state & ERMODE) == 0)
				    {
					echo('\\',tp);
					tp->t_state =| ERMODE;
				    }
	    }
	else
	    {
		if (tp->t_state & ERMODE)		/* If in erase mode, */
		    {
			echo('\\',tp);			/* echo '\' to indicate out of erase mode */
			tp->t_state =& ~ERMODE;		/* and turn it off */
		    }
		if (c == tp->t_retype)
		    {
			if (cano) ttcursor(0136,tp->t_delpos,tp);
			else {
				echo(c,tp);
				echo('\n',tp);
			}
			ttretype(tp);
			c = -1;
		    }
		else if (c == tp->t_kill)
		    {
			if (cano) {
				ttcursor(0136,tp->t_delpos,tp);
				echostr(ttkill,tp);
			} else {
				echo (c,atp);
				echo('\n',tp);
			}
			while ((tp->t_llf != tp->t_canq.c_cl) &&
				(unputc(&tp->t_canq) > 0)) ;	/* flush till empty or end-of-line */
			c = -1;
		    }
		else if (c == tp->t_EOT)		/* End of file indicator */
		    {
			putc(c,&tp->t_canq);
			c = -2;
		    }
		else if (c == tp->t_silent)
		    {
			flushtty(tp);
			echo (c,tp); echo ('\n',tp); echo ('\n',tp);
			if (tp->t_flag2 & SILENT)
				tp->t_flag2 =& ~SILENT;
			else tp->t_flag2 =| SILENT;
		    }
		else if (c == tp->t_clear)
		    {
			if (cano) echostr(ttclear,tp);
			else {
				echo(c,tp);
				echo('\n',tp);
			}
			ttretype(tp);
			tp->t_delpos = 0;
			c = -1;
		    }
		else if (c == tp->t_freeze)
		  {	tp->t_state =| FROZEN;		/* freeze the screen */
			return;
		  }
		else if (c == tp->t_thaw)
		  {	tp->t_state =& ~FROZEN;		/* wakeup output   */
			wakeup(&tp->t_outq);
			return;
		  }
		else
			putc(c,&tp->t_canq);		/* Anything else goes straight to can queue */
	    }

	echo(c,tp);					/* Echo the character */
	if (tp->t_canq.c_cc == 1 || (tp->t_llf&07) == 0)
		tp->t_llf = tp->t_canq.c_cf;		/* Initialize t_llf */

	if (t_flags & RAW || c == '\n' || c == -2)	/* If line delimiter, wake up canan() */
	    {
		tp->t_delpos = 0;			/* for kill processing */
		tp->t_llf = tp->t_canq.c_cl;
		wakeup(&tp->t_canq);
	    }
	return;						/* (Implied return anyway) */
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
	int ctype,sps;

/*	tk_nout =+ 1;*/
	rtp = tp;
	c = ac;
	if (rtp->t_flag2 & SILENT) return(0);

	if ((rtp->t_flag2 & (RACK | WACK))
		&& ((c == tp->t_enq) || (c == tp->t_ack) ||
		  (c == tp->t_esc)))
		    {
			if (putc(rtp->t_esc,&rtp->t_outq)) return(1);
			if (c == rtp->t_enq) c = CESC1;
			else if (c == rtp->t_ack) c = CESC2;
		    }
			

	if (rtp->t_flag2 & LITOUT)			/* Literal output */
	    {
		if (putc(c, &rtp->t_outq))
			return(1);
		else
		     {
			if (rtp->t_acnt) rtp->t_acnt--;
			return(0);
		     }
	    }
	c =& 0177;
	/*
	 * Ignore EOT in normal mode to avoid hanging up
	 * certain terminals.
	 */
	if (c==004 && (rtp->t_flags&(RAW|WACK|RACK))==0)
		return(0);
	/*
	 * Turn tabs to spaces as required
	 */
	if (c=='\t' && rtp->t_flags&XTABS) {
		do
			if (ttyoutput(' ', rtp)) return(1);
		while (rtp->t_col&07);
		return(0);
	}
	/*
	 * for upper-case-only terminals,
	 * generate escapes.
	 */
	if (rtp->t_flags&LCASE) {
		colp = "({)}!|^~'`";
		while(*colp++)
			if(c == *colp++) {
				if (ttyoutput('\\', rtp)) return(1);
				c = colp[-2];
				break;
			}
		if ('a'<=c && c<='z')
			c =+ 'A' - 'a';
	}
	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
	if (c=='\n' && rtp->t_flags&CRMOD)	/* added CRMOD test - KLH 6/19/79 */
		 if (ttyoutput('\r', rtp)) return(1);

	if (putc(c, &rtp->t_outq)) return(1);	/* put char in buffer */
	if (rtp->t_acnt) rtp->t_acnt--;

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
		if(ctype == 1) { /* tty 37 */
			if (*colp)
				c = max((*colp>>4) + 3, 6);
		} else
		if(ctype == 2) { /* vt05 */
			c = 6;
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
	if(c) putc(c|0200, &rtp->t_outq);  /* put in delay unless zero */
	return(0);	/* we always return 0 here because if inserting
				the delay char fails, we can just ignore. */
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
	int sps;
	register struct tty *tp;
	struct { int (*func)(); };

	tp = atp;
	addr = tp->t_addr;
	if (tp->t_state&SSTART) {
		(*addr.func)(tp);
		return;
	}
	sps = PS->integ;
	spl5();
	if ((addr->tttcsr&DONE)==0 || tp->t_state&TIMEOUT)
		return;
	if ((c=getc(&tp->t_outq)) >= 0) {
		if (tp->t_flag2 & LITOUT)			/* Literal output */
			addr->tttbuf = c;
		else if (c<=0177)
			addr->tttbuf = c | (partab[c]&0200);
		else {
			timeout(ttrstrt, tp, (c&0177) + DLDELAY);
			tp->t_state =| TIMEOUT;
		}
	}
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
	register int c;

	tp = atp;
	tp->t_flag2 =& ~SILENT;
	spl5();
	while ((tp->t_canq.c_cc == 0) || (tp->t_llf == tp->t_canq.c_cf))
	    {
		if ((tp->t_state & CARR_ON) == 0)
			return;
		sleep(&tp->t_canq,TTIPRI);
	    }
	spl0();
	while ((tp->t_canq.c_cf) && (tp->t_llf != tp->t_canq.c_cf))
	    {
		c = getc (&tp->t_canq);
		if (tp->t_canq.c_cc <= tp->t_alwat)
		    {
			if ((tp->t_flag2 & RACK) && (tp->t_state & ENQRCV))
			    {
				tp->t_state =& ~ENQRCV;
				while (putc(tp->t_ack, &tp->t_outq)) sleep(&lbolt,22);
				ttstart(tp);
			    }

			if ((tp->t_flag2 & WHOLD) && (tp->t_state & HELD))
			    {
				tp ->t_state =& ~HELD;
				while (putc(tp->t_release, &tp->t_outq)) sleep(&lbolt,22);
				ttstart(tp);
			    }
		    }
		if (tp->t_flags & RAW)
		    {
			 /* Changed 6/19/79 KLH to permit
			reading more than one char in
			raw mode. */
			if(passc(c) < 0) break;
				else continue;
/*			passc(c);	*/
/*			break;	*/
		    }
		if ((c == tp->t_EOT) || passc(c) < 0 || c == '\n')
			break;
	    }
}

/*
 * wakes up process waiting for an acknowledge if the
 * ACKWAIT bit in t_state is still set.
 */
ttywakeup(atp)
register struct tty *atp;
{
	if (atp->t_state & ACKWAIT)
	     {
		atp->t_state =& ~ACKWAIT;
		wakeup(&atp->t_acnt);
	     }
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
	while ((c=cpass())>=0) {
		spl5();
		while(tp->t_state & FROZEN)
			sleep(&tp->t_outq, TTOPRI);	/* wait for thaw  */
		while (tp->t_outq.c_cc > TTHIWAT) {
			ttstart(tp);
			tp->t_state =| ASLEEP;
			sleep(&tp->t_outq, TTOPRI);
		}
		while((tp->t_flag2 & WACK) && (tp->t_acnt == 0))
		     {
			while (putc(tp->t_enq, &tp->t_outq)) sleep(&lbolt,22);
			ttstart(tp);
			if ((tp->t_state & ACKWAIT) == 0)
			     {
				timeout(ttywakeup,tp,tp->t_atime);
				tp->t_state =| ACKWAIT;
			     }
			sleep(&tp->t_acnt,TTOPRI);
		     }
		spl0();

		while (ttyoutput(c, tp)) sleep(&lbolt, 21);
	}
	tp->t_delpos = tp->t_col;
	ttstart(tp);
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


/* common code for ttymode handlers */

ttmode(atp,acp)
int *acp;
char atp[];

/* Rather than considering atp as a pointer to a tty structure, it is treated
 * as an array of bytes, so that the individual bytes can be referenced as an
 * offset from the base, rather than being individually selected via ->.
 */

{
	register bn,bits;
	register int *cp;

	cp = acp;
	bits = (*cp) & TBITS;			/* extract argument bits
*/
	if ((bn = (((*cp)&TBYTNUM)>>8)) > TMAXBN)	/* extract byte number, and check for validity */
	    {
		u.u_error = EINVAL;		/* invalid argument */
		return(1);
	    }

	if ((*cp) & TFLUSH) wflushtty(atp);	/* request to flush buffer first*/

	*cp = ((*cp) & 0177400) + (atp[TFIXOFS+bn] & 0377);	/* current value placed in *cp */
	switch((*cp) & TCNTL)				/* control bits */
	    {
		case TCLEAR:				/* bit clear */
			atp[TFIXOFS + bn] =& ~bits;
			break;
		case TSET:				/* bit set */
			atp[TFIXOFS + bn] =| bits;
			break;
		case TREPLAC:
			atp[TFIXOFS + bn] = bits;	/* replace */
		case TGET: ;				/* just get it. (done anyway) */
	    }
	if (((*cp)&TCNTL != TGET) && (bn<6))		/* if speeds or mode changed, return 0. (for dhttymd) */
		return(0);
	return(1);
}
