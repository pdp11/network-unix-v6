#
/*
 *	Copyright 1973 Bell Telephone Laboratories Inc
 */

/*
 *   KL/DL-11 driver
 */
#include "/usr/sys/h/param.h"
#include "/usr/sys/h/conf.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/tty.h"
#include "/usr/sys/h/proc.h"

/* base address */
#define	KLADDR	0177560	/* console */
#define	KLBASE	0176500	/* kl and dl11-a */
#define	DLBASE	0175610	/* dl-e */
#define	NKL11	1
#define	NDL11	10
#define DSC	0100000
#define RING    040000
#define CARRIER 010000
#define ANSR    04
#define DSRDY	02
#define	RDRENB	01

struct	tty kl11[NKL11+NDL11];

struct klregs {
	int klrcsr;
	int klrbuf;
	int kltcsr;
	int kltbuf;
};

int	dialup[] {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
		/*0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10*/

klopen(dev, flag)
{
	register char *addr;
	register struct tty *tp;

	if(dev.d_minor >= NKL11+NDL11) {
		u.u_error = ENXIO;
		return;
	}
	tp = &kl11[dev.d_minor];
	if (u.u_procp->p_ttyp == 0) {
		u.u_procp->p_ttyp = tp;
		tp->t_dev = dev;
	}
	/*
	 * set up minor 0 to address KLADDR
	 * set up minor 1 thru NKL11-1 to address from KLBASE
	 * set up minor NKL11 on to address from DLBASE
	 */
	addr = KLADDR + 8*dev.d_minor;
	if(dev.d_minor)
		addr =+ KLBASE-KLADDR-8;
	if(dev.d_minor >= NKL11)
		addr =+ DLBASE-KLBASE-8*NKL11+8;
	tp->t_addr = addr;
	if ((tp->t_state&ISOPEN) == 0) {
		tp->t_state = ISOPEN;
		tp->t_flags = XTABS|LCASE|ECHO|CRMOD;
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
	}
	addr->kltcsr =| IENABLE;
	if (dialup[dev.d_minor]) {
		tp->t_state =| WOPEN;
		addr->klrcsr =| (IENABLE|040);
		if (addr->klrcsr & (RING|CARRIER)) {
			tp->t_state =| CARR_ON;
			addr->klrcsr =| ANSR|DSRDY|RDRENB;
		}
		while ((tp->t_state & CARR_ON) == 0)
			sleep(&tp->t_rawq,TTIPRI);
		tp->t_state =& ~WOPEN;
	}
	else
	{
	tp->t_state =| CARR_ON;
	addr->klrcsr =| IENABLE|DSRDY|RDRENB|016;
	}
}

klclose(dev)
{
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
if(tp->t_flags&HUPCL)
		tp->t_addr->klrcsr =& ~(ANSR|DSRDY);
	wflushtty(tp);
	tp->t_state = 0;
}

klread(dev)
{
	ttread(&kl11[dev.d_minor]);
}

klwrite(dev)
{
	ttwrite(&kl11[dev.d_minor]);
}

klxint(dev)
{
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
	ttstart(tp);
	if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc == TTLOWAT)
		wakeup(&tp->t_outq);
}

klrint(dev)
{
	register int c, *addr;
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
	addr = tp->t_addr;
	c = addr->klrbuf;
	addr->klrcsr =| RDRENB;
	if (dialup[dev.d_minor]) {
		if(addr->klrcsr&RING) {
			addr->klrcsr =| (ANSR|DSRDY);
			return;
		}

		if(tp->t_state&WOPEN && addr->klrcsr&CARRIER) {
			tp->t_state =| CARR_ON;
			wakeup(&tp->t_rawq);
			return;
		}
		if((addr->klrcsr & CARRIER) == 0) {
			if((tp->t_state&WOPEN) == 0) {
				if(tp->t_state & CARR_ON) {
					signal(tp, SIGHUP);
					addr->klrcsr = IENABLE|040;
				}
				flushtty(tp);
			}
		tp->t_state =& ~CARR_ON;
		return;
		}
	}

	if ((c&0177)==0)
		addr->kltbuf = c;	/* hardware botch */
	ttyinput(c, tp);
}

klsgtty(dev, v)
int *v;
{
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
	ttystty(tp, v);
}
