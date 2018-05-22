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
 *
 *	Merged by Greg Noel with his code prior to April 79, to include
 *		FLOWCTL, paramaterized devices, etc.
 */

#include "param.h"
#include "conf.h"
#include "user.h"
#include "tty.h"

#ifndef NDH11
#define	NDH11	16	/* default number of lines */
#endif  NDH11
#define	DHNCH	8	/* max number of DMA chars */
#ifndef DHADDRS
#define DHADDRS	0760020	/* default register locations */
#endif	DHADDRS

struct	tty dh11[NDH11];
/*
 * Place from which to do DMA on output
 */
char	dh_clist[NDH11][DHNCH];

/*
 * Hardware control bits
 */
#define	BITS6	01
#define	BITS7	02
#define	BITS8	03
#define	TWOSB	04
#define	PENABLE	020
/* DEC manuals incorrectly say this bit causes generation of even parity. */
#define	OPAR	040
#define	HDUPLX	040000

#define	IENABLE	030100
#define	PERROR	010000
#define	FRERROR	020000
#define	XINT	0100000
#define	SSPEED	7	/* standard speed: 300 baud */

/*
 * Software copy of last dhbar
 */
int 	dhsar[(NDH11+15)/16];

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

int dhaddrs[(NDH11+15)/16] { DHADDRS };

/*
 * Open a DH11 line.
 */
dhopen(dev, flag)
{
	register struct tty *tp;
	register struct dhregs *dhaddr;
	extern dhstart();

	if (dev.d_minor >= NDH11) {
		u.u_error = ENXIO;
		return;
	}
	dhaddr = dhaddrs[ dev.d_minor >> 4 ];
	tp = &dh11[dev.d_minor];
#ifdef FLOWCTL
	if (dev.d_minor >= NDH11-FLOWCTL && tp->t_state&ISOPEN) {
		u.u_error = EACCES;
		return;
	}
#endif FLOWCTL
	tp->t_addr = dhstart;
	dhaddr->dhcsr =| IENABLE;
	tp->t_state =| WOPEN|SSTART;
	if ((tp->t_state&ISOPEN) == 0) {
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
		tp->t_speeds = SSPEED | (SSPEED<<8);
		tp->t_flags = ODDP|EVENP|ECHO;
		dhparam(dev);
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
	dmclose(dev);
	tp->t_state =& (CARR_ON|SSTART);
	wflushtty(tp);
}

/*
 * Read from a DH11 line.
 */
dhread(dev)
{
#ifndef FLOWCTL
	ttread(&dh11[dev.d_minor]);
#endif no FLOWCTL
#ifdef FLOWCTL
	dmread(&dh11[dev.d_minor]);
#endif FLOWCTL
}

/*
 * write on a DH11 line
 */
dhwrite(dev)
{
#ifndef FLOWCTL
	ttwrite(&dh11[dev.d_minor]);
#endif no FLOWCTL
#ifdef FLOWCTL
	register struct tty *tp;
	register int c;

	tp = &dh11[dev.d_minor];

	if (tp < &dh11[NDH11-FLOWCTL]) ttwrite(tp);
	else {
		if ((tp->t_state&CARR_ON)==0) return;
		while ((c = cpass()) >= 0) {
			spl5();
			while (tp->t_outq.c_cc > TTHIWAT) {
				dhstart(tp);
				tp->t_state =| ASLEEP;
				sleep(&tp->t_outq, TTOPRI);
			}
			spl0();
			putc(c,&tp->t_outq);
		}
		dhstart(tp);
	}
#endif FLOWCTL
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
		if((tp->t_state&ISOPEN)==0 || (c&PERROR)) {
			wakeup(tp);
			continue;
		}
		if (c&FRERROR)		/* break */
			if (tp->t_flags&RAW)
				c = 0;		/* null (for getty) */
			else
				c = 0177;	/* DEL (intr) */
#ifndef FLOWCTL
		ttyinput(c, tp);
#endif no FLOWCTL
#ifdef FLOWCTL
		dmflow(c, tp);
#endif FLOWCTL
	}
}

/*
 * stty/gtty for DH11
 */
dhsgtty(dev, av)
int *av;
{
	register struct tty *tp;
	register r;

	tp = &dh11[dev.d_minor];
	if (ttystty(tp, av))
		return;
	dhparam(dev);
}

/*
 * Set parameters from open or stty into the DH hardware
 * registers.
 */
dhparam(dev)
{
	register struct tty *tp;
	register int lpr;
	register struct dhregs *dhaddr;

	tp = &dh11[dev.d_minor];
	spl5();
	dhaddr = dhaddrs[ dev.d_minor >> 4 ];
	dhaddr->dhcsr.lobyte = (dev.d_minor&017) | IENABLE;
	/*
	 * Hang up line?
	 */
	if (tp->t_speeds.lobyte==0) {
		tp->t_flags =| HUPCL;
		dmclose(dev);
		return;
	}
	lpr = (tp->t_speeds.hibyte<<10) | (tp->t_speeds.lobyte<<6);
	if (tp->t_speeds.lobyte == 4)		/* 134.5 baud */
		lpr =| BITS6|PENABLE|HDUPLX; else
		if (tp->t_flags&EVENP)
			if (tp->t_flags&ODDP)
				lpr =| BITS8; else
				lpr =| BITS7|PENABLE; else
			lpr =| BITS7|OPAR|PENABLE;
	if (tp->t_speeds.lobyte == 3)	/* 110 baud */
		lpr =| TWOSB;
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
	register c, nch;
	register struct tty *tp;
	int sps, dev;
	struct dhregs *dhaddr;
	char *cp;

	sps = PS->integ;
	spl5();
	tp = atp;
	dhaddr = dhaddrs[ (dev = tp-dh11) >> 4 ];
	/*
	 * If it's currently active, or delaying,
	 * no need to do anything.
	 */
	if (tp->t_state&(TIMEOUT|BUSY))
		goto out;
	/*
	 * t_char is a delay indicator which may have been
	 * left over from the last start.
	 * Arrange for the delay.
	 */
	if (c = tp->t_char) {
		tp->t_char = 0;
		timeout(ttrstrt, tp, (c&0177)+6);
		tp->t_state =| TIMEOUT;
		goto out;
	}
	cp = dh_clist[dev.d_minor];
	nch = 0;
	/*
	 * Copy DHNCH characters, or up to a delay indicator,
	 * to the DMA area.
	 */
	while (nch > -DHNCH && (c = getc(&tp->t_outq))>=0) {
#ifndef FLOWCTL
		if (c >= 0200) {
#endif no FLOWCTL
#ifdef FLOWCTL
		if (tp < &dh11[NDH11-FLOWCTL] && c >= 0200) {
#endif FLOWCTL
			tp->t_char = c;
			break;
		}
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
	} else if (c = tp->t_char) {
		tp->t_char = 0;
		timeout(ttrstrt, tp, (c&0177)+6);
		tp->t_state =| TIMEOUT;
	}
    out:
	PS->integ = sps;
}
