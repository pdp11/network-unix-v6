#
/*
 *	DH-11 driver
 *	This driver calls on the DHDM driver.
 *	If the DH has no DM11-BB, then the latter will
 *	be fake. To insure loading of the correct DM code,
 *	lib2 should have dhdm.o, dh.o and dhfdm.o in that order.
 *
 *      This driver was modified to allow  the  use  of  multiple
 *      DH11s.   To avail yourself of this feature (or to disable
 *      it) you should do the following:
 *
 *      In this module: Initialize the global array dhaddrs  with
 *          the  base addresses for your dh11s.  Initialize NDH11
 *          to the total number of dh11  ports  in  your  system.
 *          Dimension dhsar to the number of dh11s you have.
 *
 *      In l.s: When you initialize  the  interrupt  vectors  for
 *          your  dh11s, add to each interrupt PS a dh11 index (0
 *          for the first dh11,  1  for  the  second  etc).  This
 *          number   will  be  used  to  index  into  dhaddrs  to
 *          determine  which  set   of   registers   caused   the
 *          interrupt.  It allows all sets of dh registers to use
 *          the same interrupt routines.
 *
 *      globals:
 *
 *      dhaddrs   contains base addresses for the various dh11s in the
 *                system.
 *
 *      dhsar     contains (for each dh11) a copy of the bar as it
 *                was last known to be.
 *
 *      history:
 *
 *      Initially a Release six driver.
 *      Modified by Dennis L. Mumaugh for multiple DH/DM support
 *              15 April 1977
 *      Thanks to Mark Kampe (UCLA) for assistance
 */

#include "param.h"
#include "conf.h"
#include "user.h"
/*#include "userx.h"*/
#include "tty.h"
#include "proc.h"
#ifdef DNTTY
#include "baudot.h"	/* DN: file containing baudot mapping tables */
#define VRTMO 20+20	/* 60'ths of sec to wait for dropping RTS */
#define IDLTMO (15*60*60)&077777 /* 60'ths of sec to sleep between idle chks */
#endif DNTTY

#ifndef NDH11
#define	NDH11	32	/* number of lines */
#endif NDH11
#define	DHNCH	8	/* max number of DMA chars */

struct	tty dh11[NDH11];
/*
 * Place from which to do DMA on output
 */
char	dh_clist[NDH11][DHNCH];

/*
 * Used to communicate the number of lines to the DM
 */
int	ndh11	NDH11;

/*
 * Hardware control bits
 */
#ifdef DNTTY
#define BITS5	00	/* DN: for Baudot terms */
#endif DNTTY
#define	BITS6	01
#define	BITS7	02
#define	BITS8	03
#define	TWOSB	04
#define	PENABLE	020
#define	OPAR	040	/* Old DEC manuals wrongly say this sets EVEN parity.*/
#define	HDUPLX	040000

#define	IENABLE	030100
#define	PERROR	010000
#define	FRERROR	020000
#define	XINT	0100000
#define	SSPEED	7	/* standard speed: 300 baud */

/*
 * DM control bits
 */
#define	TURNON	03	/* CD lead + line enable */
#define	TURNOFF	01	/* line enable */
#define	RQS	04	/* request to send */

#define	XCLUDE	0200	/* exclusive-use flag against open */

/*
 * Software copy of last dhbar
 */
int 	dhsar[2];

struct dhregs {
	int dhcsr;
	int dhnxch;
	int dhlpr;
	int dhcar;
	int dhbcr;
	int dhbar;
	int dhbreak;
	int dhsilo;
};

int dhaddrs[2] { 0760020, 0760040 };

#ifdef DNTTY
int dnidlflg;	/* Set when idle timeout rtn activated */
#endif DNTTY

/*
 * Open a DH11 line.
 */
