#
#ifndef NEW
#define PGRPS x
#endif
/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 *
#ifndef NEW
 *	install with cc -Oc pty.c [-D EXTA] [-D PGRPS]
 *			ar r lib pty.o
#endif
#ifdef NEW
 *	install with cc -Oc pty.c [-D EXTA] [-D PGRPS];ar r lib pty.o
#endif
 *
 *	EXTA adds code so that the pty can be used as a batch device, ala
 *	TOPS-10 (This code is experimental and doesn't work).
 *	PGRPS if defined indicates that process groups are to be used.
 *	Otherwise the older non-process group signalling system will be
#ifndef NEW
 *	used. 
 *
 *	This particular source is slightly more elegant than the current
 *	version of pty.c .  Unfortunately this version is also 8 bytes
 *	bigger.  For certain installations 8 bytes is a lot...  So being
 *	basically chicken hearted, this version is classed as EXPERIMENTAL.
#endif
#ifdef NEW
 *	used.  NOTE: in this source PGRPS is already defined.  It will have
 *	to be deleted to revert to the older system (unless your compiler
 *	has the -U switch).
#endif
 */
#include "/usr/sys/h/param.h"
#include "/usr/sys/h/conf.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/tty.h"
#ifndef NEW
#include "/usr/sys/h/proc.h"
#endif

#define NPTY 8				/* Number of pseudo-teletypes */
#define NODELAY (HUPCL | XTABS | LCASE | ECHO | CRMOD | RAW | ODDP | EVENP)
#ifndef NEW
#define	SSPEED	11	/* DLM Special 2400 */
#endif
/*#define EXTA	((14<<8)|14)*/
#ifdef NEW
#define PGRPS X
#endif

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
#ifndef NEW
#ifndef	PGRPS
	register struct proc *pp;
#endif	PGRPS
#endif
	extern int nullsys();
#ifdef NEW
	extern int ptsstart();
#endif

	if (dev.d_minor >= NPTY)	/* Verify minor device number */
	{
		u.u_error = ENXIO;	/* No such device */
		return;
	}
	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */

	/* The slave signals that it is open by setting the ISOPEN
#ifndef NEW
	 * bit in the t_state byte.  The controller signals that it is
	 * is open by setting the CARR_ON bit in the t_state byte.
#endif
#ifdef NEW
	 * bit in the t_state word.  The controller signals that it is
	 * is open by putting the address of ptsstart in the t_addr word.
#endif
	 * The absense of these conditions signals that the respective
	 * side has closed.  This signalling is necessary because these
	 * closes can occur asynchronously.
#ifdef NEW
	 * t_addr should point to a valid subroutine name whenever
	 * the slave is open (has returned from ptsopen).  For this reason
	 * the routine nullsys is used as an alternate to ptsstart
	 * when the controller wants to signal the slave that the controller
	 * has closed.  There should be a better method of signalling...
#endif
	 */

	if ((tp->t_state&(ISOPEN|WOPEN))==0)	/* closed? */
	{
#ifndef NEW
		tp->t_state =| WOPEN|SSTART;
#endif
#ifdef NEW
		tp->t_state =| WOPEN|SSTART|CARR_ON;
#endif
					/* Mark as open and with */
					/* special start routine */
		tp->t_flags = 0;/* No special features (nor raw mode) */
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
#ifndef NEW
		tp->t_speeds = (SSPEED | (SSPEED << 8)); /* DLM Special */
#endif
	}
#ifndef NEW
#ifdef	PGRPS
#endif
	ttyopen(dev,tp);
#ifndef NEW
#endif	PGRPS
#ifndef	PGRPS
	pp = u.u_procp;
	if (pp->p_ttyp == 0)
	{
		pp->p_ttyp = tp;	/* for /dev/tty etc */
		tp->t_dev = dev;
	}
	tp->t_state =| ISOPEN;
#endif	PGRPS
#endif
	for(;;)
	{
		wakeup(&tp->t_state);	/* Wakeup controller's open sleep */
#ifndef NEW
		if(tp->t_state&CARR_ON)	/* has ptc openned yet? */
#endif
#ifdef NEW
		if(tp->t_addr == &ptsstart)	/* has ptc openned yet? */
#endif
			break;		/* this should not happen the first
					 * time around the loop
					 */
		sleep(&tp->t_addr,TTIPRI+3);	/* arbitrary prio */
	}
}

ptsclose(dev)					/* Close slave part of PTY */
{
	register struct tty *tp;
	extern int ptsstart();

#ifndef NEW
	tp = &pty[dev.d_minor];			/* Setup pointer to PTY header */
#endif
	tp->t_state =& ~WOPEN;			/* Mark intermediate state */
#ifndef NEW
	if (tp->t_state&CARR_ON)		/* Other side open? */
#endif
#ifdef NEW
	tp = &pty[dev.d_minor];			/* Setup pointer to PTY header */
	if (tp->t_addr == &ptsstart)		/* Other side open? */
#endif
	{
		ptsstart(tp);
		wakeup(&tp->t_rawq);
		wflushtty(tp);			/* Yes, wait for it */
	}else{
		flushtty(tp);			/* Otherwise, just blast it */
	}
	tp->t_state =& ~ISOPEN;		/* Mark as closed */
}

ptsread(dev) /* Read from PTY, i.e. from data written by controlling device */
{
	register struct tty *tp;
#ifdef NEW
	extern int ptsstart();
#endif

	tp = &pty[dev.d_minor];			/* point to PTY struct */
#ifndef NEW
#	ifdef EXTA
	if(tp->t_speeds==EXTA)
		ptttread(tp);
	else
#	endif EXTA
	ttread(tp);
#endif
#ifdef NEW
	if (tp->t_addr == &ptsstart)	/* Is there a controlling device? */
#		ifdef EXTA
		if(tp->t_speeds==EXTA)
			ptttread(tp);
		else
#		endif EXTA
			ttread(tp);		/* Yes, then read it! */
						/* else, return end of file */
#endif
}

