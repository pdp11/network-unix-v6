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
#include "../h/tty.h"
#include "../h/proc.h"

/* The entire tty driver is #ifdef'ed on NEWTTY. */

# ifdef NEWTTY

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

/* -------------------------- T T Y I N I T ------------------------- */
/*
 * ttyinit(tp) initializes a new tty structure.
 * It sets all function characters, sets TB_NL as the lone break class,
 * and clears all input and output flags except TI_CLR_MSB and the echo flags.
 * Speed is set to 300 baud for historical reasons; it's actually pretty
 * irrelevant as long as it isn't 0 or 134.5.
 */
ttyinit(atp)
struct tty *atp;
{
    register struct tty *tp;

    tp = atp;
    tp->t_modes.t_spcl[TC_ERASE] = CERASE;
    tp->t_modes.t_spcl[TC_KILL] = CKILL;
    tp->t_modes.t_spcl[SIGINT] = CINTR;
    tp->t_modes.t_spcl[SIGQIT] = CQUIT;
    tp->t_modes.t_spcl[TC_ESC] = CESC;
    tp->t_modes.t_spcl[TC_EOF] = CEOT;
    tp->t_modes.t_spcl[TC_REPLAY] = CREPLAY;

    tp->t_modes.t_breaks = TB_NL;
    tp->t_modes.t_oflags = TO_CLR_MSB;
    tp->t_modes.t_iflags = TI_CLR_MSB | TI_BRK_ECHO | TI_NONBRK_ECHO;

    tp->t_modes.t_pflags = 0;
    tp->t_modes.t_ispeed = B300;
    tp->t_modes.t_ospeed = B300;

    tp->t_modes.t_width = 0;
    tp->t_modes.t_len = 0;
    tp->t_modes.t_pagelen = 0;
    tp->t_modes.t_term = TY_UNKNOWN;

    tp->t_pos.t_col = 0;
    tp->t_pos.t_line = 0;
}


/* -------------------------- M O D T T Y ------------------------------- */
/*
 * The routine implementing the modtty system call.
 * first arg is type.
 * Second arg is pointer to user's buffer; third arg is length.
 */
modtty()
{
	struct modes v;     /* Actually modes, cursor, tty state, or sgtty structure */
        register char *up, *vp;
        register int ulen;
        int flag;
        int *wp;

        vp = &v;
        flag = u.u_arg[0];
        up = u.u_arg[1];
        ulen = u.u_arg[2];
        if (ulen > sizeof(v))
            ulen = sizeof(v);

        if (flag & M_SET)
        {
/* Read in existing structure in case user-supplied structure not long enough */
	    sgtty(&v, (flag & M_TYPE) | M_GET, &v);
            while (ulen-- > 0)
                *vp++ = fubyte(up++);
/* Maintain compatibility with older device routines (see stty() ) */
            wp = &v;
            u.u_arg[0] = *wp++;
            u.u_arg[1] = *wp++;
            u.u_arg[2] = *wp++;
            sgtty(0, flag, &v);
        }
        else
            sgtty(&v, flag, &v);

        if (u.u_error)
                return;
        if (flag & M_GET)
            while (ulen-- > 0)
                subyte(up++, *vp++);
}

/* -------------------------- T T Y S T T Y ------------------------- */
/*
 * ttystty -- device driver utility function
 * Called from device's sgtty routine with the address of the
 * device's tty structure.
 * If flag & M_GET, gtty is being done and the information is
 * passed back in v;
 * if flag & M_SET, stty is being done and v contains the input information.
 *
 * The value returned is 0 if stty was done, nonzero otherwise.
 */
ttystty(tp, junk, flag, av)
struct tty *tp;
char *junk;
int flag;
char *av;
{
    register char *p;
    register char *v;
    register char *top;
    char *base;

    v = av;
    if (flag & M_OWAIT)
	ttwait(tp);
    if (flag & M_IFLUSH)
	iflushtty(tp);
    if (flag & M_OFLUSH)
	oflushtty(tp);

    switch(flag & M_TYPE)
    {
    case M_CURSOR:
	base = &(tp->t_pos);
	top = &(tp->t_pos) + 1;
	goto common;

    case M_MODES:
	base = &(tp->t_modes); /* Get base */
	top = &(tp->t_modes) + 1;

    common:
	if (flag & M_GET)
	    for (p = base; p < top;)
		*v++ = *p++;
	else if (flag & M_SET)
	    for (p = base; p < top;)
		*p++ = *v++;
	break;

    default:
	u.u_error = EINVAL;
	return(1);

    case M_SGTTY:
	if (flag & M_GET)
	    mod2g(tp, v);
	else if (flag & M_SET)
	    s2mod(v, tp);
	break;

    case 0:
	break;

    case M_STATE:
	if (flag & M_GET)
	{
	    v->m_state = tp->t_state;
	    v->m_rawq = tp->t_rawq.c_cc;
	    v->m_outq = tp->t_outq.c_cc;
            v->m_recsize = tp->t_canq.c_cc;
	    break;
	}
	if (flag & M_SET)
	{
	    u.u_error = EINVAL;
	    return(1);
	}
    }

    if (flag & M_SET)
	if (tp->t_modes.t_iflags & TI_LOCAL_ECHO)
	    tp->t_modes.t_iflags =& ~(TI_BRK_ECHO|TI_NONBRK_ECHO|TI_DEFER_ECHO);

    if (flag & M_ECHO)	/* Pre-echo, for deferred echo mode */
        ecanon(tp);

    if (flag & M_SET)
	return(0);
    else
	return(1);
}

