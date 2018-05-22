#
/*
 * changed for Delphi/UNIX
 */

/*
 *   KL/DL-11 driver
 */
#include "param.h"
#include "conf.h"
#include "user.h"
/* #include "../userx.h" */
#include "tty.h"

/* base address */
#define	KLADDR	0177560	/* console */
#define	KLBASE	0176500	/* kl and dl11-a */
#define	DLBASE	0175610	/* dl-e */
#define	NKL11	1
#define	NDL11	0
#define DSRDY	02
#define	RDRENB	01

#define	NL1	000400
#define	NL2	001000
#define	CR2	020000
#define	FF1	040000
#define	TAB1	002000

struct	tty kl11[NKL11+NDL11];

struct klregs {
	int klrcsr;
	int klrbuf;
	int kltcsr;
	int kltbuf;
}

klopen(dev, flag)
{
	register char *addr;
	register struct tty *tp;

	if(dev.d_minor >= NKL11+NDL11) {
		u.u_error = ENXIO;
		return;
	}
	tp = &kl11[dev.d_minor];
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
		tp->t_flags = XTABS|ECHO|CRMOD;			/*la36*/
/*		tp->t_flags = XTABS|LCASE|ECHO|CRMOD|CR1;	/*tty33*/
/*		tp->t_flags = EVENP|ECHO|NL1|CR2|FF1|TAB1;	/*tty37*/
/*		tp->t_erase = CERASE;	*/
/*		tp->t_kill = CKILL;	*/
/*		tp->t_quit = 'B' & 077;				/*interrupt characters */
/*		tp->t_intr = 'Q' & 077;	*/
/*		tp->t_EOT = 'D' & 077;	*/
	}
	addr->klrcsr =| IENABLE|DSRDY|RDRENB;
	addr->kltcsr =| IENABLE;
	ttyopen(dev, tp);
	tp->t_state =| CARR_ON;
}

klclose(dev)
{
	register struct tty *tp;

	tp = &kl11[dev.d_minor];
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

/*	add a handler for the `ttymode' trap */

klttymd(dev, acp)
int *acp;
{	register struct tty *tp;
	tp = &kl11[dev.d_minor];
	ttmode(tp, acp);
}
