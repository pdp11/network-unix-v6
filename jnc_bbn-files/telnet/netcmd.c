#
/*
 * Finite-state-machine for handling THP/TELNET controls.
 * This file contains all of the real intelligence needed for
 * handling either protocol.
 * Use: first allocate a state block using AllocSB().
 * Then do
 *      IoxEnter(&NetCmd, connp)
 * where connp is a ptr to a NetConn block containing
 * the state-block ptr returned from AllocSB()
 * NetCmd will read from FmNetBuf and call UserChar or do option neg.
 * as required.
 */

#include "globdefs.h"
#include "protocol.h"
#include "cbuf.h"
#include "netconn.h"
#ifndef NEWTTY
#include "/sys/sys/h/sgttybuf.h"
#endif
#ifdef NEWTTY
#include "/sys/sys/h/modtty.h"
#endif

#define entries(f) (sizeof(f)/sizeof(f[0]))

/* States of FSM; EXP stands for "expected" */
#define ESCAPE_EXP 0
#define HIBYTE_EXP 1
#define LOBYTE_EXP 2
#define CTRL_EXP   3

/* The states above always preceed the states below */
/* This ordering is used by NetChar() */

#define OPTION_EXP 4
#define MODE_EXP   5
#define WITHIN_SUBNEG 6
#define IAC_WITHIN_SUBNEG 7
#define COUNT_EXP  8
#define CHAR_EXP   9
#define WITHIN_DATA 10

#define INIT ESCAPE_EXP

/* Internal representation of WILL, WONT, DO, and DONT */

#define MY_WILL 0
#define MY_DO	1
#define MY_WONT 2
#define MY_DONT 3

extern Never(), Always(), NoAct(), EchoTst(), EchoAct();
extern BinTst(), BinAct();
extern FormTst(), LineAct(), PageAct();
extern SizeTst(), SizeAct();
extern DispTst(), DispAct();
extern ReconTst(), ReconAct();
struct OPT  /* Option negotiation table */
{
    char *optname;      /* Name of the option */
    int  optlength;     /* Length, excluding WILL/WONT/DO/DONT byte */
    int  optsr;         /* Set if it has SEND/RECEIVE byte */
    int  (*testf)();    /* Test function */
    int  (*actf)();      /* Action routine */
} opt[]
{

/* ---- Name ------------- len sr -- Test ------ Act ---------------- */

#ifdef TELNET
    {"binary",		    1,	0,  BinTst,	BinAct},
    {"echo",		    1,	0,  EchoTst,	EchoAct},
    {"reconnect",	    1,	0,  Never,	NoAct},
    {"suppress-go-ahead",   1,	0,  Always,	NoAct},
    {"set-record-size",     1,	0,  Never,	NoAct},
    {"status",		    1,	0,  Never,	NoAct},
    {"timing-mark",	    1,	0,  Always,	NoAct},
    {"RCTE",		    1,	0,  Never,	NoAct},
    {"linewidth",	    1,	1,  Never,	NoAct},
    {"pagesize",	    1,	1,  Never,	NoAct},
    {"cr-disp", 	    1,	1,  Never,	NoAct},
#endif
#ifdef THP
    {"echo",		    1,	0,  EchoTst,	EchoAct},
    {"binary",		    1,	0,  BinTst,	BinAct},
    {"RCTE",		    1,	0,  Never,	NoAct},
    {"include-go-ahead",    1,	0,  Never,	NoAct},
    {"extended-ascii",	    1,	0,  Never,	NoAct},
    {"reconnect",	    9,	0,  ReconTst,	ReconAct},
    {"set-message-size",    3,	0,  SizeTst,	SizeAct},
    {"linewidth",	    3,	1,  FormTst,	LineAct},
    {"pagesize",	    3,	1,  FormTst,	PageAct},
    {"set-vtabs",	   -1,	1,  Never,	NoAct},
    {"set-htabs",	   -1,	1,  Never,	NoAct},
    {"cr-disp", 	    3,	1,  DispTst,	DispAct},
    {"lf-disp", 	    3,	1,  DispTst,	DispAct},
    {"ff-disp", 	    3,	1,  DispTst,	DispAct},
    {"ht-disp", 	    3,	1,  DispTst,	DispAct},
    {"vt-disp", 	    3,	1,  DispTst,	DispAct},
#endif
    {"",		   -1,	0,  Never,	NoAct},
};

/* The generic "unimplemented" option is the last one. */

#define OPT_UNIMPL (entries(opt)-1)

struct SBlk    /* State block */
{
    char cmdstate;
    int ctrl;	/* (unsigned char) WILL|WONT|DO|DONT or SET_MODE|REQUEST_MODE */
    int count;	/* bytes left in record */
    int modep;
    char rest[16];
    char *restp;
#ifdef THP
    int XMode;	/* RECORD or STREAM */
#endif
    char state[entries(opt)][2];
    char orig[entries(opt)][2];
    char value[entries(opt)][2][2];
#ifdef NEWTTY
    int oldiflags[entries(opt)];
    int oldoflags[entries(opt)];
#endif
    int oldflags[entries(opt)];

};

/* -------------------------- A L L O C S B ------------------------- */
/*
 * AllocSB() allocates a state block for the connection and returns a ptr
 * to it. The state block contains all the information NetCmd needs to
 * process characters coming in on the connection.
 */
AllocSB()
{
    int i;
    int flags1;
    int flags2;
    struct SBlk *sbp;

    sbp = alloc(sizeof(*sbp));
    sbp->cmdstate = INIT;
    sbp->count = 0;
    sbp->restp = &(sbp->rest[0]);
#ifdef THP
    sbp->XMode = STREAM;
#endif
    for (i = 0; i < entries(opt); i++)
    {
	sbp->state[i][0] = 0;
	sbp->state[i][1] = 0;
	sbp->orig[i][0] = 0;
	sbp->orig[i][1] = 0;
    }
    sbp->modep = AllocMode();
    flags1 = GetFlags(OrigMode());
    for (i = 0; i < entries(opt); i++)
	sbp->oldflags[i] = flags1;
#ifdef NEWTTY
    flags1 = GetXMode(OrigMode())->t_iflags;
    flags2 = GetXMode(OrigMode())->t_oflags;
    for (i = 0; i < entries(opt); i++)
    {
	sbp->oldiflags[i] = flags1;
	sbp->oldoflags[i] = flags2;
    }
#endif
    return(sbp);
}

/* -------------------------- F R E E S B --------------------------- */
/*
 * FreeSB(sbp) is called to dispose of a state block.
 */
FreeSB(sbp)
struct SBlk *sbp;
{

    FreeMode(sbp->modep);
    free(sbp);
}