/* -------------------------- S 2 M O D ----------------------------- */
/*
 * s2mod(stp, mtp) applies an old-style stty (using the table pointed to
 * by stp) to a new-style tty structure (pointed to by tp).
 * Warning: this code depends on the fact that certain TI_, TO_, and TP_
 * defines have values identical to certain old tty flag word values.
 */
s2mod(astp, atp)
struct sgttybuf *astp;
struct tty *atp;
{

    register struct sgttybuf *stp;
    register struct tty *tp;
    register int flags;

    stp = astp;
    tp = atp;

/* Some things haven't changed */

    tp->t_modes.t_ospeed = stp->s_ospeed;
    tp->t_modes.t_ispeed = stp->s_ispeed;
    tp->t_modes.t_spcl[TC_ERASE] = stp->s_erase;
    tp->t_modes.t_spcl[TC_KILL] = stp->s_kill;

    flags = stp->s_flags;

/* Physical properties */

    tp->t_modes.t_pflags =& ~ TP_HUPCL;
    tp->t_modes.t_pflags =| (flags & HUPCL);	/* HUPCL == TP_HUPCL */
/*
 * The parity bits have changed implementation.
 * This change, although it results in more complicated
 * compatibility code, simplifies driver code.
 */
    tp->t_modes.t_pflags =& ~ TP_PENABLE;
    if ( (flags & (EVENP|ODDP)) == EVENP || (flags & (EVENP|ODDP)) == ODDP)
    {
	tp->t_modes.t_pflags =& ~ TP_ODD;
	if (flags & ODDP)
	    tp->t_modes.t_pflags =| TP_ODD;
	tp->t_modes.t_pflags =| TP_PENABLE;
    }

/* Input properties */

    tp->t_modes.t_iflags =& ~(
			    TI_LOCAL_ECHO|
			    TI_CRMOD|
			    TI_ONECASE|

			    TI_BRK_ECHO|TI_NONBRK_ECHO|
			    TI_NOSPCL
			    );

    tp->t_modes.t_iflags =| flags & (
			    CRMOD|  /* == TI_CRMOD */
			    LCASE|  /* == TI_ONECASE */
			    RAW     /* == TI_NOSPCL */
			    );

    if (flags & ECHO)
	tp->t_modes.t_iflags =| (TI_BRK_ECHO|TI_NONBRK_ECHO);

/* Implement speed hack for TELNET server */

    if (tp->t_modes.t_ispeed == B134)
	tp->t_modes.t_iflags =| TI_LOCAL_ECHO;

/* Output properties */

    tp->t_modes.t_oflags =& ~(
			    TO_XTABS|
			    TO_NL_DELAY|
			    TO_TAB_DELAY|
			    TO_CR_DELAY|
			    TO_VT_DELAY|
			    TO_CRMOD|
			    TO_ONECASE|
			    TO_XONXOFF|
			    TO_LITERAL
			    );

    tp->t_modes.t_oflags =| flags & (
			    XTABS|	/* == TO_XTABS */
			    NLDELAY|	/* == TO_NL_DELAY */
			    TBDELAY|	/* == TO_TAB_DELAY */
			    CRDELAY|	/* == TO_CR_DELAY */
			    VTDELAY|	/* == TO_VT_DELAY */
			    CRMOD|	/* == TO_CRMOD */
			    LCASE	/* == TO_ONECASE */
			    );

/* "tab3" is a synonym for XON-XOFF processing */

    if ((flags & TBDELAY) == TBDELAY)	    /* XON-XOFF */
	tp->t_modes.t_oflags =| TO_XONXOFF;

/*
 * BIT8IO on means TO_CLR_MSB and TI_CLR_MSB off, TO_LITERAL on.
 * If BIT8IO is off, it may be in the act of being turned off,
 * which is ascertained by checking BIT8IO in t_flags. If so,
 * TO_CLR_MSB and TI_CLR_MSB are turned on. If not, TO_CLR_MSB is left
 * alone, in case udelay has been turned on via modtty.
 */
    if (flags & BIT8IO)
    {
	tp->t_modes.t_iflags =& ~ TI_CLR_MSB;
	tp->t_modes.t_oflags =& ~ TO_CLR_MSB;
	tp->t_modes.t_oflags =| TO_LITERAL;
    }
    else if (tp->t_flags & BIT8IO)
    {
	tp->t_modes.t_oflags =| TO_CLR_MSB;
	tp->t_modes.t_iflags =| TI_CLR_MSB;
	tp->t_modes.t_oflags =& ~ TO_LITERAL;
    }

