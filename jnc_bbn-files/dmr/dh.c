#
/*
 */

/*
 *      DH-11 driver - supports multiple DH-11s
 *	This driver calls on the DHDM driver.
 *	If the DH has no DM11-BB, then the latter will
 *	be fake. To insure loading of the correct DM code,
 *	lib2 should have dhdm.o, dh.o and dhfdm.o in that order.
 *      Modified for await and capacity by Dan Franklin Sept 28 1978
 *      Modified by BBN(dan) Mar 26 79: removed Rand '370 parity'
 *      Modified by BBN(dan) April 9 79: made use new await interface
 *      Modified by BBN(cdh) April 17 79: for use of DH-11
 */

#include "../h/param.h"
#include "../h/conf.h"
#include "../h/inode.h"
#include "../h/user.h"
#include "../h/tty.h"
#include "../h/proc.h"

#define DHADR0  0160020
#define DHADR1  0160040
#define NDH11   32      /* number of lines */
			/* if you change NDH11, be sure to update DH11  */
			/* in dhdm.c */
#define DHNCH   14      /* max number of DMA chars */

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
int     dhsar[(NDH11+15)/16];

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

struct dhregs *dhadrs[]
{	DHADR0,
	DHADR1
};

/*
 * Open a DH11 line.
 */
dhopen(dev, flag)
{
	register struct tty *tp;
	struct dhregs *dhaddr;
	extern dhstart();

	if (dev.d_minor >= NDH11) {
		u.u_error = ENXIO;
		return;
	}
	tp = &dh11[dev.d_minor];
	tp->t_addr = dhstart;
	dhaddr = dhadrs[dev.d_minor>>4]; /* get pointer to right DH11 */
	dhaddr->dhcsr =| IENABLE;
	tp->t_state =| WOPEN|SSTART;
	if ((tp->t_state&ISOPEN) == 0) {
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
		tp->t_speeds = SSPEED | (SSPEED<<8);
		tp->t_flags = ODDP|EVENP|ECHO;    /* rand:jrl/lauren */
		dhparam(dev);                           /* HUPCL added */
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
dhrint(dhno)
int dhno;
{
	register struct tty *tp;
	register int c;
	register struct dhregs *dhaddr;

	dhaddr = dhadrs[dhno];
	while ((c = dhaddr->dhnxch) < 0) {      /* char. present */
		tp = &dh11[(dhno<<4) + ((c>>8)&017)];
		if (tp >= &dh11[NDH11])
			continue;
		if((tp->t_state&ISOPEN)==0 || (c&PERROR)) {
                        if (tp->t_itp) awake(tp->t_itp, 0);
			wakeup(tp);
			continue;
		}
		if (c&FRERROR)		/* break */
			if (tp->t_flags&RAW)
				c = 0;		/* null (for getty) */
			else
				c = 0177;	/* DEL (intr) */
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
	struct dhregs *dhaddr;

	tp = &dh11[dev.d_minor];
	spl5();
	dhaddr = dhadrs[dev.d_minor >> 4];
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
dhxint(dhno)
int dhno;
{
	register struct tty *tp;
	register ttybit, bar;
	struct dhregs *dhaddr;

	dhaddr = dhadrs[dhno];
	bar = dhsar[dhno] & ~dhaddr->dhbar;
	dhaddr->dhcsr =& ~XINT;
	ttybit = 1;
	for (tp = &dh11[dhno<<4]; bar; tp++) {
		if(bar&ttybit) {
			dhsar[dhno] =& ~ttybit;
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
	int nch;
	register c;
	register struct dhregs *dhaddr;
	register struct tty *tp;
	int sps, dev;
	int dhno;
	char *cp;

	sps = PS->integ;
	spl5();
	tp = atp;
	dev = tp - dh11;
	/*
	 * If it's currently active, or delaying,
	 * no need to do anything.
	 */
	if (tp->t_state&(TIMEOUT|BUSY|XOFFHNG))         /* agn - XON/XOFF */
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
               if (c >= 0200) {
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
        if (tp->t_outq.c_cc <= TTLOWAT) {   /* Low tide? */
                if (tp->t_itp)              /* Awaken awaiting processes */
                        awake(tp->t_itp, 0);
                if (tp->t_state&ASLEEP) {   /* And procs sleeping on output */
                        tp->t_state =& ~ASLEEP;
                        wakeup(&tp->t_outq);
                }
        }
        /*
	 * If any characters were set up, start transmission;
	 * otherwise, check for possible delay.
	 */
	if (nch) {
		dhno = dev.d_minor >> 4;
		dhaddr = dhadrs[dhno];
		dhaddr->dhcsr.lobyte = (dev.d_minor&017) | IENABLE;
		dhaddr->dhcar = cp+nch;
		dhaddr->dhbcr = nch;
		c = 1 << (dev.d_minor&017);
		dhaddr->dhbar =| c;
		dhsar[dhno] =| c;
		tp->t_state =| BUSY;
	} else if (c = tp->t_char) {
		tp->t_char = 0;
		timeout(ttrstrt, tp, (c&0177)+6);
		tp->t_state =| TIMEOUT;
	}
    out:
	PS->integ = sps;
}

/* -------------------------- D H T T Y C A P --------------------- */

dhttycap(ip, v)
   struct inode *ip;
   int *v;
   {
   register struct tty *tp;

   tp = &dh11[ip->i_addr[0].d_minor];
   ttycap(tp, v);
   }

/* -------------------------- D H T T Y A W T ----------------------- */

dhttyawt(type, ip, fd)
    char type;
    struct inode *ip;
    int fd;
{
    extern char *ablei();
    register struct tty *tp;

    tp = &dh11[ip->i_addr[0].d_minor];
    tp->t_itp = ablei(type, 0, ip, fd);
}
