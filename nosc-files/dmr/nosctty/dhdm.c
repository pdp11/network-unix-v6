#
/*
 *	DM-BB driver
 *
 *      This driver has been modified to support multiple  DM11s,
 *      in  order  to  avail  yourself  of  it  you  must  do the
 *      following.
 *
 *      In this module:
 *           Initialize the array dmaddrs to contain the  base
 *           addresses of your DM11s.
 *
 *      In l.s:
 *           When you set up the  interrupt  vectors  for  the
 *           DM11s,  you should add a DM11 index number to the
 *           interrupt PS for each vector  (0  for  the  first
 *           one,  1  for the second, etc.) This index will be
 *           passed to the interrupt routine so  that  it  can
 *           tell  which  DM11  caused  the interrupt. This is
 *           done so that one interrupt  routine  can  service
 *           the interrupts from all of the DM11s.
 *
 *      globals:
 *         dmaddrs      array of pointers to the sets of dm11 registers.
 *
 *      history:
 *
 *      Started out as a standard release 6 DM11 driver.
 *      Modified for Multi DM support by Dennis L. Mumaugh
 *              15 April 1977
 *      Thanks to Mark Kampe (UCLA) for assistance.
 *
 *	Merged by Greg Noel with his code prior to April 79, to include
 *		FLOWCTL, paramaterized devices, etc.
 */

#include "param.h"
#include "tty.h"
#include "conf.h"


#ifndef NDH11
#define	NDH11	16	/* number of lines */
#endif  NDH11
#ifndef DMADDRS
#define DMADDRS	0770500	/* default register addresses */
#endif	DMADDRS

struct	tty dh11[NDH11];
int dmaddrs[(NDH11+15)/16] { DMADDRS };

#define	DONE	0200
#define	SCENABL	040
#define	CLSCAN	01000
#define	TURNON	017	/* RQ send, CD lead, line enable */
#define	FLOWOFF	013	/* stop tty input from host*/
#define	TURNOFF	1	/* line enable only */
#define	CARRIER	0100

struct dmregs {
	int	dmcsr;
	int	dmlstat;
};

/*
 * Turn on the line associated with the (DH) device dev.
 */
dmopen(dev)
{
	register struct tty *tp;
	register struct dmregs *dmaddr;

	dmaddr = dmaddrs[dev.d_minor >> 4];
	tp = &dh11[dev.d_minor];
	dmaddr->dmcsr = dev.d_minor&017;
	dmaddr->dmlstat = TURNON;
	if (dmaddr->dmlstat&CARRIER)
		tp->t_state =| CARR_ON;
	dmaddr->dmcsr = IENABLE|SCENABL;
	spl5();
	while ((tp->t_state&CARR_ON)==0)
		sleep(&tp->t_rawq, TTIPRI);
	spl0();
}

/*
 * If a DH line has the HUPCL mode,
 * turn off carrier when it is closed.
 */
dmclose(dev)
{
	register struct tty *tp;
	register struct dmregs *dmaddr;

	dmaddr = dmaddrs[dev.d_minor>>4];
	tp = &dh11[dev.d_minor];
	if (tp->t_flags&HUPCL) {
		dmaddr->dmcsr = dev.d_minor&017;
		dmaddr->dmlstat = TURNOFF;
		dmaddr->dmcsr = IENABLE|SCENABL;
	}
}

/*
 * DM11 interrupt.
 * Mainly, deal with carrier transitions.
 */
dmint(dev)
int dev;
{
	register struct tty *tp;
	register struct dmregs *dmaddr;

	dmaddr = dmaddrs[dev];
	if (dmaddr->dmcsr&DONE) {
		tp = &dh11[ (dev<<4) | (dmaddr->dmcsr&017) ];
		if (tp < &dh11[NDH11]) {
			wakeup(tp);
			if ((dmaddr->dmlstat&CARRIER)==0) {
				if ((tp->t_state&WOPEN)==0) {
					signal(tp->t_pgrp, SIGHUP);
					dmaddr->dmlstat = 0;
					flushtty(tp);
				}
				tp->t_state =& ~CARR_ON;
			} else
				tp->t_state =| CARR_ON;
		}
		dmaddr->dmcsr = IENABLE|SCENABL;
	}
}

#ifdef FLOWCTL
/*
 * Potentially apply flow control to a DH line
 */
dmread(atp)
struct tty *atp;
{
	register struct tty *tp;
	register struct dmregs *dmaddr;
	register int dev;

	tp = atp;
	if(tp < &dh11[NDH11-FLOWCTL]) {
		ttread(tp);	/* standard line */
	} else {
		spl5();
		while(tp->t_rawq.c_cc == 0 && tp->t_state&CARR_ON)
			sleep(&tp->t_rawq, TTIPRI);
		spl0();
		while(tp->t_rawq.c_cc && passc(getc(&tp->t_rawq)) >= 0);
		if ( tp->t_rawq.c_cc < TTLOWAT && tp->t_state&NOTIFY ) {
			/* reenable host line */
			dev = tp - dh11;
			dmaddr = dmaddrs[dev>>4];
			dmaddr->dmcsr = dev&017;
			dmaddr->dmlstat = TURNON;
			tp->t_state =& ~NOTIFY;
			dmaddr->dmcsr = IENABLE|SCENABL;
						/* scan and interrupt enable*/
		}
	}
}
dmflow(ac, atp)
struct tty *atp;
{
	register int c;
	register struct tty *tp;
	register struct dmregs *dmaddr;

	c = ac;  tp = atp;
	if (tp < &dh11[NDH11-FLOWCTL]) {
		ttyinput(c, tp);	/* regular line */
	} else {
		if (tp->t_rawq.c_cc == 0) wakeup(&tp->t_rawq);
		putc(c, &tp->t_rawq);
		/* turn off host input if raw queue full*/
		if (tp->t_rawq.c_cc > TTHIWAT) {
			c = tp - dh11;
			dmaddr = dmaddrs[c>>4];
			dmaddr->dmcsr = c&017;
			dmaddr->dmlstat = FLOWOFF; /* turn off channel*/
			tp->t_state =| NOTIFY;
			dmaddr->dmcsr =	IENABLE|SCENABL;
			    /* interrupt and scan enable*/
		}
	}
}
#endif FLOWCTL
