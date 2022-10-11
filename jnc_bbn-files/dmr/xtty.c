#
/*
 * general TTY subroutines
 * Added ttycap (general tty capac routine) 9/28/78 Dan Franklin
 * Added XON/XOFF handling - 11/20/78 Alan Nemeth
 * Modified for extended tty driver -- 4/6/79 and on Dan Franklin
 */
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/xty.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/reg.h"
#include "../h/conf.h"

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


/* -------------------------- C I N I T ----------------------------- */
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

/* -------------------------- T T Y O P E N ------------------------- */
/*
 * routine called on first teletype open.
 * establishes a process group for distribution
 * of quits and interrupts from the tty.
 *
 * Modified 12/29/77 to initialize the t_itp pointer (for await) (BBN: jfh)
 *
 */
ttyopen(dev, atp)
struct tty *atp;
{
	register struct proc *pp;
	register struct tty *tp;
	extern int pgrpct;

	pp = u.u_procp;
	tp = atp;
	if(pp->p_pgrp == 0) {
		pp->p_pgrp = ++pgrpct;
		u.u_ttyp = tp;
		u.u_ttyd = dev;
		tp->t_pgrp = pgrpct;
	}
	tp->t_itp = 0;		/* initialize to no awaiters (BBN: jfh) */
	tp->t_state =& ~TS_WOPEN;
	tp->t_state =| TS_ISOPEN;
}

/* -------------------------- M O D T T Y ------------------------------- */
/*
 * The routine implementing the modtty system call.
 * first arg is type: lsb indicates set (1) or get (0).
 * Remaining bits are interpreted by the device driver routine.
 * Second arg is pointer to user's buffer; third arg is length.
 */
modtty()
{
        struct modes v;     /* Actually either modes or cursor structure */
        register char *up, *vp;
        register int ulen;
        int flag;

        vp = &v;
        flag = u.u_arg[0] & 01? 1 : 0;
        up = u.u_arg[1];
        ulen = u.u_arg[2];
        if (ulen > sizeof(v))
            ulen = sizeof(v);

        if (flag)
            while (ulen-- > 0)
                *vp++ = fubyte(up++);

        sgtty(u.u_arg[0], &v);

        if (u.u_error)
                return;
        if (flag == 0)
            while (ulen-- > 0)
                subyte(up++, *vp++);
}

/* -------------------------- S G T T Y ----------------------------- */
/*
 * Stuff common to stty and gtty.
 * Check legality and switch out to individual device routine.
 * The file descriptor is taken from u.u_ar0[R0].
 *
 * Device routines are called with three arguments:
 *      the major/minor device,
 *      the value array,
 *      and the flag.
 */
sgtty(flag, v)
int flag;
char *v;
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
        (*cdevsw[ip->i_addr[0].d_major].d_sgtty)(ip->i_addr[0], v, flag);
}

/* -------------------------- T T Y S T T Y ------------------------- */
/*
 * ttystty -- device driver utility function
 * Called from device's sgtty routine with the address of the
 * device's tty structure.
 * If flag's lsb is zero, gtty is being done and the information is
 * passed back in v;
 * if it is one, stty is being done and v contains the input information.
 *
 * The value returned is 0 if stty was done, nonzero otherwise.
 */
ttystty(tp, av, flag)
struct tty *tp;
char *av;
int flag;
{
    register char *p;
    register char *v;
    register struct modes *modes_p;

    v = av;
    if (flag & ~01)
    {
        u.u_error = EINVAL;
        return;
    }

    modes_p = &tp->t_modes; /* Get base */
    p = modes_p;            /* Assign to p */
    modes_p++;              /* Get top */

    if ((flag & 01) == 0)
    {
        while (p < modes_p)
            *v++ = *p++;
        return(1);
    }
    else
    {
        wflushtty(tp);
        while (p < modes_p)
            *p++ = *v++;
        return(0);
    }
}

