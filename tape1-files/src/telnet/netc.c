#
/*
 * Finite-state-machine for handling TELNET controls.
 * This file contains all of the real intelligence needed for
 * handling the TELNET protocol.
 * Use: first allocate a conn block using telinit(fd).
 * Then
 *	init ToNetBuf, ToUsrBuf to point at suitable areas
 *	for each char received from net
 *	    NetChar(connp, char)
 * where connp is a ptr to a NetConn block.
 * NetChar will write to ToUsrBuf or ToNetBuf as required. If ToNetBuf
 *	is not empty, write it out to the net.
 */

#include "stdio.h"
#include "tnio.h"
#include "telnet.h"

/* States of FSM; EXP stands for "expected" */

/* PROTINIT has already been defined. */
#define CTRL_EXP	  1
#define OPTION_EXP	  2
#define SUBNEG_OPT_EXP	  3
#define WITHIN_SUBNEG	  4
#define IAC_WITHIN_SUBNEG 5

/* -------------------------- C A L L ------------------------------- */
/*
 * The call macro is used to call optionally-defined functions.
 * Given a pointer to a function, and the arguments for that function,
 * it calls the function if and only if the pointer is non-null.
 */
#define call(f,args) { if ((f) != NULL) (*(f))args; }

struct OptTab  /* Option negotiation table */
{
    int   optspec;     /* Set to skip already-in-effect test (see Negotiate) */
    int (*testf)();    /* Test function */
    int (*actf)();     /* Action routine */
    int (*sbf)();      /* Subnegotiation routine */
};

extern Never(), NoAct();
struct OptTab opt[N_OPTIONS] =
{

/* ---- Spec --- Test ------ Act ---- Subneg ----- */

	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
	{0,	 NULL,       NULL,    NULL},
};

/* The generic "unimplemented" option is the last one. */

#define OPT_UNIMPL (N_OPTIONS - 1)

int verbose;

/* -------------------------- U S E R C H A R ----------------------- */

#define UserChar(c,conn) Bputc(c,connp->ToUsrBuf)

/* -------------------------- N E T C H A R ------------------------- */
/*
 * Handle all protocol controls. For option negotiation, convert WILL WONT DO
 * DONT into more convenient form, save it in OptCtrl, and call Negotiate()
 * when the control has been assembled.
 * For other controls, wait until they are all gathered up and then act on them.
 * Call UserChar as required.
 */
NetChar(connp, c)
    register NETCONN *connp;
    register int c; /* Unsigned char */
{
    register int state;
    extern int verbose;

    c &= 0377;
    state = connp->ProtState;

    switch (state)
    {
    case PROTINIT:
	if (c == IAC)
	    state = CTRL_EXP;
	else
	{
#	ifdef NCP
	    if (connp->INSCount <= 0)
#	endif NCP
#	ifdef TCP
	    if (!(connp->UrgInput))
#	endif TCP
		UserChar(c, connp);
	}
	break;

    case CTRL_EXP:
	switch(c)
	{
	case SB:
	    state = SUBNEG_OPT_EXP;
	    Bsetbuf(connp->ToOptBuf, connp->SBarray, sizeof(connp->SBarray));
	    break;
	case WILL:
	    connp->OptCtrl = TN_WILL;
	    state = OPTION_EXP;
	    break;
	case DO:
	    connp->OptCtrl = TN_DO;
	    state = OPTION_EXP;
	    break;
	case WONT:
	    connp->OptCtrl = TN_WONT;
	    state = OPTION_EXP;
	    break;
	case DONT:
	    connp->OptCtrl = TN_DONT;
	    state = OPTION_EXP;
	    break;
	default:
	    NetCtrl(connp, c);
	    state = PROTINIT;
	    break;
	}
	break;
    case OPTION_EXP:
	connp->OptNum = c;
	Negotiate(connp, 0, connp->OptCtrl, connp->OptNum);
	state = PROTINIT;
	break;
    case SUBNEG_OPT_EXP:
	connp->OptNum = c;
	state = WITHIN_SUBNEG;
	break;
    case WITHIN_SUBNEG:
	if (c == IAC)
	    state = IAC_WITHIN_SUBNEG;
	else
	    Bputc(c, connp->ToOptBuf);
	break;
    case IAC_WITHIN_SUBNEG:
	if (c == IAC)
	{
	    Bputc(c, connp->ToOptBuf);
	    state = WITHIN_SUBNEG;
	}
	else if (c == SE)
	{
	    if (verbose)
	    {
		int i;
		char * p;

		fprintf(stderr, "Received subneg for option %d: ", connp->OptNum);
		for (p = Baddr(connp->ToOptBuf), i = 0; i < Blen(connp->ToOptBuf); i++, p++)
		    fprintf(stderr, "0%o ", *p & 0377);
		fprintf(stderr, "\n");
	    }

	    if (opt[connp->OptNum].sbf != NULL)
		(*(opt[connp->OptNum].sbf))(connp, connp->OptNum,
					    Baddr(connp->ToOptBuf), Blen(connp->ToOptBuf));
	    state = PROTINIT;
	}
	else
	    state = WITHIN_SUBNEG;
	break;
    default:
	fprintf(stderr, "TELNET library error: unknown state %d", state);
	state = PROTINIT;
	break;
    }

    connp->ProtState = state;
    return(state);
}