ptswrite(dev) /* Write on PTY, i.e. to be read from controlling device */
{
	register struct tty *tp;
	extern int ptsstart();

	tp = &pty[dev.d_minor];			/* point to PTY struct */
#ifndef NEW
	ttwrite(tp);
#endif
#ifdef NEW
	if (tp->t_addr == &ptsstart)		/* controlling dev open? */
		ttwrite(tp);
#endif
}

ptcopen(dev,flag)				/* Open for PTY Controller */
{
	register struct tty *tp;
#ifndef NEW
	extern int ptsstart();	/* Routine to start write from slave */
#endif
#ifdef NEW
	int ptsstart();			/* Routine to start write from slave */
#endif

	if (dev.d_minor >= NPTY)	/* Verify minor device number */
	{
		u.u_error = ENXIO;	/* No such device */
		return;
	}
	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */
#ifndef NEW
	if (tp->t_state&CARR_ON)			/* Already open? */
#endif
#ifdef NEW
	if (tp->t_addr == &ptsstart)			/* Already open? */
#endif
	{
		u.u_error = EIO;	/* I/O error
					(Is there something better? */
		return;
	}
	tp->t_addr = &ptsstart;		/* Name of start routine */
#ifndef NEW
	tp->t_state =| CARR_ON;
#endif
	for(;;)
	{
		wakeup(&tp->t_addr);		/* Wakeup slave's open sleep */
		if(tp->t_state&ISOPEN)
			break;		/* this should not happen the first
					 * time around the loop
					 */
		sleep(&tp->t_state,TTIPRI+4);	/* arbitrary prio */
	}
}

ptcclose(dev)				/* Close controlling part of PTY */
{
	register struct tty *tp;
	extern int nullsys();

	tp = &pty[dev.d_minor];			/* Point to PTY struct */
#ifndef NEW
	tp->t_state =& ~CARR_ON;		/* Mark as closed */
#endif
#ifdef NEW
	tp->t_addr = &nullsys;			/* Mark as closed */
#endif
	if (tp->t_state&(ISOPEN|WOPEN))		/* Is it slave open? */
#ifdef	PGRPS					/* process group system */
		signal(tp->t_pgrp,SIGHUP);	/* Yes, send a HANGUP */
#endif
#ifndef	PGRPS					/* for old version six */
		signal (tp, SIGHUP);
#endif
	flushtty(tp);				/* Clean things up */
}

ptcread(dev)				/* Read from PTY's output buffer */
{
	register struct tty *tp;
	register int temp;

	tp = &pty[dev.d_minor];		/* Setup pointer to PTY header */
	wakeup(&tp->t_rawq);
					/* Wait for something to arrive */
	while (tp->t_outq.c_cc==0 && (tp->t_state&ISOPEN))
	{
		sleep(&tp->t_outq.c_cf,TTIPRI+2); /* (Woken by ptsstart */
						/*different prio for debuggin*/
	}
	temp = tp->t_outq.c_cc;	/* Remember so we don't check every time */
	/* Copy in what's there, or all that will fix */
	while (tp->t_outq.c_cc && passc(getc(&tp->t_outq))>=0);
	/* Now, decide whether to wake up process waiting on input queue */
	if (tp->t_outq.c_cc==0 || (temp>TTLOWAT && tp->t_outq.c_cc<=TTLOWAT))
		wakeup(&tp->t_outq);
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
	{
#		ifdef EXTA
		if(tp->t_speeds==EXTA)	/* Extern A */
		{
			ptsstart(tp);
			while(tp->t_delct>0 && (tp->t_state&ISOPEN))
			{
				sleep(&tp->t_rawq,TTIPRI+1);	/* different
								 * prio so
								 * we can see
								 * it with ps
								 */
			}
		}
#		endif EXTA
		ttyinput(c,tp);
	}
}

/* Note: Both slave and controlling device have the same routine for */
					/* 'sgtty' */
ptysgtty(dev,v)	int *v;			/* Read and write status bits */
{
	register struct tty *tp;
	register int *vv;

	tp = &pty[dev.d_minor];
#	ifdef EXTA
	if(tp->t_speeds==EXTA && v == 0)
	{
		vv = &u.u_arg[1];
		tp->t_erase = vv->lobyte;
		tp->t_kill = vv->hibyte;
		tp->t_flags = vv[1];
	}
	else
#	endif EXTA
		if(ttystty(tp, v))
			return;		/* gtty */
		else
			/* Delay loses on pty's */
			tp->t_flags =& NODELAY;	/* was stty */
}

/*	This is the ttread for pty's if we are going to use pty's for
 *	batch processing.  The only addition is that echoing is done at
 *	the time that the char is read up into user space.  This increase
 *	in synchrony between the input and output is needed so that chars
 *	from user processes don't fall all over the echo of input chars.
 *	Echo is assumed
 *	to be off and speed of EXTA is used to indicate the special
 *	function.)
 */
#ifdef EXTA
ptttread(atp)
struct tty *atp;
{
	register struct tty *tp;
	register cnt;
	register int c;

	tp = atp;
	cnt = 0;
	if ((tp->t_state&CARR_ON)==0)
		return( 0 );
	if (tp->t_canq.c_cc || canon(tp)) {
		while (tp->t_canq.c_cc && passc(c=getc(&tp->t_canq))>=0) {
			if(tp->t_speeds==EXTA)
				ttyoutput(c,tp);
			cnt++;
		}
	}
	return( cnt );
}
#endif EXTA