/* -------------------------- W F L U S H T T Y --------------------- */
/*
 * Wait for output to drain, then flush input waiting.
 */
wflushtty(atp)
struct tty *atp;
{
	register struct tty *tp;

	tp = atp;
	spl5();
	tp->t_state =& ~TSO_XOFFHNG;        /* agn - XON/XOFF */
	ttstart(tp);                    /* agn - XON/XOFF */
	while (tp->t_outq.c_cc) {
		tp->t_state =| TS_ASLEEP;
		sleep(&tp->t_outq, TTOPRI);
	}
	flushtty(tp);
	spl0();
}

/* -------------------------- F L U S H T T Y ----------------------- */
/*
 * flush all TTY queues
 */
flushtty(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int sps;

	tp = atp;
        while (getc(&tp->t_inq) >= 0);
	while (getc(&tp->t_outq) >= 0);
	wakeup(&tp->t_inq);
	wakeup(&tp->t_outq);
	sps = PS->integ;
	spl5();
	while (getc(&tp->t_inq) >= 0);
	tp->t_delct = 0;
	PS->integ = sps;
}

/* -------------------------- T T R E A D --------------------------- */
/*
 * ttread -- move chars from input queue to user's buffer
 * Called from device's read routine after it has
 * calculated the tty-structure pointer.
 *
 * Sleep until a break char entered (delct nonzero).
 * Read chars and copy to user until T_DELIM hit or passc returns negative.
 *      Handle mapping of T_USR_DELIM back to T_DELIM
 *      If deferred echo, echo here according to t_modes.t_iflags
 * Return count of chars copied.
 */
ttread(atp)
struct tty *atp;
{
    register struct tty *tp;
    int cnt;
    register int c, defecho;
    int lastc;

    tp = atp;
    if ((tp->t_state & TS_CARR_ON) == 0)
        return(0);

    spl5();
    while (tp->t_delct == 0)
    {
        if ((tp->t_state & TS_CARR_ON) == 0)
        {
            spl0();
            return(0);
        }
        sleep(&tp->t_inq, TTIPRI);
    }
    spl0();

    if (tp->t_modes.t_iflags & TI_DEFER_ECHO)
        defecho = tp->t_modes.t_iflags & (TI_BRK_ECHO|TI_NONBRK_ECHO);
    else
        defecho = 0;

    cnt = 0;
    lastc = -1;
    for (;;)
    {
        c = getc(&tp->t_inq);
        if (c == T_DELIM)
        {
            if (defecho & TI_BRK_ECHO)
                ttyoutput(lastc, tp);
            tp->t_delct--;
            break;
        }
        else
        {
            if (defecho & TI_NONBRK_ECHO)
                if (lastc != -1)
                    ttyoutput(lastc, tp);
            lastc = c;
            if (c == T_USR_DELIM)
                c = T_DELIM;
            if (passc(c) >= 0)
                cnt++;
            else
                break;
        }
    }

    if (tp->t_state & TSI_XOFFHNG)
        if (tp->t_inq.c_cc <= TTYWARN)
            {
            ttyoutput(CXON, tp);
            tp->t_state =& ~TSI_XOFFHNG;
            }

    return(cnt);
}

/* -------------------------- M A P T A B   A R R A Y --------------- */
/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Used for
 * upper-case only terminals.
 */
char	maptab[]
{
        000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
        000,'|',000,000,000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
        000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};

/* -------------------------- C L A S S   A R R A Y ----------------- */

