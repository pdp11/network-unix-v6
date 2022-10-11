/*
 * Stuff relating to TELNET options and related stty calls.
 */
#include <stdio.h>
#include "globdefs.h"
#include "tnio.h"
#include "telnet.h"
#include "ttyctl.h"

#define MAX_OPTIONS 20

extern int termfd;

struct opt_tab
{
    char * name;
    int value;
    int (*testf)();
    int (*actf)();
    int (*subf)();
};

/* -------------------------- O P T I O N S ------------------------- */

extern int always();
extern int bintst(), binact();
extern int echotst(), echoact();
extern int vtsub();

static struct opt_tab options[MAX_OPTIONS] =
{
    {"binary", OPT_BINARY, bintst, binact, NULL},
    {"echo",   OPT_ECHO,   echotst, echoact, NULL},
    {"reconnect", OPT_RECONNECT, always, NULL, NULL},
#ifdef OPT_SUPPRESS_GA
    {"suppress-ga", OPT_SUPPRESS_GA, always, NULL, NULL},
#endif
    {"record-size", OPT_RECSIZE, always, NULL, NULL},
    {"status", OPT_STATUS, always, NULL, NULL},
    {"timing-mark", OPT_TMARK, always, NULL, NULL},
    {"rcte", OPT_RCTE, always, NULL, NULL},
    {"linewidth", OPT_LINEWIDTH, always, NULL, NULL},
    {"pagesize", OPT_PAGESIZE, always, NULL, NULL},
#ifdef VTELNET
    {"vterm", OPT_VTERM, always, NULL, vtsub},
#endif
};

/* -------------------------- O P T N A M E ------------------------- */
/*
 * Return name of option, given number. If none, return NULL.
 */
char *
optname(optnum)
    int optnum;
{
    int i;
    extern char * locv();

    for (i = 0; i < MAX_OPTIONS; i++)
	if (optnum == options[i].value)
	    return(options[i].name);
    return(NULL);
}

/* -------------------------- O P T N U M --------------------------- */
/*
 * Return number of option given name. Accepts a numeric "name".
 * If name not recognized, returns -1 (ambiguous) or -2 (unknown).
 */
optnum(optname)
    char * optname;
{
    int i;
    int optnum;
    int prevmatch;
    int candidate;
    extern char * atoiv();

    if (*atoiv(optname, &optnum) == '\0')
	return(optnum);

    prevmatch = 0;
    candidate = -1;
    for (i = 0; i < MAX_OPTIONS; i++)
    {
	int nmatch;

	if (nmatch = compar(optname, options[i].name))
	    if (nmatch == -1)
	    {
		candidate = options[i].value;
		break;
	    }
	    else if (nmatch > prevmatch)
	    {
		candidate = options[i].value;
		prevmatch = nmatch;
	    }
	    else if (nmatch == prevmatch)
		return(-1);

    }
    if (candidate == -1)
	return(-2);
    return(candidate);
}

/* -------------------------- O P T I O N --------------------------- */
/*
 * Accept or refuse an option. A nonzero arg means accept, zero means
 * refuse.
 */
option(arg, optno)
    int arg;
    int optno;
{
    int i;

    for (i = 0; i < MAX_OPTIONS; i++)
	if (options[i].value == optno)
	{
	    if (arg)
		return(setopt(optno, 0,
			    options[i].testf,
			    options[i].actf,
			    options[i].subf));
	    else
		return(setopt(optno, 0, NULL, NULL, NULL));
	}
    return(-1);
}

/* -------------------------- E C H O T S T ------------------------- */
/*
 * The TTY can respond to WILL ECHO.
 */
echotst(connp, org, opcode, optno)
    NETCONN *connp;
    int org;
    int opcode;
    int optno;
{
    if (opcode == TN_WILL)
	return(CAN);
    else
	return(CANT);
}

/* -------------------------- E C H O A C T ------------------------- */
/*
 * Handle echo option. Receiving a WILL ECHO causes the tty to turn off
 * its own echoing, go into raw mode, and turn off tab expansion
 * and cr->lf conversion. The former state of these flags is remembered
 * for a future WONT ECHO.
 */
echoact(connp, opcode, optno)
    NETCONN *connp;
    int opcode;
    int optno;
{
    register struct tchars *tp;
    TTYMODE *modep = NULL;
    static int oldflags;

    switch(opcode)
    {
    case TN_WILL:

/* Turn off tab expansion so Ann Arbor cursor addressing works (!*&) */

	oldflags = SetFlags(CurMode(), CBREAK|ECHO|CRMOD|XTABS|RAW, CBREAK);
	modep = AllocMode();
	tp = &(modep->tm_tchars);
	tp->t_intrc = tp->t_quitc = 0377;
	ChgMode(modep);
	fprintf(stderr, "Remote echo\r\n");
	break;

    case TN_WONT:
	if (oldflags != 0)
	    SetFlags(CurMode(), ECHO|CRMOD|XTABS|CBREAK|RAW, oldflags);
	fprintf(stderr, "Local echo\r\n");
	break;
    }
}

/* -------------------------- B I N T S T --------------------------- */
/*
 * We can't do binary.
 */
bintst()
{
    return(CANT);
}

/* -------------------------- B I N A C T --------------------------- */
/*
 * Perform binary option.
 * There is a hidden assumption that if the user negotiates into binary
 * mode, he will negotiate out of it before negotiating into or out of
 * any other mode. Otherwise TTY settings could get upset. Solving
 * this problem for the general case is a real pain.
 */
binact(connp, opcode, optno)
    NETCONN *connp;
    int opcode;
    int optno;
{
}

#ifdef VTELNET
/* -------------------------- V T S U B ----------------------------- */
/*
 * Call the correct routine depending on the subnegotiation received.
 */
vtsub(connp, option, subp, sublen)
    NETCONN * connp;
    int option;
    register struct vterm_sb * subp;
    int sublen;
{
    switch (subp->sb_num)
    {
    case SB_VTSWITCH:
	set_vtin(connp, subp->sb_data.sb_vtno);
	break;
    case SB_MODES:
	vtstty(connp, subp->sb_data.sb_flags);
	break;
    }
}
#endif VTELNET

/* -------------------------- E C H O ------------------------------- */
/*
 * If terminal not echoing now, but it was before, echo the character.
 * Also simulate CRMOD if it is no longer turned on.
 */
echo(c)
    char c;
{
    int origflags = GetFlags(OrigMode());
    int curflags = GetFlags(CurMode());

    if ((c == '\r' || c == '\n') && (origflags & CRMOD) && ! (curflags & CRMOD))
    {
	if (curflags & ECHO)	/* CR or LF already echoed, echo other */
	    if (c == '\r')
		fprintf(stderr, "\n");
	    else
		fprintf(stderr, "\r");
	else if (origflags & ECHO)  /* Neither echoed, echo both */
	    fprintf(stderr, "\r\n");
    }
    else if ((origflags & ECHO) && !(curflags & ECHO))
	write(2, &c, 1);
}
/* ------------------------------------------------------------------ */