/* -------------------------- C H G S B ----------------------------- */
/*
 * ChgSB(sbp) is called to make the state block pointed to by sbp
 * the current state block. All it does is change to that set of tty
 * modes.
 */
ChgSB(sbp)
    struct SBlk *sbp;
{

    ChgMode(sbp->modep);
}

/* -------------------------- N E T C M D --------------------------- */
/*
 * As long as there is room in ToUsrBuf, get chars from FmNetBuf.
 * There must be enough room to handle the full expansion of any single
 * TELNET/THP control.
 */
NetCmd(aconnp)
    struct NetConn *aconnp;
{
    register struct NetConn *connp;
    register retval;

    retval = 1;
    connp = aconnp;
    while (!Cempty(connp->FmNetBuf) && Cfree(connp->ToUsrBuf) >= USERGROW)
    {
        retval = 0;
        NetChar(Cgetc(connp->FmNetBuf), connp);
    }
    return(retval);
}

/* -------------------------- N E T C H A R ------------------------- */
/*
 * Handle all protocol controls. For option negotiation, convert WILL WONT DO
 * DONT into more convenient form, save it in SBlk.ctrl, and call negotiate()
 * when the control has been assembled.
 * For other controls, wait until they are all gathered up and then act on them.
 * Handle REPEAT and DATA records.
 * Call UserChar as required.
 */
NetChar(c, connp)
{
    register char rstate;
    register struct SBlk *sbp;
    extern int verbose;

    sbp = connp->StateP;
    rstate = sbp->cmdstate;

    c =& 0377;

    switch (rstate)
    {
    case INIT:
        if (c != IAC)
        {
            if (connp->InSynCnt <= 0)
                UserChar(c, connp);
            if (connp->RemState == SUSP || connp->LocState == SUSP)
                fdprintf(2, "Text received in suspended state!\r\n");
        }
        else
#ifdef TELNET
            rstate = CTRL_EXP;
#endif
#ifdef THP
            rstate = HIBYTE_EXP;
#endif
        break;
#ifdef THP
    case HIBYTE_EXP:
        sbp->count = c * 256;
        rstate = LOBYTE_EXP;
        break;
    case LOBYTE_EXP:
        sbp->count =+ c;
        rstate = CTRL_EXP;
        break;
#endif

    case CTRL_EXP:
        switch(c)
        {
#ifdef THP
        case DATA:
            rstate = WITHIN_DATA;
            break;
        case REPEAT:
            rstate = COUNT_EXP;
            break;
        case SET_MODE:
        case REQUEST_MODE:
            sbp->ctrl = c;
            rstate = MODE_EXP;
            break;
        case SUSPEND:
            connp->RemState = SUSP;
            SendCtrl(connp, SUSPENDED, 0, 0);
            fdprintf(2, "Connection remotely suspended\r\n");
            rstate = INIT;
            break;
        case SUSPENDED:
            if (connp->LocState == SUSPING)
            {
                connp->LocState = SUSP;
                fdprintf(2, "Connection suspended\r\n");
            }
            rstate = INIT;
            break;
        case CONTINUE:
            if (connp->RemState == SUSP)
            {
                connp->RemState = ACTIVE;
                fdprintf(2, "Connection remotely resumed\r\n");
            }
            rstate = INIT;
            break;
#endif
#ifdef TELNET
        case SB:
            rstate = WITHIN_SUBNEG;
            break;
        case IAC:
            UserChar(IAC, connp);
            rstate = INIT;
            break;
#endif
        case DM:
            connp->InSynCnt--;
            rstate = INIT;
            break;
        case WILL:
            sbp->ctrl = MY_WILL;
            rstate = OPTION_EXP;
            break;
        case DO:
            sbp->ctrl = MY_DO;
            rstate = OPTION_EXP;
            break;
        case WONT:
            sbp->ctrl = MY_WONT;
            rstate = OPTION_EXP;
            break;
        case DONT:
            sbp->ctrl = MY_DONT;
            rstate = OPTION_EXP;
            break;
        case BREAK:
            UserChar('\0', connp);
            rstate = INIT;
            break;
        case AYT:
            SendCtrl(connp, NOP, 0, 0);
            SendBuf(connp, 5, "Yes\r\n");
            rstate = INIT;
            break;
        case IP:
#ifndef NEWTTY
            UserChar('\177', connp);
#endif
#ifdef NEWTTY
	    UserChar(GetXMode(sbp->modep)->t_intr, connp);
#endif
            SendSynch(connp);
            rstate = INIT;
            break;
        case EC:
            UserChar(GetMode(sbp->modep)->s_erase, connp);
            rstate = INIT;
            break;
        case EL:
            UserChar(GetMode(sbp->modep)->s_kill, connp);
            rstate = INIT;
            break;
        case NOP:
        case AO:
#ifdef TELNET
        case GA:
#endif
            rstate = INIT;
            break;
        default:
            com_err(0, "Unknown net control 0%o (%d.) ignored", c, c);
            rstate = INIT;
            break;
        }
        break;
    case OPTION_EXP:
        if (sbp->restp < &(sbp->rest[sizeof(sbp->rest)]))
            *(sbp->restp)++ = c;
        if (sbp->count == 0)
        {
            negotiate(connp, 0, sbp->ctrl, sbp->restp - sbp->rest, sbp->rest);
            sbp->restp = sbp->rest;
            rstate = INIT;
        }
        break;
#ifdef TELNET
    case WITHIN_SUBNEG:
        if (c == IAC)
            rstate = IAC_WITHIN_SUBNEG;
        break;
    case IAC_WITHIN_SUBNEG:
        if (c == SE)
            rstate = INIT;
        else
            rstate = WITHIN_SUBNEG;
        break;
#endif
#ifdef THP
    case MODE_EXP:
        if (sbp->ctrl == REQUEST_MODE)
        {
            sbp->XMode = c;
            SendCtrl(connp, SET_MODE, 1, &c);
            if (verbose)
                fdprintf(2, "%s mode\r\n", c == RECORD? "Record" : "Stream");
        }
        else if (sbp->ctrl == SET_MODE)
        {
            if (verbose)
                fdprintf(2, "Receiving in %s mode\r\n",
                c == RECORD? "record" : "stream");
        }
        else
            com_err(0, "NetCmd error: mode expected");
        rstate = INIT;
        break;
    case COUNT_EXP:
        sbp->ctrl = c;
        rstate = CHAR_EXP;
        break;
    case CHAR_EXP:    /* Here c is char to repeat, ctrl is count. */
        while (sbp->ctrl-- > 0)
            UserChar(c, connp);
        rstate = INIT;
        break;
    case WITHIN_DATA:
        UserChar(c, connp);
        break;
#endif
    default:
        com_err(0, "NetCmd error: unknown state %d", rstate);
        rstate = INIT;
        break;
    }

#ifdef THP
    if (rstate == INIT && sbp->count != 0)
        com_err(0, "Long record: %d left after 0%o received", sbp->count, c);
    if (rstate > CTRL_EXP)  /* Record header complete */
    {
        if (sbp->count == 0)
            if (rstate == WITHIN_DATA)
                rstate = INIT;
            else
                com_err(0, "Short record count: state = %d, ctrl = 0%o", rstate, c);
        else
            sbp->count--;
    }
#endif

    sbp->cmdstate = rstate;
    return;
}