int class[128]
{
TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,
/* BS */  /* HT */  /* LF */  /* VT */  /* FF */  /* CR */
TB_FORMAT,TB_FORMAT,TB_LF,    TB_FORMAT,TB_FORMAT,TB_CR,    TB_CTRL,  TB_CTRL,
TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,
TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_ESC,   TB_CTRL,  TB_CTRL,  TB_CTRL,  TB_CTRL,

TB_SPACE, TB_TERM,  TB_OTHER, TB_OTHER, TB_OTHER, TB_OTHER, TB_OTHER, TB_OTHER,
TB_BAL,   TB_BAL,   TB_OTHER, TB_OTHER, TB_TERM,  TB_OTHER, TB_TERM,  TB_OTHER,

TB_DIGIT, TB_DIGIT, TB_DIGIT, TB_DIGIT, TB_DIGIT, TB_DIGIT, TB_DIGIT, TB_DIGIT,
TB_DIGIT, TB_DIGIT, TB_TERM,  TB_TERM,  TB_BAL,   TB_OTHER, TB_BAL,   TB_TERM,

TB_OTHER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER,
TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER,
TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER, TB_UPPER,
TB_UPPER, TB_UPPER, TB_UPPER, TB_BAL,   TB_OTHER, TB_BAL,   TB_OTHER, TB_UNDER,

TB_OTHER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER,
TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER,
TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER, TB_LOWER,
TB_LOWER, TB_LOWER, TB_LOWER, TB_BAL,   TB_OTHER, TB_BAL,   TB_OTHER, TB_DEL
};

/* -------------------------- T T Y I N P U T ----------------------- */
/*
 * Handle one terminal input character.
 * XON/XOFF, special chars, and immediate echoing are all handled here.
 * The top half is awakened as required.
 * The arguments are the character and the appropriate
 * tty structure.
 *
 * TI_LOCAL_ECHO is not handled yet, although TI_TAB_ECHO and TI_CR_ECHO are.
 */
ttyinput (ac, atp)
int ac;
struct tty *atp;
{
    register int t_flags, c;
    register struct tty *tp;
    int ch;

    tp = atp;
    c = ac;
    t_flags = tp->t_modes.t_iflags;

    if (t_flags & TI_CLR_MSB)
        c =& 0177;

    if (tp->t_modes.t_oflags & TO_XONXOFF)
    {
        if (c == CXOFF)
        {
            tp->t_state =| TSO_XOFFHNG;
            return;
        }
        if (c == CXON)
        {
            tp->t_state =& ~TSO_XOFFHNG;
            ttstart(tp);
            return;
        }
    }

    if (c == '\r' && (t_flags & TI_CRMOD))
        c = '\n';
/*
 * Special character processing
 * Note that if t_modes.t_intr, t_modes.t_quit, etc. is set to a value >= 0200, its test will
 * always fail. Thus, setting them to such values turns off their function.
 */
    if (c == tp->t_modes.t_intr)
    {
        signal(tp->t_pgrp, SIGINT);
        flushtty(tp);
    }
    else if (c == tp->t_modes.t_quit)
    {
        signal(tp->t_pgrp, SIGQIT);
        flushtty(tp);
    }
    else if (c == tp->t_modes.t_erase)
    {
        ch = unputc(&tp->t_inq);
        if (ch == tp->t_modes.t_esc)
            goto NORMAL;
        if (ch == T_DELIM)
            putc(ch, &tp->t_inq);
        else if (ch != -1)    /* Avoid case where unputc returned emptyness */
            tterase(ch, tp);
    }
    else if (c == tp->t_modes.t_kill)
    {
        ch = unputc(&tp->t_inq);
        if (ch == tp->t_modes.t_esc)
            goto NORMAL;
        while (ch != -1 && ch != T_DELIM)
            ch = unputc(&tp->t_inq);
        if (ch != -1)
            putc(ch, &tp->t_inq);
        ttkill(tp);
    }
    else if (c == tp->t_modes.t_replay)
        ttreplay(tp);
    else
    {
NORMAL:
        if (tp->t_inq.c_cc >= TTYHOG)
        {
            ttyoutput('\7', tp);    /* Bell */
            return;
        }

        if ((t_flags & TI_XONXOFF) == TI_XONXOFF)
            if (tp->t_inq.c_cc >= TTYHOG - TTYWARN)
            {
                if (!(tp->t_state & TSI_XOFFHNG))
                {
                    ttyoutput(CXOFF, tp);
                    tp->t_state =| TSI_XOFFHNG;
                }
            }

        if (t_flags & TI_ONECASE)
        {
            if (c >= 'A' && c <= 'Z')   /* Lowercase it */
                c =+ 'a' - 'A';

            ch = unputc(&tp->t_inq);
            if (ch == tp->t_modes.t_esc && !(c & ~0177) && maptab[c])
                c = maptab[c];
            else if (ch != -1)
                putc(ch, &tp->t_inq);
        }

        if (c != tp->t_modes.t_eot)
        {
            if (c == T_DELIM)
                putc(T_USR_DELIM, &tp->t_inq);
            else
                putc(c, &tp->t_inq);
        }

        if (c == tp->t_modes.t_eot || tp->t_modes.t_breaks
            & (c&~0177? TB_NON_ASCII : class[c]))
        {
            wakeup(&tp->t_inq);
            if (tp->t_itp != 0)
                awake(tp->t_itp, 0);
            if (putc(T_DELIM, &tp->t_inq) == 0)
                tp->t_delct++;
            ch = t_flags & TI_BRK_ECHO;
        }
        else
            ch = t_flags & TI_NONBRK_ECHO;

        if (!(t_flags & TI_DEFER_ECHO)
        && (ch || (c == '\t' && t_flags & TI_TAB_ECHO)
               || (c == '\n' && t_flags & TI_CR_ECHO)))
        {
            ttyoutput (c, tp);
            ttstart (tp);
        }
    }
}