dhopen(dev, flag)
{
	register struct tty *tp;
	register struct dhregs *dhaddr;
	extern dhstart();

#ifdef DNTTY
	extern dnidltmo();
	if(dnidlflg == 0) {
		dnidlflg++;
		timeout(dnidltmo, 0, IDLTMO);
		}
#endif DNTTY

	if (dev.d_minor >= NDH11) {
		u.u_error = ENXIO;
		return;
	}
	dhaddr = dhaddrs[ dev.d_minor >> 4 ];
	tp = &dh11[dev.d_minor];
	tp->t_addr = dhstart;
	tp->t_dev = dev;
	dhaddr->dhcsr =| IENABLE;
	tp->t_state =| WOPEN|SSTART;
	if ((tp->t_state&ISOPEN) == 0) {
/*		tp->t_erase = CERASE;	*/
/*		tp->t_kill = CKILL;	*/
/*		tp->t_quit = CQUIT;	/* I'm not sure this is the best */
/*		tp->t_intr = CINTR;	/* place to set these up but... */
/*		tp->t_EOT =  CEOT;	*/
		tp->t_speeds = SSPEED | (SSPEED<<8);
		tp->t_flags = ODDP|EVENP|ECHO;
#ifdef DNTTY
		tp->t_dnflgs = 0;
		tp->t_ioct = 1;
		/* DN: following line is complete crock to turn off modem
		 * control for the first DH.  It would be unnecessary if
		 * there was some decent way to set TTY parameters BEFORE
		 * the open attempt, or if system itself knew more about
		 * its hardwired line parameters.  Can just use null modems,
		 * but it would sure be nice to have things set right from
		 * the start.
		 */
/*		if(dev.d_minor>>4 == 0) tp->t_flags =| DUMC; */
		/* another big crock */
		if((dev.d_minor&017) < 5) tp->t_flags =| DUMC;
#endif DNTTY
		dhparam(tp);
	} else if (tp->t_state&XCLUDE && u.u_uid!=0) {
		u.u_error = EBUSY;
		return;
	}
	dmopen(dev);
	ttyopen(dev, tp);
}

/*
 * Close a DH11 line.
 */
dhclose(dev)
{
	register struct tty *tp;

	tp = &dh11[dev.d_minor];
	if (tp->t_flags&HUPCL)
		dmctl(dev, TURNOFF);
	tp->t_state =& (CARR_ON|SSTART);
#ifdef DNTTY
	tp->t_dnflgs =& ~(TMOCTS|TMORTS);
#endif DNTTY
	wflushtty(tp);
}

/*
 * Read from a DH11 line.
 */
dhread(dev)
{
	ttread(&dh11[dev.d_minor]);
}

/*
 * write on a DH11 line
 */
dhwrite(dev)
{
	ttwrite(&dh11[dev.d_minor]);
}

/*
 * DH11 receiver interrupt.
 */
dhrint(dev)
int dev;
{
	register struct tty *tp;
	register int c;
	register struct dhregs *dhaddr;

	dhaddr = dhaddrs[dev];
	while ((c = dhaddr->dhnxch) < 0) {	/* char. present */
		tp = &dh11[ (dev<<4) | ((c>>8) & 017) ];
		if (tp >= &dh11[NDH11])
			continue;
		if((tp->t_state&ISOPEN)==0) {
			wakeup(tp);
			continue;
		}
		if (c&PERROR)
			if ((tp->t_flags&(EVENP|ODDP))==EVENP
			 || (tp->t_flags&(EVENP|ODDP))==ODDP )
				continue;
		if (c&FRERROR)		/* break */
			if (tp->t_flags&RAW)
				c = 0;		/* null (for getty) */
			else
				c = tp->t_intr;	/* intr */
#ifdef DNTTY
		if (tp->t_dnflgs&BAUDOT){
			tp->t_ioct++;		/* Bump to mark non-idle */
			c = bditab[(c&037)|(tp->t_dnflgs&FIGS ? 040 : 0)]&0377;
			/* printf("(I:%o,%o)",ac&0177,c);	/* Debug */
			switch(c) {
				case FIGSCHR: tp->t_dnflgs =| FIGS; continue;
				case LETSCHR: tp->t_dnflgs =& ~FIGS; continue;
				case 015:
					if(tp->t_dnflgs&LICLF) break;
					else {tp->t_dnflgs=| LICCR; continue;}
				case 012:
					if(tp->t_dnflgs&LICCR){c = 015; break;}
					else {tp->t_dnflgs=| LICLF; continue;}
				}
			tp->t_dnflgs =& ~(LICCR | LICLF);
			}
#endif DNTTY
		ttyinput(c, tp);
	}
}