/* -------------------------- N E G O T I A T E --------------------- */
/*
 * Handle actual option negotiation. First arg is pointer to connection block.
 * Second is 1 if originating request, 0 if received over net.
 * Third is WILL, WONT, DO, or DONT.
 * Fourth is count of additional data (normally only 1 byte).
 * Fifth is pointer to that data.
 *
 * For origination of a request, we reverse DO and WILL to make it look
 * like it came in over the net. Then we see whether it can be done,
 * by calling the test function. If so, it gets "replied to", which originates
 * in the right way (reverses DO and WILL again). Otherwise the user is informed
 * of his misfortune.
 *
 * For requests originated at the foreign host, we check to see whether the
 * option is already in effect; if so, no reply is made and no action is taken.
 * This is a little tricky, because even if a previous negotiation was
 * successful, this one may be for different values. E.g.,
 * DO LINE-WIDTH SENDER 60 is not already in effect if the present line width
 * is 80, or if the receiver is managing line width.
 * SENDER/RECEIVER and the value byte (line width, page length, etc.) are both
 * regarded as values to be compared against what's there, assuming the option
 * is in effect at all (previous successful negotiation).
 * The same code can handle a request for a different record size, which is
 * a two-byte value without a SENDER/RECEIVER field.
 * If it is not in effect, we call the test function to see whether it can be
 * done, and reply appropriately. We don't bother
 * the test function if the foreign host is saying DONT or WONT, since
 * such requests can never be refused (just ignored, if they are already
 * in effect). If everything checks, an appropriate reply is sent off, and if
 * the result of the test was that it can be done, the action routine is called.
 *
 * Back on the originating host, when the reply is received, the action routine
 * is called. It's too late to test anything; if it can be done, it will be.
 * Note that conflicts (different line widths, etc.) are resolved in the action
 * routine.
 *
 * The test function is permitted to modify the values of the option.
 * If, for example, we are asked to do PAGE-SIZE 30, but all we can really do is
 * PAGE-SIZE 24, then testf can actually modify the values pointed to by "args".
 * If org_here is set, however, then it should not modify the values, but
 * send a message to the user (if it wants) and return CANT. After all, the user
 * can resend the do-able thing if that's really what he wants.
 */
char *which[] { "will", "do", "won't", "don't"};

/* Values returned by testf */
#define CANT 0
#define CAN  1

/* Useful defines */
#define NO  0
#define YES 1

negotiate(connp, org_here, op, nargs, args)
    struct NetConn *connp;
    int org_here;
    int op;
    int nargs;
    char args[];
{
    int opnum;
    register struct SBlk *sbp;
#define DO_SIDE 0
#define WILL_SIDE 1
    int side;   /* DO_SIDE or WILL_SIDE */
    int nstate; /* NO (DONT or WONT) or YES (DO or WILL) */
    int rop;
    register char *values;
    int nvalues;
    int verdict;
    extern int verbose;

    sbp = connp->StateP;
    opnum = args[0] & 0377;
    values = &args[1];
    nvalues = nargs - 1;
    if (opnum >= OPT_UNIMPL) opnum = OPT_UNIMPL;

    if (opt[opnum].optlength != -1 && opt[opnum].optlength != nargs)
    {
        com_err(0, "Option %s is %d bytes long, should be %d",
                    opt[opnum].optname, nargs, opt[opnum].optlength);
        return;
    }

    rop = op;
    if (org_here)   /* Reverse DO and WILL */
        if      (op == MY_DO)   op = MY_WILL;
        else if (op == MY_WILL) op = MY_DO;
        else if (op == MY_DONT) op = MY_WONT;
        else                    op = MY_DONT;

    side = (op == MY_DO || op == MY_DONT)? DO_SIDE : WILL_SIDE;
    nstate = (op == MY_DO || op == MY_WILL)? YES : NO;

    if (org_here)                           /* Origination */
    {
        if (nstate == NO || (*opt[opnum].testf)(connp, org_here, op, nargs, args) == CAN)
        {
            if (verbose) snap("Sending", rop, nargs, args);
            SendReply(connp, YES, op, nargs, args);
            sbp->orig[opnum][side] = YES;
            vcopy(nvalues, values, sbp->value[opnum][side]);/* Remember for conflicts */
        }
        else
            snap("Unimplemented:", rop, nargs, args);
    }

    else if (sbp->orig[opnum][side] == NO)   /* Reply to foreign origination */
    {
/*
 * The option is already in effect if the new state is the same as the current state
 * AND either the new state is DONT/WONT or it is DO/WILL and the new values are the
 * same as the current ones. If the option is "timing mark", reply regardless...
 */
        if (sbp->state[opnum][side] == nstate)
            if (nstate == NO || veq(nvalues, values, sbp->value[opnum][side]))
#ifdef TELNET
                if (opnum != OPT_TMARK)
#endif
                {
                    snap("Already did", op, nargs, args);
                    return;
                }
        vcopy(nvalues, values, sbp->value[opnum][side]);/* Remember for conflicts */

/* It's not already in effect; find out whether it could be. */

        if (nstate == NO)
            verdict = CAN;     /* Can always DONT/WONT */
        else
            verdict = (*opt[opnum].testf)(connp, org_here, op, nargs, args);
        SendReply(connp, verdict, op, nargs, args);
        if (verdict != CANT)
        {
            if (verbose) snap("Affirming", op, nargs, args);
            (*opt[opnum].actf)(connp, sbp->modep, op, nargs, args);
            sbp->state[opnum][side] = nstate;
            vcopy(nvalues, values, sbp->value[opnum][side]); /* Remember */
        }
    }

    else                                     /* Reply to our origination */
    {   if (connp->UsingPty == 0)
            snap("Reply was", op, nargs, args);     /* Always notify user */
        (*opt[opnum].actf)(connp, sbp->modep, op, nargs, args);
        sbp->orig[opnum][side] = NO;
        sbp->state[opnum][side] = nstate;
        vcopy(nvalues, values, sbp->value[opnum][side]); /* Remember */
    }
}

/* -------------------------- V C O P Y ----------------------------- */
/*
 * vcopy(nargs, args, values) copies the arguments into the value array.
 */
