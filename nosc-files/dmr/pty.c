#/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */
#include "param.h"
#include "conf.h"
#include "user.h"
#include "tty.h"

#ifndef NPTY
#define NPTY 8		/* Number of pseudo-teletypes */
#endif
#define NODELAY (HUPCL | XTABS | LCASE | ECHO | CRMOD | RAW | ODDP | EVENP)

/*
 * A pseudo-teletype is a special device which is not unlike a pipe.
 * It is used to communicate between two processes.  However, it allows
 * one to simulate a teletype, including mode setting, interrupt, and
 * multiple end of files (all not possible on a pipe).  There are two
 * really two drivers here.  One is the device which looks like a TTY
 * and can be thought of as the slave device, and hence its routines are
 * are prefixed with 'pts' (PTY slave).  The other driver can be
 * thought of as the controlling device, and its routines are prefixed
 * by 'ptc' (PTY Controller).  To type on the simulated keyboard of the
 * PTY, one does a 'write' to the controlling device.  To get the
 * simulated printout from the PTY, one does a 'read' on the controlling
 * device.  Normally, the controlling device is called 'ptyx' and the
 * slave device is called 'ttyx' (to make programs like 'who' happy).
 */

struct tty pty[NPTY];			/* TTY headers for PTYs */

ptsopen(dev,flag)			/* Open for PTY Slave */
{
	register struct tty *tp;
	int ptsstart();

	if (dev.d_minor >= NPTY) {	/* Verify minor device number */
		u.u_error = ENXIO;	/* No such device */
		return;
	}
	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */

	/* The slave signals that it is open by setting the ISOPEN or WOPEN
	 * bit in the t_state word.  The controller signals that it is open
	 * by setting the CARR_ON bit in the t_state word.
	 */

	if ((tp->t_state&(ISOPEN|WOPEN))==0)	/* closed? */
	{
		tp->t_addr = &ptsstart;		/* special start routine */
		tp->t_flags = 0;/* No special features (nor raw mode) */
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
	}
	for(;;) {
		tp->t_state =| WOPEN|SSTART;	/* Mark as opening */
		if(tp->t_state&CARR_ON) break;	/* has ptc openned yet? */
		sleep(&tp->t_addr,TTIPRI+3);	/* arbitrary prio */
	}
	ttyopen(dev,tp);
}

ptsclose(dev)					/* Close slave part of PTY */
{
	register struct tty *tp;

	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */
/*	wakeup(&tp->t_rawq);		/* ???? */
	tp->t_state =& ~(ISOPEN|WOPEN);	/* Mark as closed */
	wakeup(&tp->t_addr);		/* Wakeup anybody trying to open */
	ptsstart(tp);			/* Let controller return EOF */
	wflushtty(tp);			/* Wait for it */
}

ptsread(dev) /* Read from PTY, i.e. from data written by controlling device */
{
	ttread(&pty[dev.d_minor]);
}

ptswrite(dev) /* Write on PTY, i.e. to be read from controlling device */
{
	ttwrite(&pty[dev.d_minor]);
}
/*
 *  The PTY Controller simulates a line driver -- it sets CARR_ON when there
 * is somebody there to do something with it, and drops the carrier when he
 * goes away.  BUSY is set as long as the controlling side is open.
 */

ptcopen(dev,flag)				/* Open for PTY Controller */
{
	register struct tty *tp;

	if (dev.d_minor >= NPTY ||	/* Verify minor device number */
	    (tp = &pty[dev.d_minor])->t_state&BUSY	/* Already open? */
	    || (tp->t_state&(ISOPEN|WOPEN)) == 0) {	/* Disabled? */
		u.u_error = ENXIO;	/* No such device */
		return;
	}
	tp->t_state =| (CARR_ON|BUSY);	/* Mark as open */
	wakeup(&tp->t_addr);		/* Wakeup slave's open sleep */
}

ptcclose(dev)				/* Close controlling part of PTY */
{
	register struct tty *tp;

	tp = &pty[dev.d_minor];		/* Point to PTY struct */
	tp->t_state =& ~(CARR_ON|BUSY);	/* Mark as closed */
	if (tp->t_state&ISOPEN)		/* Is slave open? */
		signal(tp->t_pgrp,SIGHUP);	/* Yes, send a HANGUP */
	flushtty(tp);			/* Clean things up */
}

ptcread(dev)				/* Read from PTY's output buffer */
{
	register struct tty *tp;
	int ptsstart(), nullsys();

	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */
/*	wakeup(&tp->t_rawq);		/* ???? */
					/* Wait for something to arrive */
	while (tp->t_outq.c_cc==0 &&
	    (tp->t_state&(ISOPEN|CARR_ON))==(ISOPEN|CARR_ON)) {
		tp->t_addr = &ptsstart;
		sleep(&tp->t_outq.c_cf,TTIPRI+2); /* (Woken by ptsstart) */
						/*different prio for debuggin*/
		tp->t_addr = &nullsys;
	}
	/* Copy in what's there, or all that will fit */
	while (tp->t_outq.c_cc && passc(getc(&tp->t_outq))>=0);

	/* Potentially wake up process waiting on output queue */
	if (tp->t_outq.c_cc<=TTLOWAT && tp->t_state&ASLEEP) {
		tp->t_state =& ~ASLEEP;
		wakeup(&tp->t_outq);
	}
}

ptsstart(tp) /* Called by 'ttstart' to output a character.  Merely wakes up */
{				/* controlling half, which does actual work */
	wakeup(&tp->t_outq.c_cf);
}

ptcwrite(dev)			/* Stuff characters into PTY's input buffer */
{
	register struct tty *tp;
	register char c;

	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */
	/* For each character test if the other side is still open */
	/* do this first in order to give the writer a better idea of how */
	/* many chars he sent */
	while ((c = cpass()) >= 0 && (tp->t_state&ISOPEN))
		ttyinput(c,tp);
}

/* Note: Both slave and controlling device have the same routine for */
					/* 'sgtty' */
ptysgtty(dev,v)	int *v;			/* Read and write status bits */
{
	register struct tty *tp;

	tp = &pty[dev.d_minor];
	if(ttystty(tp, v))
		return;		/* gtty */
			/* Delay loses on pty's */
	tp->t_flags =& NODELAY;	/* was stty */
	if(tp->t_speeds.lobyte == 0) {
		tp->t_flags =| HUPCL;
		tp->t_flags =& ~CARR_ON;
		signal(tp->t_pgrp, SIGHUP);
		ptsstart();
	}
}