/*
 * stty/gtty for DH11
 */
dhsgtty(dev, av)
int *av;
{
	register struct tty *tp;
	register struct dhregs *dhaddr;
	register int *mod;

	tp = &dh11[dev.d_minor];
	/*
	 * Special weirdness.
	 * On stty, if the input speed is 15 (EXT B)
	 * then the output speed selects a special function.
	 * The stored modes are not affected.
	 */
	if (av==0 && (mod=u.u_arg)[0].lobyte==15) {
		dhaddr = dhaddrs[dev.d_minor>>4];
		switch (mod[0].hibyte) {

		/*
		 * Set break
		 */
		case 1:
			dhaddr->dhbreak =| 1<<(dev.d_minor&017);
			return;

		/*
		 * Clear break
		 */
		case 2:
			dhaddr->dhbreak =& ~(1<<(dev.d_minor&017));
			return;

		/*
		 * Turn on request to send
		 */
		case 3:
			dmctl(dev, TURNON|RQS);
			return;

		/*
		 * Turn off request to send
		 */
		case 4:
			dmctl(dev, TURNON);
			return;

		/*
		 * Prevent opens on channel
		 */
		case 5:
			tp->t_state =| XCLUDE;
			return;
		}
		return;
	}
	if (ttystty(tp, av))
		return;
	dhparam(tp);
}

/*
 * Set parameters from open or stty into the DH hardware
 * registers.
 */
dhparam(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int lpr;
	register struct dhregs *dhaddr;

	tp = atp;
	spl5();
	dhaddr = dhaddrs[ tp->t_dev.d_minor >> 4 ];
	dhaddr->dhcsr.lobyte = (tp->t_dev.d_minor&017) | IENABLE;
	/*
	 * Hang up line?
	 */
	if (tp->t_speeds.lobyte==0) {
		tp->t_flags =| HUPCL;
		dmctl(tp->t_dev, TURNOFF);
		return;
	}
	lpr = (tp->t_speeds.hibyte<<10) | (tp->t_speeds.lobyte<<6);
	switch (tp->t_speeds.lobyte) {
#ifdef DNTTY
		case BEXTA:		/* DN: Crock - Ext A implies baudot */
			lpr =| BITS5|TWOSB;	/* 5 bits with 1.5 stop bits */
			tp->t_dnflgs =| BAUDOT;	/* State of FIGS undefined */
			break;
#endif DNTTY
		case B134:		/* 134.5 baud */
			lpr =| BITS6|PENABLE|HDUPLX;
			break;
		case B110:		/* 110 baud */
			lpr =| TWOSB;	/* fall thru to continue */
		default:
			if (tp->t_flags&EVENP)
				if (tp->t_flags&ODDP) lpr =| BITS8;
						else lpr =| BITS7|PENABLE;
				else lpr =| BITS7|OPAR|PENABLE;
		}
	dhaddr->dhlpr = lpr;
	spl0();
}

/*
 * DH11 transmitter interrupt.
 * Restart each line which used to be active but has
 * terminated transmission since the last interrupt.
 */
dhxint(dev)
int dev;
{
	register struct tty *tp;
	register ttybit, bar;
	struct dhregs *dhaddr;

	dhaddr = dhaddrs[dev];
	bar = dhsar[dev] & ~dhaddr->dhbar;
	dhaddr->dhcsr =& ~XINT;
	ttybit = 1;
	for (tp = &dh11[dev<<4]; bar; tp++) {
		if(bar&ttybit) {
			dhsar[dev] =& ~ttybit;
			bar =& ~ttybit;
			tp->t_state =& ~BUSY;
			dhstart(tp);
		}
		ttybit =<< 1;
	}
}