vcopy(nargs, args, values)
    int nargs;
    char args[];
    char values[];
{
    register char *ap;
    register char *vp;
    register char *lastv;
    int len;
    struct SBlk *p;

    len = sizeof(p->value[0][0]);
    if (nargs < len) len = nargs;
    ap = args;
    vp = values;
    lastv = vp + len;

    while (vp < lastv)
        *vp++ = *ap++;
}

/* -------------------------- V C M P ------------------------------- */
/*
 * veq(nargs, args, values) compares the incoming arguments
 * against the current values. It returns 1 if they are equal, 0 if not.
 */
veq(nargs, args, values)
    int nargs;
    char args[];
    char values[];
{
    register char *ap;
    register char *vp;
    int len;
    register char *lastv;
    struct SBlk *p;

    len = sizeof(p->value[0][0]);
    if (nargs < len) len = nargs;
    ap = args;
    vp = values;
    lastv = vp + len;

    while (vp < lastv)
        if (*vp++ != *ap++)
            return(0);
    return(1);
}

/* -------------------------- S E N D R E P L Y --------------------- */
/*
 * SendReply(connp, reply, op, nargs, argp) replies to a negotiated
 * option. If reply is nonzero, it replies affirmatively; if reply is
 * zero, it replies negatively. op is MY_WILL/WONT/DO/DONT; the argument
 * buffer contains at least one char, which is the option number, although
 * it may contain more.
 */
SendReply(connp, reply, op, nargs, argp)
    struct NetConn *connp;
    int reply;
    int op;     /* Unsigned char */
    int nargs;
    char *argp;
{
    extern int verbose;

    switch (op)
    {
    case MY_DO:
        if (reply == 0)
        {
            SendCtrl(connp, WONT, nargs, argp);
	    if (verbose) snap("Refusing", op, nargs, argp);
        }
        else
            SendCtrl(connp, WILL, nargs, argp);
        break;
    case MY_WILL:
        if (reply == 0)
        {
            SendCtrl(connp, DONT, nargs, argp);
	    if (verbose) snap("Refusing", op, nargs, argp);
        }
        else
            SendCtrl(connp, DO, nargs, argp);
        break;
    case MY_DONT:
        SendCtrl(connp, WONT, nargs, argp);
        break;
    case MY_WONT:
        SendCtrl(connp, DONT, nargs, argp);
        break;
    }
}

/* -------------------------- N E V E R ----------------------------- */

Never()
{
    return(CANT);
}

/* -------------------------- A L W A Y S --------------------------- */

Always()
{
    return(CAN);
}

/* -------------------------- N O A C T ----------------------------- */

NoAct()
{
    return;
}

/* -------------------------- E C H O T S T ------------------------- */
/*
 * The PTY can respond to DO ECHO; the TTY can respond to WILL ECHO.
 */
EchoTst(connp, org, op, nargs, args)
    struct NetConn *connp;
    int org;
    int op;
    int nargs;
    char args[];
{
    if (connp->UsingPty)
        if (op == MY_DO)
            return(CAN);
        else
            return(CANT);
    else
        if (op == MY_WILL)
            return(CAN);
        else
            return(CANT);
}

/* -------------------------- E C H O A C T ------------------------- */
/*
 * Handle echo option. Receiving a WILL ECHO causes the tty to turn off
 * its own echoing, go into raw mode, and turn off tab expansion
 * and cr->lf conversion. The former state of these flags is remembered
 * for a future WONT ECHO. Receiving a DO ECHO enables echoing on the pty;
 * receiving a DONT ECHO disables it. This is done through setting the speeds;
 * a speed of 134.5 baud prevents the pty from echoing regardless of the state
 * of the ECHO flag.
 */
EchoAct(connp, modep, op, nargs, args)
    struct NetConn *connp;
    char *modep;
    int op;
    int nargs;
    char *args;
{
    register struct SBlk *sbp;
    struct sgttybuf *ttyp;

    sbp = connp->StateP;
    switch(op)
    {
    case MY_WILL:

/* Turn off tab expansion so Ann Arbor cursor addressing works (!*&) */

	sbp->oldflags[args[0]] = SetFlags(modep, RAW|ECHO|CRMOD|XTABS, RAW);
	fdprintf(2, "Remote echo\r\n");
	break;

    case MY_WONT:
	SetFlags(modep, ECHO|CRMOD|XTABS|RAW, sbp->oldflags[args[0]]);
	fdprintf(2, "Local echo\r\n");
	break;


/* A DO or DONT echo applies (literally) to the pty. */

    case MY_DONT:
	SetFlags(modep, ECHO, 0);
	ttyp = GetMode(modep);
	ttyp->s_ospeed = ttyp->s_ispeed = B134;
	SetMode(modep, ttyp);
	break;

    case MY_DO:
	ttyp = GetMode(modep);
	ttyp->s_ospeed = ttyp->s_ispeed = B2400;
	SetMode(modep, ttyp);
	SetFlags(modep, ECHO, ECHO);
	break;
    }
}

#ifdef THP
/* -------------------------- F O R M T S T ------------------------- */
/*
 * FormTst() checks out the formatting options (line width and page size).
 * The pty can format outgoing data, and the user tty can format incoming
 * data. Both sides are willing to let the other side handle the formatting.
 * What this means is that of the eight possibilities
 * (Pty/Tty  X	DO/WILL  X  SENDER/RECEIVER)
 * the only forbidden ones are User-WILL-Sender (which would require the
 * the user side to format outgoing data) and Server-DO-Receiver
 * (which would require the server side to format incoming data).
 */
FormTst(connp, org, op, nargs, args)
    struct NetConn *connp;
    int   org;
    int op;
    int nargs;
    char *args;
{

    if (connp->UsingPty)
	if (op == MY_DO && args[1] == RECEIVER)
	    return(CANT);
	else
	    return(CAN);
    else
	if (op == MY_WILL && args[1] == SENDER)
	    return(CANT);
	else
	    return(CAN);
}

/* -------------------------- L I N E A C T ------------------------- */
/*
 * Set line width. Depending on whether this is the pty or tty, and on
 * whether DO or WILL and whether SENDER or RECEIVER, it may or may not
 * be this side's job to set the line width. The pty can only be the
 * sender of line-width adjusted data; the tty can only be the receiver.
 * If the other side is doing it, it must be turned off here.
 * Consult the THP definition for more information.
 * Note: the line width conflict resolution algorithm of the THP report is not
 * implemented. It just uses what the incoming record specifies.
 */