    tp->t_modes.t_breaks = TB_LF;
    if (flags & RAW)
    {
	tp->t_modes.t_breaks = TB_ALL;
	tp->t_modes.t_oflags =& ~ TO_XONXOFF;
    }

    tp->t_flags = flags;

}

/* -------------------------- M O D 2 G ----------------------------- */
/*
 * mod2g(mtp, stp) translates a new-styl tty structure (as pointed to by mtp)
 * into the gtty structure pointed to by stp.
 * Warning: this code depends on the fact that certain TO_ TI_ and TP_ #defines
 * are defined to the same values as certain sgttybuf flag word #defines.
 * They are flagged with comments.
 */
mod2g(amtp, astp)
    struct tty *amtp;
    struct sgttybuf *astp;
{
    register struct tty *mtp;
    register struct sgttybuf *stp;
    register int flags;

    mtp = amtp;
    stp = astp;

/* Some things haven't changed */

    stp->s_ospeed = mtp->t_modes.t_ospeed;
    stp->s_ispeed = mtp->t_modes.t_ispeed;
    stp->s_erase = mtp->t_modes.t_spcl[TC_ERASE];
    stp->s_kill = mtp->t_modes.t_spcl[TC_KILL];

    flags = 0;	    /* Build up result */

/* Physical properties */

    flags =| (mtp->t_modes.t_pflags & TP_HUPCL);

    if (mtp->t_modes.t_pflags & TP_PENABLE)
	if ((mtp->t_modes.t_pflags & TP_PARITY) == TP_ODD)
	    flags =| ODDP;
	else
	    flags =| EVENP;
    else
	flags =| (EVENP|ODDP);

/* Other properties */

    flags =| (
	     mtp->t_modes.t_iflags & (
		TI_CRMOD|	/* == CRMOD */
		TI_ONECASE|	/* == LCASE */
		TI_BRK_ECHO|	/* == ECHO */
		TI_NOSPCL	/* == RAW */
	     )) | (
	     mtp->t_modes.t_oflags & (
		TO_XTABS|	/* == XTABS */
		TO_ONECASE|	/* == LCASE */
		TO_CRMOD|	/* == CRMOD */
		TO_TAB_DELAY|	/* == TBDELAY */
		TO_VT_DELAY|	/* == VTDELAY */
		TO_CR_DELAY|	/* == CRDELAY */
		TO_NL_DELAY	/* == NLDELAY */
	    ));

    if (mtp->t_modes.t_oflags & TO_XONXOFF)
	flags =| TBDELAY;	/* XONXOFF is tab3 */
    if ( (mtp->t_modes.t_oflags & (TO_CLR_MSB|TO_LITERAL)) == TO_LITERAL
      && (mtp->t_modes.t_iflags & TI_CLR_MSB) == 0 )
	flags =| BIT8IO;
    stp->s_flags = flags;
}

/* -------------------------- T T W A I T --------------------------- */
/*
 * Wait for output to drain.
 */
ttwait(atp)
struct tty *atp;
{
        register struct tty *tp;

        tp = atp;
        spl5();
        tp->t_state =& ~TSO_XOFFHNG;        /* agn - XON/XOFF */
        ttstart(tp);                        /* agn - XON/XOFF */
        while (tp->t_outq.c_cc) {
                tp->t_state =| TS_ASLEEP;
                sleep(&tp->t_outq, TTOPRI);
        }
        spl0();
}

/* -------------------------- I F L U S H T T Y --------------------- */
/*
 * Flush input queue.
 */
iflushtty(atp)
struct tty *atp;
{
	register struct tty *tp;
	register int sps;

	tp = atp;
	sps = PS->integ;
	spl5();
	while (getc(&tp->t_rawq) >= 0);
        while (getc(&tp->t_canq) >= 0);
	if (tp->t_state & TSI_XOFFHNG)
	{
	    ttyoutput(CXON, tp);
	    tp->t_state =& ~ TSI_XOFFHNG;
	    ttstart(tp);
	}
	lvpolite(tp);
	PS->integ = sps;
	wakeup(&tp->t_rawq);
}

/* -------------------------- O F L U S H T T Y --------------------- */
/*
 * Flush output queue.
 */
oflushtty(atp)
struct tty *atp;
{
	register struct tty *tp;

	tp = atp;
	while (getc(&tp->t_outq) != -1);
	wakeup(&tp->t_outq);
	if (tp->t_state & (TSO_XOFFHNG|TS_ENDPAGE))
	{
	    tp->t_state =& ~ (TSO_XOFFHNG|TS_ENDPAGE);
	    ttstart(tp);
	}
}

/* -------------------------- F L U S H T T Y ----------------------- */
/*
 * flush all TTY queues
 */
flushtty(atp)
struct tty *atp;
{
        register struct tty *tp;

        tp = atp;
        iflushtty(tp);
        oflushtty(tp);
}

