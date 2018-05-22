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
 */

#include "param.h"
#include "tty.h"
#include "conf.h"

int dmaddrs[2] {0170500, 0170510};

struct	tty dh11[];
int	ndh11;		/* Set by dh.c to number of lines */

#define	BUSY	020	/* for CSR, not line status reg */
#define SECRX	020
#define	DONE	0200
#define	SCENABL	040
#define	CLSCAN	01000
#define	TURNON	03	/* CD lead, line enable */
#define	TURNOFF	1	/* line enable only */
#define	CARRIER	0100
#ifdef DNTTY
#define	DMRTS	04
#define DMCTS	040
#endif DNTTY

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
	if (tp->t_flags&DUMC){		/* KLH: if don't want modem ctl, */
		dmaddr->dmlstat = 0;	/* turn it off for line */
		tp->t_state =| CARR_ON;	/* pretend carrier's on */
		dmaddr->dmcsr = IENABLE|SCENABL;
		return;
		}
	dmaddr->dmlstat = TURNON;
	if (dmaddr->dmlstat&CARRIER)
		tp->t_state =| CARR_ON;
	dmaddr->dmcsr = IENABLE|SCENABL;
	spl5();
	while ((tp->t_state&CARR_ON)==0)
		sleep(&tp->t_canq, TTIPRI);
	spl0();
}

/*
 * If a DH line has the HUPCL mode,
 * turn off carrier when it is closed.
 */
/* This routine is obsoleted by existence of DMCTL. --KLH 5/15/79
 * dmclose(dev)
 * {
 *	register struct tty *tp;
 *	register struct dmregs *dmaddr;
 *
 *	dmaddr = dmaddrs[dev.d_minor>>4];
 *	tp = &dh11[dev.d_minor];
 *	if (tp->t_flags&HUPCL) {
 *		dmaddr->dmcsr = dev.d_minor&017;
 *		dmaddr->dmlstat = TURNOFF;
 *		dmaddr->dmcsr = IENABLE|SCENABL;
 *	}
 * }
 */

/*
 * Dump control bits into the DM registers.
 */
dmctl(dev, bits)
{
	register struct dmregs *dmaddr;
	dmaddr = dmaddrs[dev.d_minor>>4];
	dmaddr->dmcsr = dev.d_minor&017;
	dmaddr->dmlstat = bits;
	dmaddr->dmcsr = IENABLE|SCENABL;
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
		if (tp < &dh11[ndh11]) {
#ifdef DNTTY
			if (dmaddr->dmlstat&DMCTS) tp->t_dnflgs =| CTS;
			else tp->t_dnflgs =& ~CTS;
#endif DNTTY
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

/* This is the MIT routine "dmsecrcv" which isn't needed, hence
 * commented out.  It has been converted for multiple DH/DM's however.
 * --KLH 5/15/79
 */
#ifdef COMMENT
dmsecrcv(dev)
int dev;
{	register int ans;
	register struct dmregs *dmaddr;

	dmaddr = dmaddrs[dev.d_minor>>4];
	dmaddr->dmcsr =& ~SCENABL;
	while (dmaddr->dmcsr & BUSY);
	dmaddr->dmcsr =& ~017;
	dmaddr->dmcsr =| dev.d_minor&017;
	dmaddr->dmlstat =| TURNOFF;
	ans = dmaddr->dmlstat & SECRX;
	dmaddr->dmcsr =| SCENABL;
	return(ans);
}
#endif COMMENT
/*

* DN Modem support
*/

#ifdef DNTTY

/* Assert RTS for line and return state of CTS lead after suitable
 * delay for modem signal propagation.
 * This is called only from dhstart at spl 5, so needn't worry about
 * DH or timeout interrupts.
 */

dmaskcts(dev)
int dev;
{
	register struct dmregs *dmaddr;
	register int flag;

	dmaddr = dmaddrs[dev.d_minor>>4];
	dmaddr->dmcsr = dev.d_minor&017;
	dmaddr->dmlstat =| DMRTS;
	flag = dmaddr->dmlstat&DMCTS;
	dmaddr->dmcsr = IENABLE | SCENABL;
	return(flag);
}

/* Turn off RTS for specified line */

dmrtsoff(dev)
int dev;
{
	register struct dmregs *dmaddr;

	dmaddr = dmaddrs[dev.d_minor>>4];
	dmaddr->dmcsr = dev.d_minor&017;
	dmaddr->dmlstat =& ~DMRTS;
	dmaddr->dmcsr = IENABLE | SCENABL;
}

#endif DNTTY