/*
 * Start (restart) transmission on the given DH11 line.
 */
dhstart(atp)
struct tty *atp;
{
	extern ttrstrt();
#ifdef DNTTY
	extern dnrstrt(),dnrtsoff();	/* timeout routines */
#endif DNTTY
	register c, nch;
	register struct tty *tp;
	int sps, dev;
	struct dhregs *dhaddr;
	char *cp;

	sps = PS->integ;
	spl5();
	tp = atp;
	dev = tp-dh11;
	dhaddr = dhaddrs[ dev.d_minor >> 4 ];
	/*
	 * If it's currently active, or delaying,
	 * no need to do anything.
	 */
	if (tp->t_state&(TIMEOUT|BUSY))
		goto out;

	/* Commented out; too MIT specific.
	 * It has been cvted for multi DH/DM tho. --KLH 5/15/79
	 */
#ifdef COMMENT
	/*
	 * This routine hacks our line printer.  Namely it checks
	 * if the line printer is busy (dmsecrcv) and if it is,
	 * goes away for a while.
	 */
	if ((tp->t_flag2 & SECRX) && dmsecrcv(dev)) {
		timeout(ttrstrt, tp, 60);
		tp->t_state =| TIMEOUT;
		goto out;
	}
#endif COMMENT

	/*
	 * t_char is a delay indicator which may have been
	 * left over from the last start.
	 * Arrange for the delay.
	 */
	if ((c = tp->t_char)
#ifdef DNTTY
		&& ((c >= 0200) || (!(tp->t_dnflgs&BAUDOT)))
#endif DNTTY
		) {
		tp->t_char = 0;
		timeout(ttrstrt, tp, (c&0177)+6);
		tp->t_state =| TIMEOUT;
		goto out;
	}
#ifdef DNTTY
redo:
#endif DNTTY
	cp = dh_clist[dev.d_minor];
	nch = 0;
#ifdef DNTTY
	if (tp->t_dnflgs&BAUDOT) {
		if (!(tp->t_dnflgs&CTS)) {	/* If can't send, */
			if (dmaskcts(dev)) {	/* ask politely */
				tp->t_dnflgs =| CTS;	/* Won! */
				tp->t_dnflgs =& ~TMOCTS; /*may be pending*/
				printf(">");	/* Debug crock */
				}
			else {			/* Barf, wait a second. */
				if((tp->t_dnflgs&TMOCTS) == 0) {
					tp->t_dnflgs =| TMOCTS;
					timeout(dnrstrt, tp, 60);
					}
				goto out;
				}
			}
		if (c = tp->t_char) {	/* Holds extra output char */
			tp->t_char = 0;
			*cp++ = c;
			nch--;
			}
		}
#endif DNTTY
	/*
	 * Copy DHNCH characters, or up to a delay indicator,
	 * to the DMA area.
	 */
	while (nch > -DHNCH && (c = getc(&tp->t_outq))>=0) {
		if (c >= 0200 && (tp->t_flag2 & LITOUT)==0) {
			tp->t_char = c;
			break;
			}
#ifdef DNTTY
		if (tp->t_dnflgs&BAUDOT) {
			if ((c = bdtcvt(c,tp)) < 0)
				continue;	/* May want to flush char */
			*cp++ = c;
			if ((--nch > -DHNCH) && tp->t_char) {
				c = tp->t_char;
				tp->t_char = 0;
				}		/* Drop thru to store */
			else continue;	/* Already stored char so skip */
			}
#endif DNTTY
		*cp++ = c;
		nch--;
	}
	/*
	 * If the writer was sleeping on output overflow,
	 * wake him when low tide is reached.
	 */
	if (tp->t_outq.c_cc<=TTLOWAT && tp->t_state&ASLEEP) {
		tp->t_state =& ~ASLEEP;
		wakeup(&tp->t_outq);
	}
	/*
	 * If any characters were set up, start transmission;
	 * otherwise, check for possible delay.
	 */
	if (nch) {
		dhaddr->dhcsr.lobyte = (dev.d_minor & 017) | IENABLE;
		dhaddr->dhcar = cp+nch;
		dhaddr->dhbcr = nch;
		c = 1 << (dev.d_minor & 017);
		dhaddr->dhbar =| c;
		dhsar[dev.d_minor>>4] =|c;
		tp->t_state =| BUSY;
#ifdef DNTTY
		tp->t_dnflgs =& ~(TMOCTS);
		tp->t_ioct++;		/* Bump IO cnt to mark non-idle */
#endif DNTTY
	} else if ((c = tp->t_char)
#ifdef DNTTY
		&& ((c >= 0200) || (!(tp->t_dnflgs&BAUDOT)))
#endif DNTTY
		) {
		tp->t_char = 0;
		timeout(ttrstrt, tp, (c&0177)+6);
		tp->t_state =| TIMEOUT;
	}
#ifdef DNTTY
	/* This code contrives to turn off DN modem's RTS lead after
	 * the last char has left the DH.  We cannot turn it off after
	 * the output-complete interrupt because 2 chars are still in the
	 * UART, and we can't pad out the delay because baudot terms
	 * have no padding character... so set up a demon to check things
	 * after 2 char times (2*(1/6 sec)) have passed.
	 */
	/* New additional crock: try to ensure that when output
	 * is done, tty is left in LETS mode rather than FIGS.  No
	 * attempt at extra buffering or optimization is done here,
	 * because the speeds are so slow (6cps) it is highly probable
	 * that when the output queue becomes empty, the user program
	 * really has nothing more to send.  We'll see.
	 */
	else if (tp->t_dnflgs&BAUDOT) {
		if(tp->t_dnflgs&FIGS){		/* Crock */
			tp->t_char = LETSCHR;	/* " */
			tp->t_dnflgs =& ~FIGS;	/* " */
			goto redo;		/* " */
			}			/* " */
		/* Now arrange to turn off RTS */
		if(tp->t_dnflgs&TMORTS)
			outtime(dnrtsoff, tp);	/* Remove existing timeout*/
		else tp->t_dnflgs =| TMORTS;
		timeout(dnrtsoff, tp, VRTMO);	/* after last char leaves DH */
		}
#endif DNTTY
    out:
	PS->integ = sps;
}