/* -------------------------- N E T C T R L ------------------------- */
/*
 * Handle all one-character TELNET controls.
 */
NetCtrl(connp, c)
    register NETCONN *connp;
    int c;  /* Unsigned char */
{
    switch(c)
    {
    case IAC:
	UserChar(IAC, connp);
	break;
    case DM:
#	ifdef NCP
	    connp->INSCount--;
#	endif NCP
#	ifdef TCP
	    if (Bleft(connp->FmNetBuf) == 0) connp->UrgInput = 0;
#	endif
	break;
    case BREAK:
	call(connp->BREAKf, (connp));
	break;
    case AYT:
	call(connp->AYTf, (connp));
	break;
    case IP:
	call(connp->IPf, (connp));
	break;
    case EC:
	call(connp->ECf, (connp));
	break;
    case EL:
	call(connp->ELf, (connp));
	break;
    case AO:
	call(connp->AOf, (connp));
	break;
    case GA:
	call(connp->GAf, (connp));
	break;
    case NOP:
	break;
    default:
	fprintf(stderr, "Unknown TELNET control 0%o (%d.) ignored", c, c);
	break;
    }
}

/* -------------------------- N E G O T I A T E --------------------- */
/*
 * Handle actual option negotiation. First arg is pointer to connection block.
 * Second is 1 if originating request, 0 if received over net.
 * Third is WILL, WONT, DO, or DONT.
 * Fourth is option number.
 * Return value is 0 if OK, nonzero if some problem, like option number out
 * of range or test function said no.
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
 * is 80, or if the receiver is managing line width. Such deeper issues are
 * finessed by allowing the user to set opt.optspec, which bypasses this check
 * altogether.
 *
 * If it is not in effect, we call the test function to see whether it can be
 * done, and reply appropriately. We normally don't bother the test function if
 * the foreign host is saying DONT or WONT, since such requests can never be
 * refused -- just ignored, if they are already in effect. (Hence, the test function
 * must be called anyway if opt.optspec is set.)
 * If everything checks, an appropriate reply is sent off, and if
 * the result of the test was that it can be done, the action routine is called.
 *
 * Back on the originating host, when the reply is received, the action routine
 * is called. It's too late to test anything; if it can be done, it will be.
 * Note that conflicts (different line widths, etc.) should be resolved in the
 * action routine.
 */

/* Useful defines */
#define NO  0
#define YES 1

