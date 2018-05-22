#
/*
 */

/*
 * general TTY subroutines
 */
#include "/usr/sys/h/param.h"
#include "/usr/sys/h/systm.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/tty.h"
#include "/usr/sys/h/proc.h"
#include "/usr/sys/h/inode.h"
#include "/usr/sys/h/file.h"
#include "/usr/sys/h/reg.h"
#include "/usr/sys/h/conf.h"

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 */
char	maptab[]
{
	000,000,000,000,004,000,000,000,
	010,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,'|',000,000,000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	'@',000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};

/*
 * The actual structure of a clist block manipulated by
 * getc and putc (mch.s)
 */
struct cblock {
	struct cblock *c_next;
	char info[6];
};

/* The character lists-- space for 6*NCLIST characters */
struct cblock cfree[NCLIST];
/* List head for unused character blocks. */
struct cblock *cfreelist;

/*
 * structure of device registers for KL, DL, and DC
 * interfaces-- more particularly, those for which the
 * SSTART bit is off and can be treated by general routines
 * (that is, not DH).
 */
struct {
	int ttrcsr;
	int ttrbuf;
	int tttcsr;
	int tttbuf;
};

/*
 * routine called on first teletype open.
 * establishes a process group for distribution
 * of quits and interrupts from the tty.
 */
ttyopen(dev, atp)
struct tty *atp;
{
	register struct proc *pp;
	register struct tty *tp;

	pp = u.u_procp;
	tp = atp;
	if(pp->p_pgrp == 0) {
		pp->p_pgrp = pp->p_pid;
		u.u_ttyp = tp;
		u.u_ttyd = dev;
		tp->t_pgrp = pp->p_pid;
	}
	tp->t_state =& ~WOPEN;
	tp->t_state =| ISOPEN;
}

/*
 * The routine implementing the gtty system call.
 * Just call lower level routine and pass back values.
 */
gtty()
{
	int v[3];
	register *up, *vp;

	vp = v;
	sgtty(vp);
	if (u.u_error)
		return;
	up = u.u_arg[0];
	suword(up, *vp++);
	suword(++up, *vp++);
	suword(++up, *vp++);
}

/*
 * The routine implementing the stty system call.
 * Read in values and call lower level.
 */
stty()
{
	register int *up;

	up = u.u_arg[0];
	u.u_arg[0] = fuword(up);
	u.u_arg[1] = fuword(++up);
	u.u_arg[2] = fuword(++up);
	sgtty(0);
}

/*
 * Stuff common to stty and gtty.
 * Check legality and switch out to individual
 * device routine.
 * v  is 0 for stty; the parameters are taken from u.u_arg[].
 * c  is non-zero for gtty and is the place in which the device
 * routines place their information.
 */
sgtty(v)
int *v;
{
	register struct file *fp;
	register struct inode *ip;

	if ((fp = getf(u.u_ar0[R0])) == NULL)
		return;
	ip = fp->f_inode;
	if ((ip->i_mode&IFMT) != IFCHR) {
		u.u_error = ENOTTY;
		return;
	}
	(*cdevsw[ip->i_addr[0].d_major].d_sgtty)(ip->i_addr[0], v);
}

/*
 * Wait for output to drain, then flush input waiting.
 */
wflushtty(atp)
struct tty *atp;
{
	register struct tty *tp;

	tp = atp;
	spl5();
	while (tp->t_outq.c_cc) {
		tp->t_speeds =& ~(BLOCK|CDUMP);	/* RJB 77-09 */
		tp->t_state =| ASLEEP;
		sleep(&tp->t_outq, TTOPRI);
	}
	flushtty(tp);
	spl0();
}

/*
 * Initialize clist by freeing all character blocks, then count
 * number of character devices. (Once-only routine)
 */
cinit()
{
	register int ccp;
	register struct cblock *cp;
	register struct cdevsw *cdp;

	ccp = cfree;
	for (cp=(ccp+07)&~07; cp <= &cfree[NCLIST-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
	}
	ccp = 0;
	for(cdp = cdevsw; cdp->d_open; cdp++)
		ccp++;
	nchrdev = ccp;
}

/*
 * flush all TTY queues
 */
flushtty(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int sps;

	tp = atp;
	tp->t_speeds =& ~(BLOCK|CDUMP);		/* RJB 77-09 */
	while (getc(&tp->t_canq) >= 0);
	while (getc(&tp->t_outq) >= 0);
	sps = PS->integ;
	spl5();
	while (getc(&tp->t_rawq) >= 0);
	tp->t_delct = 0;
	PS->integ = sps;
}