/* -------------------------- T T E R A S E ------------------------- */
/*
 * tterase(c, tp) erases c from the user's terminal. If the user is
 * on a printing terminal, it just prints <backslash><char>. If the
 * user is on a CRT, it erases the character itself.
 */
tterase(c, atp)
int c;
struct tty *atp;
{
    register struct tty *tp;
    register int len;
    register int flags;
    int ctype;

    tp = atp;
    flags = tp->t_modes.t_iflags;

/* Was the character echoed? */

    if (flags & TI_DEFER_ECHO)
        return;
    ctype = (c & ~0177)? TB_NON_ASCII : class[c];
    if ((ctype & tp->t_modes.t_breaks) != 0)
    {
        if ((flags & TI_BRK_ECHO) == 0)
            return;
    }
    else
        if ((flags & TI_NONBRK_ECHO) == 0)
            return;

/* Apparently -- undo it */

    if ((flags & TI_CRT) == 0)
    {
        ttyoutput('\\', tp);
        ttyoutput(c, tp);
        return;
    }

    if (c & ~0177)
        len = 0;
    else switch(partab[c] & P_CTYPE)
    {
    case P_CTRL:
    case P_VTFF:
    case P_LF:
    case P_CR:
        len = 0;
        break;

    case P_PRINTING:
        if ((ctype == TB_UPPER) && (flags & TI_ONECASE))
            len = 2;
        else
            len = 1;
        break;

    case P_BS:
    case P_HT:
        ttreplay(tp);
        return;
    }

    if ((c == tp->t_modes.t_erase) || (c == tp->t_modes.t_kill))
        len++;
    while(len--)
    {
        ttyoutput('\b', tp);
        ttyoutput(' ', tp);
        ttyoutput('\b', tp);
    }
}

/* -------------------------- T T K I L L --------------------------- */
ttkill(atp)
struct tty *atp;
{
    register struct tty *tp;
    register col;

    tp = atp;
    if ((tp->t_modes.t_iflags & TI_CRT) == 0)
        ttyoutput('\n', tp);
    else
        ttyoutput('\n', tp);
}
/* -------------------------- T T R E P L A Y ----------------------- */
ttreplay()
{
}