Negotiate(connp, org_here, op, opnum)
    register NETCONN * connp;
    int org_here;
    int op;
    int opnum;
{
#define DO_SIDE 0
#define WILL_SIDE 1
    int side;	/* DO_SIDE or WILL_SIDE */
    int nstate; /* NO (DONT or WONT) or YES (DO or WILL) */
    int rop;
    int verdict;
    extern int verbose;

    if (setexit())
	return(1);  /* Some error happened */

    rop = op;
    if (org_here)   /* Reverse DO and WILL */
	if	(op == TN_DO)	op = TN_WILL;
	else if (op == TN_WILL) op = TN_DO;
	else if (op == TN_DONT) op = TN_WONT;
	else			op = TN_DONT;

    side = (op == TN_DO || op == TN_DONT)? DO_SIDE : WILL_SIDE;
    nstate = (op == TN_DO || op == TN_WILL)? YES : NO;

    if (opnum >= OPT_UNIMPL)
	if (org_here)
	    rcmderr(0, "Unimplemented option.\n");
	else if (nstate == YES)
	{
	    SendReply(connp, CANT, op, opnum);
	    return(0);
	}

    if (org_here)			    /* Origination */
    {
	if (nstate == NO ||
	    (opt[opnum].testf != NULL && (*opt[opnum].testf)(connp, org_here, op, opnum) == CAN))
	{
	    if (verbose) snap("Sending", rop, opnum);
	    SendReply(connp, YES, op, opnum);
	    connp->Orig[opnum][side] = YES;
	}
	else
	{
	    if (verbose) snap("Unimplemented:", rop, opnum);
	    reset(1);
	}
    }

/* Reply to foreign origination */
    else if (connp->Orig[opnum][side] == NO)
    {
	if (opt[opnum].optspec && opt[opnum].testf != NULL)
	    verdict = (*opt[opnum].testf)(connp, org_here, op, opnum);
	else
	{
	    if (connp->OptState[opnum][side] == nstate)
		verdict = ALREADY;
	    else if (nstate == NO)
		verdict = CAN; /* Can always DONT/WONT */
	    else if (opt[opnum].testf == NULL)
		verdict = CANT;
	    else
		verdict = (*opt[opnum].testf)(connp, org_here, op, opnum);
	}
	if (verdict == ALREADY)
	    return(1);

	if (nstate == NO)
	    verdict = CAN;  /* Force the test function to comply with the protocol */

	SendReply(connp, verdict, op, opnum);
	if (verdict == CAN)
	{
	    if (verbose) snap("Affirming", op, opnum);
	    call(opt[opnum].actf, (connp, op, opnum));
	    connp->OptState[opnum][side] = nstate;
	}
    }

    else				     /* Reply to our origination */
    {
	if (verbose) snap("Reply was", op, opnum);
	call(opt[opnum].actf, (connp, op, opnum));
	connp->Orig[opnum][side] = NO;
	connp->OptState[opnum][side] = nstate;
    }
    return(0);
}

/* -------------------------- S E N D R E P L Y --------------------- */
/*
 * SendReply(connp, reply, op, opnum) replies to a negotiated
 * option. If reply is nonzero, it replies affirmatively; if reply is
 * zero, it replies negatively. op is TN_WILL/WONT/DO/DONT; opnum is the
 * option number.
 */
SendReply(connp, reply, op, opnum_int)
    struct NetConn *connp;
    int reply;
    int op;	/* Unsigned char */
    int opnum_int;  /* Unsigned char */
{
    char opnum;
    extern int verbose;

    opnum = opnum_int;
    switch (op)
    {
    case TN_DO:
	if (reply == 0)
	{
	    SendCtrl(connp, WONT, 1, &opnum);
	    if (verbose) snap("Refusing", op, opnum);
	}
	else
	    SendCtrl(connp, WILL, 1, &opnum);
	break;
    case TN_WILL:
	if (reply == 0)
	{
	    SendCtrl(connp, DONT, 1, &opnum);
	    if (verbose) snap("Refusing", op, opnum);
	}
	else
	    SendCtrl(connp, DO, 1, &opnum);
	break;
    case TN_DONT:
	SendCtrl(connp, WONT, 1, &opnum);
	break;
    case TN_WONT:
	SendCtrl(connp, DONT, 1, &opnum);
	break;
    }
}

/* -------------------------- N E V E R ----------------------------- */

Never()
{
    return(CANT);
}

/* -------------------------- A L W A Y S --------------------------- */

always()
{
    return(CAN);
}

/* -------------------------- N O A C T ----------------------------- */

NoAct()
{
    return;
}

/* -------------------------- S E N D C T R L ----------------------- */
/*
 * SendCtrl(connp, OptCtrl, len, args) sends the args over the net
 * by calling SendC.
 * It puts in IAC, OptCtrl, and len characters from args.
 */