LineAct(connp, modep, op, nargs, args)
    struct NetConn *connp;
    char *modep;
    int op;
    int nargs;
    char args[];
{
    register char *mp;
    register int width;
    register int me;	/* Set if I am to act */
    struct SBlk *sbp;
    extern int verbose;

    if (connp->UsingPty)
	if (op == MY_WILL && args[1] == SENDER)
	    me = 1;
	else
	    me = 0;
    else
	if (op == MY_DO && args[1] == RECEIVER)
	    me = 1;
	else
	    me = 0;

    mp = GetXMode(modep);
    sbp = connp->StateP;

    if (me)
    {
	sbp->oldoflags[args[0]] =
		SetXFlags(modep, 'o', TO_AUTONL|TO_CRMOD, TO_AUTONL|TO_CRMOD);
	width = args[2] & 0377;
	if (width == 0)
	    width = 80; /* Default size */
	else if (width == 255)
	    width = 0;	/* Infinite size */
	mp->t_width = width;
    }
    else if (op == MY_WONT || op == MY_DONT)
	SetXFlags(modep, 'o', TO_AUTONL|TO_CRMOD, sbp->oldoflags[args[0]]);
    else
	sbp->oldoflags[args[0]] =
		SetXFlags(modep, 'o', TO_AUTONL, 0);

    SetXMode(modep, mp);
    return;
}

/* -------------------------- P A G E A C T ------------------------- */
/*
 * Set the page size. Exactly the same considerations apply as with
 * setting the line width. Furthermore, if the other side is doing
 * the page-size processing, then XONXOFF processing must be off on this side;
 * if this side is doing it, then XONXOFF processing must be on.
 * When the option is turned off again, the bit must be restored to its
 * previous value.
 *
 * Note: the conflict resolution algorithm of the THP report is not
 * implemented. It just uses what the incoming record specifies.
 */
PageAct(connp, modep, op, nargs, args)
    struct NetConn *connp;
    char *modep;
    int op;
    int nargs;
    char args[];
{
    register char *mp;
    register int length;
    register int me;	/* Set if I am to act */
    struct SBlk * sbp;
    extern int verbose;

    mp = GetXMode(modep);

    if (connp->UsingPty)
	if (op == MY_WILL && args[1] == SENDER)
	    me = 1;
	else
	    me = 0;
    else
	if (op == MY_DO && args[1] == RECEIVER)
	    me = 1;
	else
	    me = 0;

    sbp = connp->StateP;
    if (me)
    {
	sbp->oldoflags[args[0]] = SetXFlags(modep, 'o', TO_XONXOFF, TO_XONXOFF);
	length = args[2] & 0377;
	if (length == 0)
	    length = 24; /* Default size */
	else if (length == 255)
	    length = 0;  /* Infinite size */
	mp->t_pagelen = length;
    }
    else if (op == MY_WONT || op == MY_DONT)
	SetXFlags(modep, 'o', TO_XONXOFF, sbp->oldoflags[args[0]]);
    else
    {
	sbp->oldoflags[args[0]] = SetXFlags(modep, 'o', TO_XONXOFF, 0);
	mp->t_pagelen = 0;
    }
    SetXMode(modep, mp);
    return;
}

/* -------------------------- R E C O N T S T ----------------------- */
/*
 * ReconTst() checks out the suggested THP Reconnection negotiation.
 * It is only willing to accept DO host/port START; DO host/port EXPECT
 * is not implemented.
 */
ReconTst(connp, org, op, nargs, argp)
    struct NetConn *connp;
    int org;
    int op;         /* MY_WILL/WONTDO/DONT */
    int nargs;     /* Length of rest of record */
    char *argp;     /* Rest of record */
{
    struct Reconn reconn;
    register char *rp;
    register char *ap;
    int i;

    rp = &reconn;
    ap = argp;

/* Throw away RECONNECT byte */

    nargs--;
    ap++;

/* Copy rest of record */

    for (i = 0; i < sizeof(reconn) && i < nargs; i++)
        rp[i] = ap[i];

/* Check it out */

    if (reconn.RType != START)
        return(CANT);  /* Don't know how to handle it */

    if (org == 1)
        if (op == MY_WILL)
            return(CAN);
        else
            return(CANT);
    else
        if (op == MY_DO)
            return(CAN);
        else
            return(CANT);
}

/* -------------------------- R E C O N A C T ----------------------- */
/*
 * ReconAct() implements the THP reconnection option.
 * Actually, it only implements the response to DO host/port START;
 * the response to DO host/port EXPECT depends on the purpose of
 * the reconnection (e.g., if the reconnection is from an authenticator,
 * the DO EXPECT should set up a listening server connection which
 * when established will require no login).
 *
 * Note that implementation of DO host/port START is sufficient for
 * a User THP which wants to be able to move the other end of its
 * connection from some authenticator to the host the user wanted.
 */
ReconAct(connp, modep, op, nargs, argp)
    struct NetConn *connp;
    char *modep;    /* Ignored */
    int op;	    /* MY_WILL/WONTDO/DONT */
    int nargs;	   /* Length of rest of record */
    char *argp;     /* Rest of record */
{
    int i;
    struct Reconn reconn;
    char *rp;
    char *open_args[5];

/* Fill in structure */

    rp = &reconn;
    for (i = 1; i < sizeof(reconn) && i < nargs; i++)
	*rp++ = argp[i];

    if (op == MY_WILL || op == MY_WONT || op == MY_DONT)
	return;

/* Set EOF. The reply will be the last data sent over this connection. */

    connp->ToNetEOF = 1;

/* Build up an open command. */

    i = 0;
    sbegin();
    open_args[i++] = "open";
    open_args[i++] = scatn(hostname(reconn.RHost), -1);     /* Copy */
    open_args[i++] = "-fp";
    open_args[i++] = scatn(locv(0, reconn.RPort), -1);	    /* Copy */
    open_args[i] = 0;
    fdprintf(2, "Opening connection to %s, port %s\r\n",
		open_args[1], open_args[3]);
    netopen(0, i, open_args);
    send();
}

/* -------------------------- S I Z E T S T ------------------------- */
/*
 * SizeTst() checks out the Approximate Record Size option. We are willing
 * to receive any record size; we can only xmit up to about CBUFSIZE ourselves.
 */
SizeTst(connp, org, op, nargs, args)
    struct NetConn *connp;
    int org;
    int op;

    int nargs;
    char *args;
{
    int size;

    size = (args[1] & 0377) * 256 + (args[2] & 0377);
    if (op == MY_WILL)
	return(CAN);
    else if (size < CBUFSIZE && size > 4)
	return(CAN);
    else
	return(CANT);
}

/* -------------------------- S I Z E A C T ------------------------- */
/*
 * SizeAct() does the work associated with the Approximate Record Size
 * option. If we are being asked to accept some record size (WILL), there is
 * nothing for us to do; if we are being asked to perform it (DO),
 * the connection's RecSize field is set appropriately.
 */