/* -------------------------- W F L U S H T T Y --------------------- */
/*
 * Wait and flush.
 */
wflushtty(tp)
    struct tty * tp;
{
    ttwait(tp);
    flushtty(tp);
}

/* -------------------------- C L A S S   A R R A Y ----------------- */

#ifndef SMALL
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
TB_LOWER, TB_LOWER, TB_LOWER, TB_BAL,	TB_OTHER, TB_BAL,   TB_OTHER, TB_DEL
};

#endif

/* -------------------------- I S _ W A K E U P (macro) ------------- */
/*
 * This macro is used to determine whether a char in t_rawq is wakeup char.
 */
#ifndef SMALL
#define is_wakeup(c,tp) (c==T_EOFMARK?1:tp->t_modes.t_breaks & (c&~0177?TB_NON_ASCII:class[c]))
#endif

/*
 * This version is used when there is no break class table. It divides chars into
 * the classes TB_NON_ASCII, TB_NL, and not TB_NL.
 */
#ifdef SMALL
#define is_wakeup(c,tp) (c==T_EOFMARK?1:tp->t_modes.t_breaks&(c&~0177?TB_NON_ASCII:c=='\n'?TB_NL:~TB_NL))
#endif

/* -------------------------- T T R E A D --------------------------- */
/*
 * ttread -- move chars from input queue to user's buffer
 * Called from device's read routine after it has
 * calculated the tty-structure pointer.
 *
 * Read chars and copy to user until t_canq empty or passc returns negative.
 * Zero line for page-length control; clear ZOOM mode.
 */
ttread(atp)
struct tty *atp;
{
    register struct tty *tp;

    tp = atp;

    while (tp->t_canq.c_cc == 0)
    {
        if ((tp->t_state & TS_CARR_ON) == 0)
        {
            spl0();
            return;
        }
        spl6();
        if (tp->t_rawq.c_cc == 0 || ecanon(tp) == 0)
        {
            tp->t_state =| TSI_WAKEUP;
            sleep(&tp->t_rawq, TTIPRI);
        }
        else
            break;  /* Skip test on canq.c_cc because 
                     * ecanon can return nonzero with an empty canq
                     * (which means that the next char was EOT).
                     */
    }
    spl0();

/* Begin new page */

    tp->t_pos.t_line = 0;
    if (tp->t_state & TS_ENDPAGE)
    {
        tp->t_state =& ~ TS_ENDPAGE;
        ttstart(tp);
    }
    tp->t_state =& ~ TS_ZOOM;

/* Transfer chars */

    while (tp->t_canq.c_cc != 0)
        if (passc(getc(&tp->t_canq)) < 0)
            break;

    if (tp->t_canq.c_cc == 0) /* Ate break character */
        tp->t_state =& ~ TS_ALREADY_ECHOED;

    if (tp->t_state & TSI_XOFFHNG)
        if (tp->t_rawq.c_cc <= TTYWARN)
        {
            tp->t_state =& ~TSI_XOFFHNG;
            ttyoutput(CXON, tp);
            ttstart(tp);
        }

    return;
}

/* -------------------------- E C A N O N --------------------------- */
/*
 * Transfer characters from t_rawq to t_canq, echoing if deferred echo is on.
 * The name is somewhat misleading, as canonicalization is now done in
 * the ttyinput routine. Called from ttread when characters are present,
 * and from ttystty upon an M_ECHO control order. Uses TS_ALREADY_ECHOED
 * to indicate whether the characters currently in the input queue,
 * up to the first break character, have been echoed yet.
 * If no break character is found, the t_canq is put back in the t_rawq and
 * 0 is returned. If a break character is found, returns nonzero.
 * Note that break characters are
 */
ecanon(atp)
    struct tty *atp;
{
    register struct tty *tp;
    register int c;
    register int echoflag;

    tp = atp;
    if (tp->t_modes.t_iflags & TI_DEFER_ECHO)
        echoflag = tp->t_state & TS_ALREADY_ECHOED;
    else
        echoflag = 1;