SendCtrl(connp, OptCtrl, alen, args)
    struct NetConn *connp;
    char OptCtrl;
    int alen;
    char *args;
{
    register int len;
    register char *ap;
    extern int AutoFlush;

    len = alen;    /* OptCtrl doesn't count */
    ap = args;
    SendC(connp, IAC);
    SendC(connp, OptCtrl);
    while(len-- > 0)
	SendC(connp, *ap++);
    if (AutoFlush)
	telflush(connp);
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

/* -------------------------- B R E A K D E F A U L T --------------- */
/*
 * Default handler for TELNET BREAK controls. Writes a NUL.
 */
BREAKdefault(connp)
    struct NetConn *connp;
{
    UserChar('\0', connp);
}

/* -------------------------- A Y T D E F A U L T ------------------- */
/*
 * Default handler for TELNET AYT controls. Sends back IAC NOP, "Yes\n".
 */
AYTdefault(connp)
    struct NetConn *connp;
{
    SendCtrl(connp, NOP, 0, 0);
    telwrite(connp, "Yes\r\n", 5);
}

/* -------------------------- I P D E F A U L T --------------------- */
/*
 * Default handler for TELNET IP controls. Sends a RUBOUT to the user,
 * returns a SYNCH to the other side.
 */
IPdefault(connp)
    struct NetConn *connp;
{
    UserChar('\177', connp);
    SendSynch(connp);
}

/* -------------------------- S N A P ------------------------------- */
/*
 * dump out a description of an option. Assumes that opnum has been made
 * to fit inside the option table.
 */
snap(msg, op, opnum)
    char *msg;
    int op;   /* Unsigned char */
    int opnum;/* Unsigned char */
{
    static char *which[] = { "don't", "do", "will", "won't"};
    static char *names[] = {
	"binary",
	"echo",
	"reconnect",
	"suppress-go-ahead",
	"set-record-size",
	"status",
	"timing-mark",
	"RCTE",
	"linewidth",
	"pagesize",
	"cr-disp",
	"ht-stops",
	"ht-disp",
	"ff-disp",
	"vt-stops",
	"vt-disp",
	"lf-disp",
    };

    fprintf(stderr, "%s %s ", msg, which[op]);
    if (opnum >= 0 && opnum < sizeof(names)/sizeof(names[0]))
	fprintf(stderr, "%s ", names[opnum]);
    else
	fprintf(stderr, "%d ", opnum);
    fprintf(stderr, "\r\n");
}

/* -------------------------- S E T O P T --------------------------- */
/*
 * Set up for an option.
 */
setopt(option, skflag, test, actor, subneg)
    int option;
    int skflag;
    int (*test)();
    int (*actor)();
    int (*subneg)();
{
    if (option >= N_OPTIONS)
	return(-1);
    opt[option].optspec = skflag;
    opt[option].testf = test;
    opt[option].actf = actor;
    opt[option].sbf = subneg;

    return(0);
}

/* -------------------------- S E N D O P T ------------------------- */
/*
 * Send an option negotiation request. The work is actually
 * done by Negotiate. Return error code from Negotiate.
 */
sendopt(connp, opcode, option)
    register NETCONN * connp;
    int opcode;
    int option;
{
    return(Negotiate(connp, 1, opcode, option));
}

/* -------------------------- S E N D S U B ------------------------- */
/*
 * Send a subnegotiation.
 */
sendsub(connp, option, subp, sublen)
    register NETCONN * connp;
    char option;
    char * subp;
    int sublen;
{
    SendCtrl(connp, SB, 0, 0);
    telwrite(connp, &option, 1);
    telwrite(connp, subp, sublen);
    SendCtrl(connp, SE, 0, 0);
    if (verbose)
    {
	fprintf(stderr, "Sending subneg for option %d: ", option);
	while (sublen-- > 0)
	    fprintf(stderr, "0%o ", *subp++ & 0377);
	fprintf(stderr, "\n");
    }
}

/* -------------------------- T E L S T A T U S --------------------- */
/*
 * Report status of the given TELNET option.
 */
telstatus(connp, option)
    register NETCONN * connp;
    int option;
{
    int retval;

    retval = 0;
    if (option < 0 || option >= N_OPTIONS)
	return(retval);
    if (connp->OptState[option][DO_SIDE])
	retval += 1;
    if (connp->OptState[option][WILL_SIDE])
	retval += 3;
    return(retval);
}

/* -------------------------- S E N D C T L ------------------------- */
/*
 * Send a TELNET control. AO and IP must be followed by a SYNCH.
 */
sendctl(connp, c)
    register NETCONN * connp;
    int c;
{
    switch(c)
    {
    case EC:
    case EL:
    case BREAK:
    case AYT:
    case NOP:
    case GA:
	SendCtrl(connp, c, 0, 0);
	break;
    case IP:
    case AO:
	SendCtrl(connp, c, 0, 0);
	SendSynch(connp);
	break;
    }
}

/* ------------------------------------------------------------------ */