/* ttymode handler for dh11 */

dhttymd(dev,acp)
int *acp;
{
	register struct tty *tp;
	tp = &dh11[dev.d_minor];
	if (ttmode(tp, acp)) return;
	dhparam(tp);
}

/*

* DN Baudot TTY support - output conversion
*/
#ifdef DNTTY

	/* All ASCII transformations now done -- Hack baudot here
	 * if necessary (note LCASE should have done some work too)
	 */
bdtcvt(ac,atp)
int ac;
struct tty *atp;
{
	register struct tty *tp;
	register int c;
	tp = atp;
	c = ac;
	if (c >= 0140) c =& 0137; /* At this point, be merciless about case. */
	if(c < 040)		/* If a control char, check special cases */
		 switch (c) {
			case '\r': c = 0210; break;
			case '\n': c = 0202; break;
			case 007:  c = 053;  break;	/* ^G - bell */
			case 006:  c = FIGSCHR; break;	/* ^F - force FIGS */
			case 014:  c = LETSCHR; break;	/* ^L - force LETS */
			default: return(-1);
			}
	else c = bdotab[c-040]&0377;
/*	printf("(O:%o,%o)\n",ac,c);	/* Debug */
	if (c&0200) {
		if(c==NOCH) c = 04;	/* for now, cvt randoms to SP */
		switch (c) {
			case FIGSCHR: tp->t_dnflgs=|FIGS; break;
			case LETSCHR: tp->t_dnflgs=& ~FIGS; break;
			case 0202:	/* Special crock for LF */
				tp->t_char = LETSCHR&037;/* Follow with LETS */
				tp->t_dnflgs=& ~FIGS;
				break;
			}
		return(c&037);
		}
	if(c&040 && !(tp->t_dnflgs&FIGS)) { /* Wants FIGS and not in? */
		tp->t_char = c&037;		/* Store char */
		tp->t_dnflgs =| FIGS;
		return(FIGSCHR&037);
		}
	if(!(c&040) && tp->t_dnflgs&FIGS) { /* Wants LETS and not in? */
		tp->t_char = c&037;		/* Store char */
		tp->t_dnflgs =& ~FIGS;
		return(LETSCHR&037);
		}
	return(c&037);
}