    for (;;)
    {
	c = getc(&tp->t_rawq);
	if (c == -1)	    /* Empty -- undo */
	{
	    tp->t_rawq.c_cc = tp->t_canq.c_cc;
	    tp->t_rawq.c_cf = tp->t_canq.c_cf;
	    tp->t_rawq.c_cl = tp->t_canq.c_cl;
	    tp->t_canq.c_cc = tp->t_canq.c_cf = tp->t_canq.c_cl = 0;
	    tp->t_state =| (TS_ALREADY_ECHOED|TS_ECHO);
	    return(0);
	}
	if (echoflag == 0)
	    echo(c, tp);
	if (is_wakeup(c, tp))
	{
	    if (c != T_EOFMARK)
		putc(c, &tp->t_canq);
	    tp->t_state =& ~ TS_ALREADY_ECHOED;
	    return(1);
	}
        else
            putc(c, &tp->t_canq);
    }
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

/* -------------------------- T T Y I N P U T ----------------------- */
/*
 * ttyinput(c, tp)
 * Puts c on input queue in tty structure pointed to by tp.
 * Called from interrupt side when a char received.
 * Handles X-ON X-OFF, special function chars, break char classes, etc.
 *
 * NOTES:
 *   1. t_modes.t_erase, etc. => t_modes.t_spcl[NCHARS]
 *	#define TC_ERASE, TC_KILL, TC_ESC, TC_REPLAY, TC_EOF
 *	Note that the INTR and QUIT chars go in positions SIGINT and SIGQIT resp
 *	Also need state TS_ESC for escape processing.
 *   2. TC_EOF, when typed, is turned into T_EOFMARK internally, which the user
 *	can't type.
 *   3. TI_NOSPCL turns all spcl chars on or off. Individual ones can be
 *	turned off by assigning 0377 (or some other char which is
 *	unlikely to be typed) to them, which is sufficient for most purposes.
 *   4. Now possible to put special functions on keys w. values > 0177.
 *   5. There is no longer a delimiter char or t_delct. Instead ttread, via 
 *      ecanon, decides when it has genuinely reached a break char. This is
 *      required by the definition of deferred echo.
 *   8. TC_ESC is now a never-break char.
 */
ttyinput(ac, atp)
    int ac;
    struct tty *atp;
{
    register int t_flags, c;
    register struct tty *tp;
    int esc;

    tp = atp;
    c = ac;
    t_flags = tp->t_modes.t_iflags;

    c =& 0377;
    if (t_flags & TI_CLR_MSB)
	c =& 0177;

    if (c == CXOFF && tp->t_modes.t_oflags & TO_XONXOFF)
    {
	tp->t_state =| TSO_XOFFHNG;
	return;
    }
/*
 * XON may either be from terminal, to continue, or from user,
 *  to go to next page. If the driver is XOFFHNG then the XON
 *  is probably from the terminal.
 * CZOOM is like CXON but causes later CPAUSEs to be ignored.
 */
    if (c == CXON || (c == CZOOM && tp->t_modes.t_pagelen != 0))
	if (tp->t_modes.t_oflags & TO_XONXOFF
	|| tp->t_state & (TSO_XOFFHNG|TS_ENDPAGE))
	{
	    if (tp->t_state & TSO_XOFFHNG && c == CXON)
		tp->t_state =& ~TSO_XOFFHNG;
	    else
		tp->t_state =& ~TS_ENDPAGE;
	    if (c == CZOOM)
		tp->t_state =^ TS_ZOOM; /* Toggle */
	    ttstart(tp);
	    return;
	}

    if (c == '\r' && (t_flags & TI_CRMOD))
	c = '\n';

    esc = tp->t_state & TS_ESC;
    tp->t_state =& ~ TS_ESC;

    if (t_flags & TI_NOSPCL)
	goto NORMAL;
/*
 * See if special. Note pun of t_flags. Also note use of SIGINT and SIGQIT
 * as indices into t_spcl array as well as signal values.
 */
    for (t_flags = 0; t_flags < sizeof(tp->t_modes.t_spcl); t_flags++)
    {
	if ((tp->t_modes.t_spcl[t_flags] & 0377) == c && c != 0377)
	{
	    if (esc)
	    {
		if (t_flags != TC_ESC)
		    unputc(&tp->t_rawq);    /* Delete escape char from queue */
		goto NORMAL;
	    }
	    switch(t_flags)
	    {
	    case SIGINT:
	    case SIGQIT:
		signal(tp->t_pgrp, t_flags);
		oflushtty(tp);
		ttkill(tp, 1);
		iflushtty(tp);
		ttstart(tp);
		break;

	    case TC_KILL:
	    case TC_ERASE:

		kill_loop:
		    c = unputc(&tp->t_rawq);
		    if (is_wakeup(c, tp))   /* Can't edit past break char */
		    {
			putc(c, &tp->t_rawq);
			lvpolite(tp);
		    }
		    else if (c != -1)	/* Avoid end-of-queue */
			if (t_flags == TC_ERASE)
			    tterase(c, tp);
			else
			{
			    if (t_flags == TC_KILL)
			    {
				ttkill(tp, 1);
				t_flags++;  /* != TC_ERASE or TC_KILL */
			    }
			    goto kill_loop;
			}
		    if (tp->t_rawq.c_cc == 0)
			lvpolite(tp);
		break;

	    case TC_ESC:
		tp->t_state =| TS_ESC;
		goto NORMAL;

	    case TC_REPLAY:
		ttreplay(tp);
		break;

	    case TC_EOF:
		c = T_EOFMARK;
		goto NORMAL;
	    }
	    return;	/* That's all for specials */
	}
    }

NORMAL:
    t_flags = tp->t_modes.t_iflags;

    if (tp->t_rawq.c_cc >= TTYHOG)
    {

OVERFLOW:
        oflushtty(tp);          /* Prevent bells, etc. from filling up */
	ttyoutput('\7', tp);	/* Bell */
	ttstart(tp);
	tp->t_state =& ~ TS_ESC;
	return;
    }