SizeAct(connp, modep, op, nargs, args)
    struct NetConn *connp;
    char *modep;
    int op;
    int nargs;
    char *args;
{
    int size;

    size = (args[1] & 0377) * 256 + (args[2] & 0377);
    if (op == MY_DO)
	connp->RecSize = size;
    else if (op == MY_DONT)
	connp->RecSize = 0; /* Unspecified size */
}

/* -------------------------- D I S P T S T ------------------------- */
/*
 * DispTst() approves the disposition option. The only form we do is
 * WILL SENDER (that is, telling us to perform disposition on
 * from-user/to-net data); DO RECEIVER (telling us to perform disposition
 * on from-net/to-user data) is not implemented. However, we are willing
 * to accept the other side's doing disposition: WILL RECEIVER and DO SENDER
 * are acceptable.
 *
 * The exception to the above is HT simulation, which gets done in the pty or tty.
 * The pty can only do it on from-pty/to-net data; the tty can only do it on
 * from-net/to-user data.
 */
DispTst(connp, org, op, nargs, args)
    struct NetConn *connp;
    int   org;
    int op;
    int nargs;
    char *args;
{
    int value;  /* Unsigned char */

    value = args[2] & 0377;

    if ((op==MY_WILL && args[1]==RECEIVER) || (op==MY_DO && args[1]==SENDER))
        return(CAN);
    else if (op == MY_DO && args[1] == RECEIVER)
    {
	if (args[0] == OPT_HT_DISP && value == DISP_SIMULATE)
            return(connp->UsingPty == 0? CAN : CANT);   /* The tty can do it */
        else
            return(CANT);
    }
    else if (op == MY_WILL && args[1] == SENDER)
/*
 * We do NOT turn line feeds into NL and spaces,
 * or FF or VT into the right number of blank lines.
 */
	if (value == DISP_SIMULATE)
            if (args[0] == OPT_LF_DISP || args[0] == OPT_FF_DISP || args[0] == OPT_VT_DISP)
                return(CANT);
            else if (args[0] == OPT_HT_DISP)         /* Simulate tabs? */
                return(connp->UsingPty? CAN : CANT); /* The tty can do it */
        else
            return(CAN);
}

/* -------------------------- D I S P A C T ------------------------- */
/*
 * Implement disposition options. Most of the work is actually done
 * by SendBuf below; all this routine implements is HT simulation,
 * which can be done purely by manipulating the pty or tty.
 */
DispAct(connp, modep, op, nargs, args)
    struct NetConn *connp;
    char *modep;
    int op;
    int nargs;
    char args[];
{
    int value;	/* Unsigned char */
    int out;
    int me;

    value = args[2] & 0377;

/* out is true if the action affects the output path of the pty or tty. */

    if (connp->UsingPty)
	if (op == MY_WILL || op == MY_WONT)
	    out = 1;
	else
	    out = 0;
    else
	if (op == MY_DO || op == MY_DONT)
	    out = 1;
	else
	    out = 0;

/* me is true if the local side is supposed to do the tab handling. */

    if (
	 (op == MY_WILL && args[1] == SENDER)
	 ||
	 (op == MY_DO && args[1] == RECEIVER)
       )
	me = 1;
    else
	me = 0;

    if (args[0] == OPT_HT_DISP)
    {
	if (!out)
	{
	    if (me && value == DISP_SIMULATE)
		com_err(0, "Can't expand tabs on input.");
	    return;
	}

	if (me && value == DISP_SIMULATE)
	    SetFlags(modep, XTABS, XTABS);
	else
	    SetFlags(modep, XTABS, 0);
    }

}
#endif

/* -------------------------- B I N T S T --------------------------- */
/*
 * We can do binary, in either direction, if we have the new tty driver
 * at our disposal.
 */
BinTst()
{
#ifdef NEWTTY
    return(CAN);
#endif
#ifndef NEWTTY
    return(CANT);
#endif
}

/* -------------------------- B I N A C T --------------------------- */
/*
 * Perform binary option. The pty is the mirror image of the tty.
 * There is a hidden assumption that if the user negotiates into binary
 * mode, he will negotiate out of it before negotiating into or out of
 * any other mode. Otherwise TTY settings could get upset. Solving
 * this problem for the general case is a real pain.
 */
BinAct(connp, modep, op, nargs, args)
    struct NetConn *connp;
    char *modep;
    int op;
    int nargs;
    char args[];
{
#ifdef NEWTTY
    struct SBlk *sbp;

    sbp = connp->StateP;

#define USER 1000
#define SERVER 2000

#define B_IN_IMASK (TI_CLR_MSB|TI_CRMOD|TI_NOSPCL|TI_ONECASE)
#define B_IN_ISET  TI_NOSPCL
#define B_IN_OMASK TO_XONXOFF
#define B_IN_OSET  0
/* Have to clear TO_XONXOFF to let ctrl-Q, ctrl-S in */

#define B_OUT_OMASK (TO_CLR_MSB|TO_CRMOD|TO_ONECASE|TO_XTABS|TO_AUTONL|TO_LITERAL)
#define B_OUT_OSET  TO_LITERAL

/* The handling of CRs specified by the protocol gets turned off in binary mode */

    switch(op)
    {
    case MY_DO:
	connp->ToNetBin = 1;
	break;
    case MY_DONT:
	connp->ToNetBin = 0;
	break;
    case MY_WILL:
	connp->ToUsrBin = 1;
	break;
    case MY_WONT:
	connp->ToUsrBin = 0;
	break;
    }

    op =+ connp->UsingPty? SERVER : USER;
    switch(op)
    {
    case USER	+ MY_DO :  /* We (user) sending binary */
    case SERVER + MY_WILL: /* We (server) rcving binary */
	sbp->oldiflags[args[0]] = SetXFlags(modep, 'i', B_IN_IMASK, B_IN_ISET);
	sbp->oldoflags[args[0]] = SetXFlags(modep, 'o', B_IN_OMASK, B_IN_OSET);
	SetXFlags(modep, 'p', TP_PENABLE, 0);
	break;

    case USER	+ MY_DONT:
    case SERVER + MY_WONT:
	SetXFlags(modep, 'i', B_IN_IMASK, sbp->oldiflags[args[0]]);
	SetXFlags(modep, 'o', B_IN_OMASK, sbp->oldoflags[args[0]]);
	/* Restoring TP_PENABLE is not worth the trouble */
	break;

    case USER	+ MY_WILL: /* We (user) rcving binary */
    case SERVER + MY_DO:   /* We (server) sending binary */
	sbp->oldoflags[args[0]] = SetXFlags(modep, 'o', B_OUT_OMASK, B_OUT_OSET);
	break;

    case USER	+ MY_WONT:
    case SERVER + MY_DONT:
	SetXFlags(modep, 'o', B_OUT_OMASK, sbp->oldoflags[args[0]]);
	break;
    }
#endif
}

