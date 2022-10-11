#
/*
 */

/*
 *	DM-BB driver
 *      Modified for await Dan Franklin Sept 28 1978
 *      Modified by BBN (cdh) 17 Apr 79 for multiple DH interfaces that
 *      may or may not have DM devices.
 */
#include "../h/param.h"
#include "../h/tty.h"
#include "../h/conf.h"

#define DH11    2               /* number of DH11s connnected */
				/* this number should = (NDH11 + 15)/16 */
				/* where NDH11 is defined in dh.c */
/*
 * The following DM addresses should correspond directly with the DH11s
 * that they are associated with.  If there is no DM11 associated with
 * a given DH, define the appropriate address as 0177777 as a flag.
 */

#define DMADR0  0170500
#define DMADR1  0177777

struct	tty dh11[];
int	ndh11;		/* Set by dh.c to number of lines */

#define	DONE	0200
#define	SCENABL	040
#define	CLSCAN	01000
#define	TURNON	07	/* RQ send, CD lead, line enable */
#define TURNOFF 1      /* line enable only */
#define	CARRIER	0100

struct dmregs {
	int	dmcsr;
	int	dmlstat;
};

struct dmregs *dmadrs[DH11]
{       DMADR0,
	DMADR1
};

/* structure to allow selective modem control BBN:cdh 7/31/78 */
/* There should be as many entries in this table as there are DH lines */

char dmodems[DH11][16] {                /* Array to indicate which lines need modem control */
	0, 0, 0, 0, 0, 0, 0, 0,		/* 0 means no modem control, 1 means yes */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Turn on the line associated with the (DH) device dev.
 */
dmopen(dev)
{
	register struct tty *tp;
	register struct dmregs *dmaddr;
	register int dmno;		/* BBN:cdh */

	tp = &dh11[dev.d_minor];
	dmno = dev.d_minor>>4;
	dmaddr = dmadrs[dmno];

	/* If modem control not wanted, turn on carrier in tp struct, BBN:cdh */

	if (dmodems[dmno][dev.d_minor&017] == 0 || dmaddr == 0177777) {
		tp->t_state =| CARR_ON;
		return;
	}
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
	register int dmno;              /* BBN:cdh */

	tp = &dh11[dev.d_minor];
	dmno = dev.d_minor>>4;
	dmaddr = dmadrs[dmno];
	if (tp->t_flags&HUPCL && dmaddr != 0177777 &&   /* BBN:cdh */
	    dmodems[dmno][dev.d_minor&017] != 0) {      /* ignore if no DM */
		dmaddr = dmadrs[dev.d_minor>>4];
		dmaddr->dmcsr = dev.d_minor&017;
		dmaddr->dmlstat = TURNOFF;
		dmaddr->dmcsr = IENABLE|SCENABL;
	}
}

/*
 * DM11 interrupt.
 * Mainly, deal with carrier transitions.
 * One should insure that the minor device number contained in the
 * interrupt vector corresponds to the number of the DH that the DM
 * is associated with, not just the DM number.  This is significant
 * if there exist DH11's (or equivalents) that have no DM11's.
 */
dmint(dmno)
int dmno;
{
	register struct tty *tp;
	register struct dmregs *dmaddr; /* put in a register */
	register int dmline;

	dmaddr = dmadrs[dmno];
	if (dmaddr->dmcsr&DONE) {
		dmline = dmaddr->dmcsr&017;	/* BBN:cdh */
		tp = &dh11[(dmno<<4) + dmline];
		if (tp < &dh11[ndh11]) {
                        if (tp->t_itp) awake(tp->t_itp, 0);
			wakeup(tp);
			if ((dmaddr->dmlstat&CARRIER)==0 &&
			    dmodems[dmno][dmline] != 0) {	/* BBN:cdh */
				if ((tp->t_state&WOPEN)==0) {
					signal(tp, SIGHUP);
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