/* -------------------------- T T W R I T E ------------------------- */
/*
 * Called from the device's write routine after it has
 * calculated the tty-structure given as argument.
 */
ttwrite(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int c;
	register int count;

	count = 0;
	tp = atp;
	if ((tp->t_state&TS_CARR_ON)==0)
		return;
	while ((c=cpass())>=0) {
		spl5();
		count++;
		while (tp->t_outq.c_cc > TTHIWAT) {
			ttstart(tp);
			tp->t_state =| TS_ASLEEP;
			sleep(&tp->t_outq, TTOPRI);
		}
		spl0();
		ttyoutput(c, tp);
	}
	ttstart(tp);
	dpadd(u.u_tio,count);   /* rand:greep - tty accounting */
}

/* -------------------------- T T Y O U T P U T --------------------- */
/*
 * put character on TTY output queue, adding delays,
 * expanding tabs, and handling the CR/NL bit.
 * It is called both from the top half for output, and from
 * interrupt level for echoing.
 * The arguments are the character and the tty structure.
 */
ttyoutput(ac, tp)
struct tty *tp;
{
	register int c;
	register struct tty *rtp;
	register char *colp;
	int ctype;

	rtp = tp;
	c = ac&0177;
	/*
	 * Turn tabs to spaces as required
	 */
        if (c=='\t' && rtp->t_modes.t_oflags&TO_XTABS) {
		do
			/* changed from ttyoutput call sfh */
			if( putc( ' ',&rtp->t_outq ) )
				return;
		while( ++rtp->t_pos.t_col&07 );
		return;
	}
	/*
	 * for upper-case-only terminals,
	 * generate escapes.
	 */
        if (rtp->t_modes.t_oflags&TO_ONECASE) {
		colp = "({)}!|^~'`";
		while(*colp++)
			if(c == *colp++) {
				ttyoutput('\\', rtp);
				c = colp[-2];
				break;
			}
		if ('a'<=c && c<='z')
			c =+ 'A' - 'a';
	}
	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
        if (c=='\n' && rtp->t_modes.t_oflags&TO_CRMOD)
		ttyoutput('\r', rtp);
	if (putc(c, &rtp->t_outq))
		return;
	/*
	 * Calculate delays.
	 * The numbers here represent clock ticks
	 * and are not necessarily optimal for all terminals.
	 * The delays are indicated by characters above 0200,
	 * thus (unfortunately) restricting the transmission
	 * path to 7 bits.
	 */
	colp = &rtp->t_pos.t_col;
        ctype = partab[c];
        c = 0;

        switch (ctype & P_CTYPE) {

        /* ordinary */
        case P_PRINTING:
                (*colp)++;

        /* non-printing */
        case P_CTRL:
                break;

        /* backspace */
        case P_BS:
                if (*colp)
                        (*colp)--;
                break;

        /* newline */
        case P_LF:
                ctype = rtp->t_modes.t_oflags & TO_NL_DELAY;
                if (ctype == TO_NL_ANN_ARBOR) {
                    if (rtp->t_modes.t_speeds == ((B_9600<<L_SPEED)|B_9600))
                        putc('\020', &rtp->t_outq);
                } else if (ctype == TO_NL_37) { /* tty 37 */
                        if (*colp)
                                c = max((*colp>>4) + 3, 6);
                } else if (ctype == TO_NL_VT05) { /* vt05 */
                        c = 6;
                } else if (ctype == TO_NL_ECD) { /* ECD Translex */
                        c = 1;
                }
                *colp = 0;

                break;

        /* tab */
        case P_HT:
                ctype = rtp->t_modes.t_oflags & TO_TAB_DELAY;
                if(ctype == TO_TAB_37) { /* tty 37 */
                        c = 1 - (*colp | ~07);
                        if(c < 5)
                                c = 0;
                }
                *colp =| 07;
                (*colp)++;
                break;

        /* vertical motion */
        case P_VTFF:
                if(rtp->t_modes.t_oflags & TO_VT_DELAY) /* tty 37 */
                        c = 0177;
                break;

        /* carriage return */
        case P_CR:
                ctype = rtp->t_modes.t_oflags & TO_CR_DELAY;
                if (ctype == TO_CR_TN300) { /* tn 300 */
                        c = 5;
                } else
                if (ctype == TO_CR_TI700) { /* ti 700 */
                        c = 10;
                }
                *colp = 0;
        }
        if(c)
                putc(c|0200, &rtp->t_outq);
}