#endif DNTTY
/*

* DN Baudot TTY support - timeout routines
*/

/* Both of these routines are called with spl = 5 from the clock
 * interrupt routine, so need not worry about DH or DM interrupts.
 * Likewise, they won't be executed if already at some int level,
 * so DH/DM int level rtns don't need to worry about these either.
 */

#ifdef DNTTY
dnrstrt(atp)
struct tty *atp;
{
	extern dnrstrt();
	register struct tty *tp;
	tp = atp;
			printf("X");		/* Debug hack */
	if (tp->t_dnflgs&TMOCTS)	/* Still need CTS check? */
		if((tp->t_state&ISOPEN) == 0) {	/* For redundant safety */
			tp->t_dnflgs =& ~TMOCTS;
			return;
			}
		if(tp->t_state&BUSY || dmaskcts(tp->t_dev)) {
			tp->t_dnflgs =& ~TMOCTS;
			tp->t_dnflgs =| CTS;
			dhstart(tp);
			}
		else timeout(dnrstrt, tp, 60);
}

dnrtsoff(atp)
struct tty *atp;
{
	register struct tty *tp;
	tp = atp;
	if (tp->t_dnflgs&TMORTS) {
		tp->t_dnflgs =& ~TMORTS;
		if (!(tp->t_state&BUSY)) {
			dmrtsoff(tp->t_dev);
			tp->t_dnflgs =& ~CTS;
			printf("O");		/* Debug hack */
			}
		}
}
#endif DNTTY
/*
*	"outtime" - DN addition.
*	The reverse of the "timeout" kernel routine; removes
*	an entry from the clock queue.
*	Called with the same params as timeout, except the third
*	argument isn't furnished.  If outtime finds an entry with
*	the same argument and function, it will remove it.
*	It is even safe to call this while the clock routine is
*	hacking the queue; it checks for imminent execution and
*	substitutes a null routine if so.
*	It doesn't look for more than one entry, although it could.
*	(remove the break and ensure p1 is preserved during entry flush).
*/

#ifdef DNTTY
#include "systm.h"

outtime(fun, arg)
{
	extern nullrtn();
	register struct callo *p1, *p2;
	register a;
	int s;
	a = arg;
	p1 = &callout[0];
	s = PS->integ;
	spl7();
	while(p1->c_func != 0) {
		if(p1->c_arg == a && p1->c_func == fun) {
			if(p1->c_time <= 0) p1->c_func = &nullrtn;
			else {
				p2 = p1++;
				while(p2->c_func = p1->c_func) {
					p2->c_time = p1->c_time;
					p2->c_arg = p1->c_arg;
					p1++; p2++;
					}
				}
			break;
			}
		p1++;
		}
	PS->integ = s;
}
nullrtn(dummy)
{
}
#endif DNTTY
/**/
/*	Idle timeout routine -- called every IDLTMO ticks
 * to see if any lines have been inactive too long & need hangup.
 */

#ifdef DNTTY

dnidltmo()
{	register i;
	register struct tty *tp;
	for (tp = &dh11[0]; tp < &dh11[NDH11]; tp++)
		if(tp->t_dnflgs&BAUDOT)
		{	if ((tp->t_state&CARR_ON) && (tp->t_ioct == 0))
				dhclose(tp->t_dev);
			tp->t_ioct = 0;
		}
	timeout(dnidltmo, 0, IDLTMO);
}
#endif DNTTY