    if ((t_flags & TI_XONXOFF) == TI_XONXOFF)
	if (tp->t_rawq.c_cc >= TTYHOG - TTYWARN)
	{
	    if (!(tp->t_state & TSI_XOFFHNG))
	    {
		ttyoutput(CXOFF, tp);
		ttstart(tp);
		tp->t_state =| TSI_XOFFHNG;
	    }
	}

    if (t_flags & TI_ONECASE)
    {
	if (c >= 'A' && c <= 'Z')   /* Lowercase it */
	    c =+ 'a' - 'A';

	if (esc && !(c & ~0177) && maptab[c])
	{
	    c = maptab[c];
	    unputc(&tp->t_rawq);
	}
    }

    if (putc(c, &tp->t_rawq) != 0)
	goto OVERFLOW;

    if ( (t_flags & TI_DEFER_ECHO) == 0     /* If not deferred echo */
     || (tp->t_state & TSI_WAKEUP) != 0     /* or if it is, but read sleeping */
     || (tp->t_state & TS_ECHO) != 0        /* or special immediate echo */
       )
    {
        echo(c, tp);
        tp->t_state =| TS_ALREADY_ECHOED;
    }

    if ((tp->t_state & TS_ESC) == 0 && is_wakeup(c, tp))
    {
        ttwake(tp);
        tp->t_state =& ~ TS_ECHO;
        lvpolite(tp);
    }
    else
    {
        if (tp->t_modes.t_iflags & TI_POLITE)
            tp->t_state =| TSO_POLITE;
    }
}

/* -------------------------- L V P O L I T E ----------------------- */
/*
 * Leave polite state; if it was on, wakeup().
 */
lvpolite(atp)
    struct tty *atp;
{
    register struct tty *tp;

    tp = atp;
    if (tp->t_state & TSO_POLITE)
    {
	tp->t_state =& ~ TSO_POLITE;
	wakeup(&tp->t_outq.c_cf);
    }
}

/* -------------------------- T T W A K E --------------------------- */
/*
 * Called to indicate end of sequence of nonbreaks ended by break char.
 * If there is a sleeping ttread, wakes it up. In any
 * case it does await-awakening.
 */
ttwake(atp)
    struct tty * atp;
{
    register struct tty *tp;

    tp = atp;
    if (tp->t_state & TSI_WAKEUP)
    {
	tp->t_state =& ~TSI_WAKEUP;
	wakeup(&tp->t_rawq);
    }
    if (tp->t_itp != 0)
	awake(tp->t_itp, 0);
}

/* -------------------------- E C H O ------------------------------- */
/*
 * Echoes given character according to TI_BRK_ECHO, TI_NONBRK_ECHO flags.
 * Understands that T_EOFMARK is a break character and should be echoed
 * as t_spcl[TC_EOF].
 */
echo(c, atp)
    int c;
    struct tty *atp;
{
    register struct tty *tp;
    register int choice;

    tp = atp;
    if (is_wakeup(c, tp))
        choice = TI_BRK_ECHO;
    else
        choice = TI_NONBRK_ECHO;

    if (c == T_EOFMARK)
	c = tp->t_modes.t_spcl[TC_EOF];
    if ( (c == '\t' && (tp->t_modes.t_iflags & TI_TAB_ECHO))
         || (c == '\n' && (tp->t_modes.t_iflags & TI_CR_ECHO))
         || (choice & tp->t_modes.t_iflags) )
    {
        ttyoutput (c, tp);
        ttstart (tp);
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

#ifndef SMALL
    if (flags & TI_DEFER_ECHO) /* If deferred echo, use wakeup flag */
	if ((tp->t_state & (TSI_WAKEUP|TS_ECHO)) == 0)
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
#endif
#ifdef SMALL
    if ( (flags & (TI_BRK_ECHO|TI_NONBRK_ECHO)) == 0)
	return;
#endif

/* Apparently -- undo it */

    if ((flags & TI_CRT) == 0)
    {
	ttyoutput('\\', tp);
	ttyoutput(c, tp);
	ttstart(tp);
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
	ttyoutput(' ', tp);
	len = 0;
	break;
    case P_HT:
	ttreplay(tp);
	return;
    }

    if ((c == tp->t_modes.t_spcl[TC_ERASE]) || (c == tp->t_modes.t_spcl[TC_KILL]))
	len++;
    while(len--)
    {
	ttyoutput('\b', tp);
	ttyoutput(' ', tp);
	ttyoutput('\b', tp);
    }
    ttstart(tp);
}

/* -------------------------- T T K I L L --------------------------- */
/*
 * Kill the line on the user's terminal. If flag is 1, indicate that
 * user's input has been erased.
 */
ttkill(atp, flag)
struct tty *atp;
int flag;
{
    register struct tty *tp;

    tp = atp;
    if (flag)
	echo(tp->t_modes.t_spcl[TC_KILL], tp);
    echo('\n', tp);
}


/* -------------------------- T T R E P L A Y ----------------------- */
/*
 * Replay queued-up input using peekc.
 */
ttreplay(atp)
struct tty *atp;
{
    register struct tty *tp;
    register int c;
    register int ps;
    struct clist peek;

