#
/*
 */

/*
 *   KL/DL-11 driver
 */
#include "../h/param.h"
#include "../h/conf.h"
#include "../h/inode.h"
#include "../h/user.h"
#include "../h/tty.h"
#include "../h/proc.h"

/* base address */
#define	KLADDR	0177560	/* console */
#define	KLBASE	0176500	/* kl and dl11-a */
#define	DLBASE	0175610	/* dl-e */
#define DSRDY	02
#define	RDRENB	01

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
		tp->t_state = ISOPEN|CARR_ON;
		tp->t_flags = XTABS|LCASE|ECHO|CRMOD;
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
	}
	addr->klrcsr =| IENABLE|DSRDY|RDRENB;
	addr->kltcsr =| IENABLE;
	ttyopen(dev, tp);
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
/* Modified 1/20/78 (BBN:jfh) to call awake
 */
{
	register struct tty *tp;
	register int tcc,titp;

	tp = &kl11[dev.d_minor];
	ttstart(tp);
	tcc = tp->t_outq.c_cc;
	if (((titp = tp->t_itp) != 0) && (tcc == TTLOWAT)) awake(titp,0);
	if (tcc == 0 || tcc == TTLOWAT) wakeup(&tp->t_outq);
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
/*
 * Await and Capacity routines
 */

/*
 * klttycap -- capacity for kl tty devices
 * BBN (dan) 3/26/79
 * BBN (dan) 4/9/79: made use new await interface.
 */

klttycap(ip, v)
   struct inode *ip;
   int *v;
{
   register struct tty *tp;
   extern struct tty kl11[];

   tp = &kl11[ip->i_addr[0].d_minor];
   ttycap(tp, v);
}


klttyawt(type, ip, fd)
    char type;
    struct inode *ip;
    int fd;
{
    extern char *ablei();
    register struct tty *tp;

    tp = &kl11[ip->i_addr[0].d_minor];
    tp->t_itp = ablei(type, 0,ip,fd) ;
}