/* -------------------------- S N A P ------------------------------- */
/*
 * dump out a description of an option. Assumes that opnum has been made
 * to fit inside the option table, and assumes that length checking on
 * nargs has already been done. Knows about some options; for those it
 * knows nothing about, it prints one byte of argument as a single number,
 * two bytes as a single word, and more than that as individual bytes.
 */
snap(msg, op, nargs, args)
    char *msg;
    int op;
    int nargs;
    char *args;
{
    register char *ap;
    int opnum;
    struct Reconn reconn;
    register char *rp;

    ap = args;
    opnum = *ap++;
    nargs--;
    fdprintf(2, "%s %s ", msg, which[op]);
    if (opnum >= 0 && opnum < OPT_UNIMPL)
	fdprintf(2, "%s ", opt[opnum].optname);
    else
	fdprintf(2, "Unimplemented option ");
    if (opt[opnum].optsr)
    {
	if (nargs > 0)
	    if (*ap == SENDER)
		fdprintf(2, "sender ");
	    else if (*ap == RECEIVER)
		fdprintf(2, "receiver ");
	    else
		fdprintf(2, "%d? (should be sender or receiver) ", *ap & 0377);
	nargs--;
	ap++;
    }
    if (nargs == 1)
	fdprintf(2, "%d", *ap & 0377);
    else if (nargs == 2)
	fdprintf(2, "%d", (*ap & 0377) * 256 + (*(ap+1) & 0377));
    else if (opnum == OPT_RECONNECT)
    {
	rp = &reconn;
	while (nargs-- > 0)
	    *rp++ = *ap++;
	fdprintf(2, "host %s port 0%o ", hostname(reconn.RHost), reconn.RPort);
	if (reconn.RType == START)
	    fdprintf(2, "start");
	else if (reconn.RType == EXPECT)
	    fdprintf(2, "expect");
	else
	    fdprintf(2, "%d? (should be start or expect)", reconn.RType);
    }
    else
	while(nargs-- > 0)
	    fdprintf(2, "%d ", *ap++);
    fdprintf(2, "\r\n");
}

/* -------------------------- O P T I O N --------------------------- */
/*
 * option(arg, argc, argv) is the option-negotiation command. It starts
 * the negotiation process for the requested option.
 * For THP, the connection mode is also handled here, as though it were
 * a genuine negotiated option. (God only knows why it isn't...)
 * "do" and "dont" are mapped into REQUEST_MODE, and "will" and "wont"
 * are mapped into SET_MODE.
 */