    tp = atp;

    ttkill(tp, 0);

    peek.c_cf = tp->t_rawq.c_cf;
    peek.c_cl = tp->t_rawq.c_cl;
    peek.c_cc = tp->t_rawq.c_cc;

    if (peek.c_cc > 0)
    {
	tp->t_state =| (TSO_POLITE|TS_ECHO);
	while ((c = peekc(&peek)) != -1)
	{
	    echo(c, tp);

	/* Let pending interrupts in */

	    ps = PS->integ;
	    spl0();
	    PS->integ = ps;
	}
    }
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
		spl6();
		count++;
		while (tp->t_outq.c_cc > TTHIWAT) {
			ttstart(tp);
			if (tp->t_outq.c_cc <= TTHIWAT) /* (For ptys) */
			    break;
			tp->t_state =| TS_ASLEEP;
			sleep(&tp->t_outq, TTOPRI);
		}
/* Allow ttyinput to echo and set polite state */
		spl0();
		spl6();
/* Sleep if in polite state */
		while (tp->t_state & TSO_POLITE)
		    sleep(&tp->t_outq.c_cf, TTOPRI);
		spl0();
		ttyoutput(c, tp);
	}
	ttstart(tp);
	dpadd(u.u_tio, count);	 /* rand:greep - tty accounting */
}

/* -------------------------- T T Y O U T P U T --------------------- */
/*
 * Put character on TTY output queue, adding delays and page-length pauses,
 * expanding tabs, and handling the CR/NL bit.
 * ttyoutput is called both from ttwrite for output, and from
 * interrupt level for echoing.
 * The arguments are the character and the tty structure.
 */
ttyoutput(ac, tp)
struct tty *tp;
{
	register int c;
	register struct tty *rtp;
	register char *colp;
	int outc;
	int ctype;

	rtp = tp;
	c = ac & 0377;
	if (rtp->t_modes.t_oflags & TO_CLR_MSB)
	    c =& 0177;
/*
 * for upper-case-only terminals,
 * generate escapes.
 */
	if (rtp->t_modes.t_oflags & TO_ONECASE)
	{
	    colp = "({)}!|^~'`";
	    while (*colp++)
		if (c == *colp++)
		{
		    ttyoutput('\\', rtp);
		    c = colp[-2];
		    break;
		}
	    if ('a'<=c && c<='z')
		c =+ 'A' - 'a';
	}
/*
 * Determine special actions, including tab expansion,
 * LF->CRLF conversion, delays, and page-length pauses.
 * c will be set to a delay in clock ticks, or CPAUSE
 * for a page-length pause.
 * The delays and CPAUSE are indicated by characters above 0200,
 * thus (unfortunately) restricting the transmission
 * path to 7 bits.
 * The delay is not necessarily optimal for all terminals.
 */
        outc = c;       /* Save for later */

        colp = &rtp->t_pos.t_col;
        ctype = partab[c];
        c = 0;