/* -------------------------- T T S T A R T ------------------------- */
/*
 * Start output on the typewriter. It is used from the top half
 * after some characters have been put on the output queue,
 * from the interrupt routine to transmit the next
 * character, and after a timeout has finished.
 * If the TS_SSTART bit is off for the tty the work is done here,
 * using the protocol of the single-line interfaces (KL, DL, DC);
 * otherwise the address word of the tty structure is
 * taken to be the name of the device-dependent startup routine.
 */
ttstart(atp)
struct tty *atp;
{
	register int *addr, c;
	register struct tty *tp;
	int sps;
        struct { int (*func)(); };
/*
 * structure of device registers for KL, DL, and DC
 * interfaces-- more particularly, those for which the
 * TS_SSTART bit is off and can be treated by general routines
 * (that is, not DH).
 */
        struct {
            int ttrcsr;
            int ttrbuf;
            int tttcsr;
            int tttbuf;
        };
        extern ttrstrt();

        tp = atp;
	sps = PS->integ;
	spl5();
	addr = tp->t_addr;
	if (tp->t_state&TS_SSTART) {
		(*addr.func)(tp);
		goto out;
	}
	if ((addr->tttcsr&DONE)==0 ||
			tp->t_state&(TS_TIMEOUT|TSO_XOFFHNG))  /* agn -XON/XOFF */
		goto out;
	if ((c=getc(&tp->t_outq)) >= 0) {
		if (c<=0177)
			addr->tttbuf = c | (partab[c]&0200);
		else {
			timeout(ttrstrt, tp, c&0177);
			tp->t_state =| TS_TIMEOUT;
		}
	}
	out:
		PS->integ = sps;
}

/* -------------------------- T T R S T R T ------------------------- */
/*
 * Restart typewriter output following a delay
 * timeout.
 * The name of the routine is passed to the timeout
 * subroutine and it is called during a clock interrupt.
 */
ttrstrt(atp)
{
	register struct tty *tp;

	tp = atp;
	tp->t_state =& ~TS_TIMEOUT;
	ttstart(tp);
}

/* -------------------------- T T Y C A P --------------------------- */
/*
 * Note: this routine was generated by tracing the behavior of ttread and
 * ttwrite under all possible conditions. Hence it is useful for any char
 * mode device which uses them.
 * Originally coded Sept 27 1978 Dan Franklin @ BBN
 */
ttycap(atp, v)
   struct tty *atp;
   int *v;
   {
   register struct tty *tp;

   tp = atp;
   if ((tp->t_state & TS_CARR_ON) == 0) /* Carrier gone? */
      {
      *v++ = -1;
      *v = -1;
      return;  /* That's all, folks. */
      }

   if (tp->t_delct == 0)
      *v = 0;
   else
/*
 * This is count of all available data.
 * (inqsize minus the T_DELIMs put in by ttyinput.)
 */
      *v = tp->t_inq.c_cc - tp->t_delct;
   v++;

   if (tp->t_outq.c_cc > TTHIWAT) /* Write will definitely block */
      *v = 0;
   else
/*
 * This is how much room is actually left before ttwrite blocks.
 * If tabs are being expanded, or padding put out, or LF mapped to
 * CR-LF, or chars echoed, etc., a write of this value will block.
 */
      *v = TTHIWAT - tp->t_outq.c_cc + 1;
   }