option(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int i, type;
    int nmatch, prevmatch;
    int candidate;
    int value;
#ifdef THP
    char *cmode[2];
    struct Reconn reconn;
    char r[sizeof(reconn)+1];
    char *rp;
#endif
    extern char *compar();
    extern long gethost();
    extern char *atoiv();
    extern struct NetConn *NetConP;

#ifdef THP
    cmode[RECORD] = "record";
    cmode[STREAM] = "stream";
#endif

    if (argc == 0)
    {
        fdprintf(2, "Try to negotiate the specified option.\r\n");
        return;
    }
    if (argc < 2)
    {
        fdprintf(2, "Usage: %s option\r\n", argv[0]);
        fdprintf(2, "Available options are:\r\n\t  ");
#ifdef TELNET
        for (i = 0; i < entries(opt); i++)
        {
            fdprintf(2, "%-20s", opt[i].optname);
#endif
#ifdef THP
        for (i = 0; i < entries(opt) + 2; i++)
        {
            fdprintf(2, "%-20s",
            i < entries(opt)? opt[i].optname : cmode[i - entries(opt)]);
#endif
            if ((i+1) % 3 == 0)
                fdprintf(2, "\r\n\t  ");
        }
        fdprintf(2, "\r\n");
        return;
    }
    if (NetConP == 0 || NetConP->StateP == 0)
    {
        fdprintf(2, "No active connection\r\n");
        return;
    }
    if      (arg == DO)   type = MY_DO;
    else if (arg == DONT) type = MY_DONT;
    else if (arg == WILL) type = MY_WILL;
    else if (arg == WONT) type = MY_WONT;
    else
    {
        com_err(0, "Error in command table\r\n");
        return;
    }
    prevmatch = 0;
    candidate = -1;
#ifdef TELNET
    for (i = 0; i < entries(opt); i++)
        if (nmatch = compar(argv[1], opt[i].optname))
#endif
#ifdef THP
    for (i = 0; i < entries(opt) + 2; i++)
        if (nmatch = compar(argv[1], i < entries(opt)?
                    opt[i].optname : cmode[i - entries(opt)]))
#endif
            if (nmatch == -1)
            {
                candidate = i;
                break;
            }
            else if (nmatch > prevmatch)
            {
                candidate = i;
                prevmatch = nmatch;
            }
            else if (nmatch == prevmatch)
            {
                fdprintf(2, "%s: option not unique\r\n", argv[0]);
                return;
            }
    if (candidate == -1)
    {
        fdprintf(2, "%s: option not recognized. %s\r\n", argv[0], argv[1]);
        return;
    }

    if (candidate < entries(opt))    /* Normal neg. opt. */
    {
        if (opt[candidate].optlength == 1)
            negotiate(NetConP, 1, type, 1, &candidate);
#ifdef THP
        else if (candidate == OPT_RECONNECT)
        {
            if (argc != 4)
            {
                fdprintf(2, "Usage: %s %s host port\r\n", argv[0], argv[1]);
                return;
            }
            reconn.RHost = gethost(argv[2]);
            if (*atoiv(argv[3], &reconn.RPort))
            {
                fdprintf(2, "Non-numeric port: %s\r\n", argv[3]);
                return;
            }
            reconn.RType = START;
            rp = &reconn;
            r[0] = OPT_RECONNECT;
            for (i = 1; i < sizeof(reconn)+1; i++)
                r[i] = rp[i-1];
            negotiate(NetConP, 1, type, i, r);
        }
        else if (candidate == OPT_RECORD_SIZE)
        {
            if (argc != 3)
            {
                fdprintf(2, "Usage: %s %s record-size\r\n", argv[0], opt[candidate].optname);
                return;
            }
            if (*atoiv(argv[2], &value) || value < 0)
            {
                fdprintf(2, "Bad record size: %s\r\n", argv[2]);
                return;
            }
            r[0] = candidate;
            r[1] = value/256;
            r[2] = value%256;
            negotiate(NetConP, 1, type, 3, r);
        }
        else if (opt[candidate].optlength == 3 && opt[candidate].optsr)
        {
            if (argc<3 || argc>4 ||  (argc==4 && argv[3][0] != 's' && argv[3][0] != 'r')
            ||  *atoiv(argv[2], &value))
            {
                fdprintf(2, "Usage: %s %s value [sender|receiver]\r\n",
                argv[0], argv[1]);
                return;
            }
            r[0] = candidate;
            r[1] = argc == 3? SENDER : argv[3][0] == 's'? SENDER : RECEIVER;
            r[2] = value;
            negotiate(NetConP, 1, type, 3, r);
        }
#endif
        else
            fdprintf(2, "Not yet implemented: %s %s\r\n", argv[0], argv[1]);
    }
#ifdef THP
    else
    {
        candidate =- entries(opt);    /* candidate is now 0 or 1 */
        if (type == MY_WONT || type == MY_DONT)
            candidate = 1 - candidate;  /* Reverse state */
        if (type == MY_WILL || type == MY_WONT)
        {
            SendCtrl(NetConP, SET_MODE, 1, &candidate);
            NetConP->StateP->XMode = candidate;
        }
        else
            SendCtrl(NetConP, REQUEST_MODE, 1, &candidate);
    }
#endif
}

/* -------------------------- S E N D C T R L ----------------------- */
/*
 * SendCtrl(connp, ctrl, len, args) sends the args over the net
 * by putting them in the ToNetBuf for the given connection.
 * It puts in the character ctrl, followed by len characters from args.
 * For THP, it inserts a two-byte count.
 */
SendCtrl(connp, ctrl, alen, args)
    struct NetConn *connp;
    char ctrl;
    int alen;
    char *args;
{
    register int len;
    register char *ap;

    len = alen;    /* ctrl doesn't count */
    ap = args;
    Cputc(IAC, connp->ToNetBuf);
#ifdef THP
    Cputc(len >> 8, connp->ToNetBuf);     /* High byte of count */
    Cputc(len & 0377, connp->ToNetBuf);   /* Low byte of count */
#endif
    Cputc(ctrl, connp->ToNetBuf);
    while(len-- > 0)
        Cputc(*ap++, connp->ToNetBuf);
}

/* -------------------------- S E N D B U F ----------------------- */
/*
 * SendBuf(connp, len, buf) checks the buffer for characters which require
 * padding, and puts out THP REPEAT records for the appropriate number
 * of nulls, or discards the character, as required. The text itself
 * is also put out.
 *
 * Note that no attempt is made to keep CR and LF together. If the
 * user requested CR padding, it was undoubtedly because he wanted
 * CRs padded, not LFs.
 */
SendBuf(connp, len, inbufp)
    struct NetConn *connp;
    int len;
    char *inbufp;
{
#ifdef TELNET
    NetBuf(connp, len, inbufp);
#endif
#ifdef THP
    register char *op;
    register char *ip;
    struct SBlk *sbp;
    int optnum;
    int action;
    char obuf[CBUFSIZE];
    char rpt[2];    /* REPEAT record -- count, char */

    sbp = connp->StateP;
    rpt[1] = '\0';  /* Pad with nulls */

    op = &obuf[0];
    for (ip = inbufp; ip < inbufp+len; ip++)
    {
	switch(*ip)
	{
	case '\t':
	    optnum = OPT_HT_DISP;
	    break;
	case '\n':
	    optnum = OPT_LF_DISP;
	    break;
	case '\13':
	    optnum = OPT_VT_DISP;
	    break;
	case '\14':
	    optnum = OPT_FF_DISP;
	    break;
	case '\r':
	    optnum = OPT_CR_DISP;
	    break;
	default:
	    *op++ = *ip;
	    continue;
	}
	if (sbp->state[optnum][WILL_SIDE] == YES
	&& sbp->value[optnum][WILL_SIDE][0] == SENDER)
		action = sbp->value[optnum][WILL_SIDE][1];
	else
	    action = 0;
	action =& 0377;
/* Discard? */
	if (action == DISP_DISCARD)
	    continue;
/* Replace with CRLF? */
	else if (action == DISP_CRLF && (*ip == '\13' || *ip == '\14'))
	{
	    NetBuf(connp, op - obuf, obuf);
	    op = obuf;
	    SendBuf(connp, 2, "\r\n");	/* Handle padding on them, too */
	}
	else if (action)
	{
	    *op++ = *ip;
	    NetBuf(connp, op - obuf, obuf);
	    op = obuf;
	    rpt[0] = action;
	    SendCtrl(connp, REPEAT, 2, rpt);
	}
	else
	    *op++ = *ip;
    }
    if (op - obuf > 0)
	NetBuf(connp, op - obuf, obuf);
#endif
}

/* -------------------------- N E T B U F ------------------------- */
/*
 * NetBuf(connp, len, buf) sends the given bufferful of data over the net.
 * For TELNET, IACs are doubled. For THP,
 * if the connection mode is RECORD, or if the buffer contains an IAC,
 * the data is sent as a record of type DATA; otherwise it is just
 * sent as a stream.
 */
NetBuf(connp, len, buf)
    struct NetConn *connp;
    int len;
    char buf[];
{
#ifdef THP
    register char *ep;      /* Points to end of buf */
    register char *bp;      /* Current position in buf */
    register int chunk;
    int recfm;              /* Just for this bufferful */

    ep = buf + len;
    recfm = STREAM;
    if (connp->StateP->XMode == RECORD)
        recfm = RECORD;
    else
        for (bp = &buf[0]; bp < ep; bp++)
            if ((*bp & 0377) == (IAC & 0377))
            {
                recfm = RECORD;
                break;
            }

    for (bp = &buf[0]; bp < ep;)
    {
        chunk = ep - bp;
        if (recfm == RECORD && connp->RecSize > 0 && connp->RecSize < chunk)
            chunk = connp->RecSize;
        if (recfm == RECORD)
        {
            SendCtrl(connp, DATA, chunk, bp);
            bp =+ chunk;
        }
        else
            while (chunk-- > 0)
                Cputc(*bp++, connp->ToNetBuf);
    }
#endif
#ifdef TELNET
    register int l;
    register char *bp;

    l = len;
    bp = &buf[0];
    while(l-- > 0)
    {
        if ((*bp & 0377) == (IAC & 0377))
            Cputc(IAC, connp->ToNetBuf);
        Cputc(*bp++, connp->ToNetBuf);
    }
#endif
}

/* -------------------------- S E N D S Y N C H --------------------- */
/*
 * SendSynch(connp) sends a synch sequence. It should be called AFTER
 * the appropriate telnet op (e.g. IP) has been sent.
 */
SendSynch(connp)
struct NetConn *connp;
{

    SendCtrl(connp, DM, 0, 0);
    SendUrg(connp);
}

/* -------------------------- G L O B A L S ------------------------- */