        switch (ctype & P_CTYPE) {

	case P_PRINTING:
		if (rtp->t_modes.t_oflags & TO_AUTONL &&
		    (rtp->t_modes.t_width & 0377) <= (*colp & 0377) &&
		    (rtp->t_modes.t_width & 0377) > 3)
		{
		    ttyoutput('\n', rtp);
		    ttyoutput('-', rtp);
		    ttyoutput('-', rtp);
		    ttyoutput('>', rtp);
		}
		(*colp)++;
	case P_CTRL:
                break;

        case P_BS:
                if (*colp)
                    (*colp)--;
                break;

	case P_LF:

	    /* turn <nl> to <cr><lf> if desired. */

		if (rtp->t_modes.t_oflags & TO_CRMOD)
		    ttyoutput('\r', rtp);

		++rtp->t_pos.t_line;

	    /* Pause at end of page if desired. */

		if (rtp->t_modes.t_oflags & TO_XONXOFF
		&& rtp->t_modes.t_pagelen != 0
		&& rtp->t_pos.t_line >= rtp->t_modes.t_pagelen)
		{
		    c = CPAUSE;
		    rtp->t_pos.t_line = 0;
		    *colp = 0;
		    break;
		}

	    /* Otherwise, calculate delay */

		ctype = rtp->t_modes.t_oflags & TO_NL_DELAY;
		if (ctype == TO_NL_ANN_ARBOR) {     /* Put out ctrl-P at 9600 */
		    if (rtp->t_modes.t_oflags & TO_LITERAL)
			c = 0;
		    else if (rtp->t_modes.t_ospeed == B9600) {
			putc(outc, &rtp->t_outq); /* Put out nl */
			outc = '\020';	/* Later put out padding char */
		    }
		} else if (ctype == TO_NL_37) {
			if (*colp)
				c = max((*colp>>4) + 3, 6);
			if (c > 0177)
				c = 0177;
		} else if (ctype == TO_NL_VT05) { /* vt05 */
			c = 6;
		} else if (ctype == TO_NL_ECD) { /* ECD Translex */
			c = 1;
		}
		*colp = 0;

		break;

	case P_HT:

            /* Turn tabs to spaces if required */

                if (rtp->t_modes.t_oflags & TO_XTABS)
                {
                    do
                        if (putc(' ', &rtp->t_outq))
                            return;
                    while (++rtp->t_pos.t_col & 07);
                    return;
                }

            /* Otherwise delay */

                ctype = rtp->t_modes.t_oflags & TO_TAB_DELAY;
                if (ctype == TO_TAB_37)
                {
                    c = 1 - (*colp | ~07);
                    if (c < 5)
                        c = 0;
                }
                *colp =| 07;
                (*colp)++;
                break;

        case P_VTFF:

            /*
             * If page-length processing turned on,
             * and at least one line has been output, pause on FF
             */
                if (outc == '\014'
                && rtp->t_modes.t_oflags & TO_XONXOFF
                && rtp->t_modes.t_pagelen != 0
                && rtp->t_pos.t_line > 0)
                {
                    c = CPAUSE;
                    rtp->t_pos.t_line = 0;
                    break;
                }

            /* Otherwise delay */

                if (rtp->t_modes.t_oflags & TO_VT_DELAY) /* tty 37 */
                        c = 0177;
                break;

        case P_CR:
                ctype = rtp->t_modes.t_oflags & TO_CR_DELAY;
                if (ctype == TO_CR_TN300)
                        c = 5;
                else if (ctype == TO_CR_TI700)
                        c = 10;
                *colp = 0;
	}

/*
 * If 8-bit output mode is on, delays cannot be used.
 */
	if (rtp->t_modes.t_oflags & TO_LITERAL)
	    c = 0;

/*
 * Page-length pauses happen before the char gets put out.
 * Ordinary delays happen after.
 */

	if (c == CPAUSE)
	{
	    putc(c, &rtp->t_outq);
	    c = 0;
	}

	if (putc(outc, &rtp->t_outq))
		return;

	if (c)
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
	extern ttrstrt();
/*
 * structure of device registers for KL, DL, and DC
 * interfaces-- more particularly, those for which the
 * TS_SSTART bit is off and can be treated by general routines
 * (that is, not DH).
 */
	struct
	{
	    int ttrcsr;
	    int ttrbuf;
	    int tttcsr;
	    int tttbuf;
	};

	tp = atp;
	sps = PS->integ;
	spl5();

	addr = tp->t_addr;

	if (tp->t_state&TS_SSTART)
	{
	    (*addr.func)(tp);
	    goto out;
	}
	if ((addr->tttcsr&DONE)==0
	  || tp->t_state&(TS_TIMEOUT|TSO_XOFFHNG|TS_ENDPAGE))
	    goto out;
	if ((c=getc(&tp->t_outq)) >= 0)
	{
	    if (tp->t_modes.t_oflags & TO_LITERAL)
		addr->tttbuf = c;
	    else if (c <= 0177)
		addr->tttbuf = c | (partab[c]&0200);
	    else if (c == CPAUSE)
	    {
		if ((tp->t_state & TS_ZOOM) == 0)
		    tp->t_state =| TS_ENDPAGE;
	    }
	    else
	    {
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
 * Return (by setting vector) the read and write capacity of the given
 * tty structure. For deferred echo, this may mean examining the input
 * queue to see what ttread would do with it.
 * This routine was generated by tracing the behavior of
 * ttread and ttwrite under all possible conditions. Hence it is useful
 * for any char mode device which uses them.
 */
ttycap(atp, v)
   struct tty *atp;
   int *v;
   {
   register struct tty *tp;
   register int c;
   register int i;
   struct clist peek;

   tp = atp;
   if ((tp->t_state & TS_CARR_ON) == 0) /* Carrier gone? */
   {
      *v++ = -1;
      *v = -1;
      return;  /* That's all, folks. */
   }

   if (tp->t_canq.c_cc != 0)  /* If there is already a record, return that */
      *v = tp->t_canq.c_cc;
   else if (tp->t_rawq.c_cc == 0) /* Otherwise, if nothing there, return that */
      *v = 0;
   else
/*
 * Calculate count of data that ttread would get if it were called under
 * the current set of break characters.
 */
   {
      spl6();
      peek.c_cc = tp->t_rawq.c_cc;
      peek.c_cf = tp->t_rawq.c_cf;
      peek.c_cl = tp->t_rawq.c_cl;
      i = 0;
      *v = 0;
      while ((c = peekc(&peek)) != -1 && *v == 0)
      {
	 i++;
	 if (is_wakeup(c, tp))
	    *v = i;
      }
      spl0();
   }
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

/* ------------------------------------------------------------------ */
# endif NEWTTY
