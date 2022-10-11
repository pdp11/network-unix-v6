#include "globdefs.h"
#include "cbuf.h"
#include "tcp.h"
#include "netconn.h"

#define entries(f) (sizeof(f)/sizeof(f[0]))

/*
 * TCP - specific routines
 */

#define SERVER 1      /* Default port of server */

/* -------------------------- N E T O P E N ------------------------ */
/*
 * netopen(arg, argc, argv) does a TcpInit if none has been done yet,
 * then a TcpOpen to the specified host. It then grabs a handy connection
 * block and copies the connection block it has built up into it.
 * We do NOT yet allocate a state block for the connection because doing so
 * also captures the original state of the terminal; until the connection
 * becomes established, it is not known whether the terminal will be the
 * controlling terminal of this process or a pty.
 * At any rate, netopen then enters the connection's
 * file descriptors in the io queue, providing the NetConn pointer as
 * the argument. The actual establishing of the connection is done by
 * the CmdNet handler.
 */
netopen(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int error;
    int lport, fport, netnum;
    int servsw;
    int sprec, rprec, msecur, asecur;
    int ntcc, tccnum;
    int opentype;
    int i, j;
    int pre;
    int host_arg_index;
    long lnum;
    char *p, *endp;
    struct CAB cab;
    struct UserTcb *utcbp;
    struct NetConn *np;
    int useptysw;
    int exitonclose;
    char *init[entries(np->Init)];
    struct {
	char l_hoi;
	char l_network;
	int  l_imp;
};

    static int DidInit;  /* Set when an Init has been done */
    extern int errno;
#ifdef LOGGING
    extern int logging;
#endif
    extern struct NetConn *NetConP;
    extern int NoUsrEOF;
    extern int localnet;
    extern CmdNet(), netintr();
    extern ToUser();
    extern char *atoiv();
    extern FromNet(), ToNet();
    extern NetCmd();
    extern long gethost();

#define LEAVE { if (arg) exit(1); return; }

/* Help */

    if (argc == 0)
    {
	fdprintf(2, "Open a network connection\r\n");
	return;
    }
    else if (argc == 1)
    {
	fdprintf(2, "Usage: %s [host] [options]\r\n", argv[0]);
	fdprintf(2, "-lp #\r\n-fp #\r\n-ps #\r\n-pr #\r\n-sa #\r\n");
	fdprintf(2, "-sm #\r\n-tcc #[,#] ...\r\n");
	fdprintf(2, "-open\r\n-listen\r\n-accept\r\n");
	fdprintf(2, "-p\r\n-server\r\n-init command ...\r\n");
	return;
    }

/* Suspend current connection, if any */

    ChgConn('s');

/* Set defaults */

    opentype = 0;  /* Open */
    cab.HostHigh = 0;
    cab.HostLow = 0;
    localnet = 10; /* Arpanet (should use sgnet) */
    netnum = localnet;
    lport = 0;
    fport = SERVER;
    init[0] = 0;
    useptysw = 0;
    exitonclose = 0;
    rprec = sprec = asecur = msecur = 0;
    ntcc = 0;
    servsw = 0;
    pre = 0;	 /* Preemption is normally off */

/* Process arguments */

    host_arg_index = 0;
    argc--;
    for (i = 1; i <= argc; i++)
    {
	if (seq(argv[i], "-server"))
	{
	    lport = SERVER;
	    opentype = 1;
	    fport = 0;
	    netnum = 0;
	    servsw++;
	}
#ifdef LOGGING
	else if (seq(argv[i], "-log"))
	{
	    if (i == argc)
		LogInit("thp.log");
	    else
		LogInit(argv[++i]);
	    logging++;
	}
#endif
	else if (seq(argv[i], "-p"))
	    pre++;
	else if (seq(argv[i], "-lp"))
	{
	    if (i == argc || *atoiv(argv[++i], &lport))
	    {
		fdprintf(2, "%s: no source port number\r\n", argv[0]);
		LEAVE;
	    }
	}
	else if (seq(argv[i], "-fp"))
	{
	    if (i == argc || *atoiv(argv[++i], &fport))
	    {
		fdprintf(2, "%s: no destination port number\r\n", argv[0]);
		LEAVE;
	    }
	}
	else if (seq(argv[i], "-ps"))
	{
	    if (i == argc || *atoiv(argv[++i], &sprec))
	    {
		fdprintf(2, "%s: No send precedence specified\r\n", argv[0]);
		LEAVE;
	    }
	}
	else if (seq(argv[i], "-pr"))
	{
	    if (i == argc || *atoiv(argv[++i], &rprec))
	    {
		fdprintf(2, "%s: No receive precedence specified\r\n", argv[0]);
		LEAVE;
	    }
	}
	else if (seq(argv[i], "-sa"))
	{
	    if (i == argc || *atoiv(argv[++i], &asecur))
	    {
		fdprintf(2, "%s: No absolute security specified\r\n", argv[0]);
		LEAVE;
	    }
	}
	else if (seq(argv[i], "-sm"))
	{
	    if (i == argc || *atoiv(argv[++i], &msecur))
	    {
		fdprintf(2, "%s: No maximum security specified\r\n", argv[0]);
		LEAVE;
	    }
	}
	else if (seq(argv[i], "-tcc"))
	{
	    if (i == argc)
	    {
		fdprintf(2, "%s: No TCC list\r\n", argv[0]);
		LEAVE;
	    }
	    i++;
	    for (p = argv[i], ntcc = 0; ; p = endp + 1, ntcc++)
	    {
		if (ntcc >= entries(cab.TCC))
		{
		    fdprintf(2, "%s: Too many TCCs. Maximum is %d\r\n",
				argv[0], entries(cab.TCC));
		    LEAVE;
		}
		endp = atoiv(p, &tccnum);
		if (*endp != '\0' && *endp != ',')
		{
		    fdprintf(2, "%s: Bad TCC list\r\n", argv[0]);
		    LEAVE;
		}
		cab.TCC[ntcc] = tccnum;
		if (*endp == '\0')
		    break;
	    }
	    ntcc++;
	}
	else if (seq(argv[i], "-open"))
	    opentype = 0;  /* Open */
	else if (seq(argv[i], "-listen"))
	{
	    opentype = 1;  /* Listen */
	    fport = 0;
	    netnum = 0;
	}
	else if (seq(argv[i], "-accept"))
	    opentype = 2;  /* Accept */
	else if (seq(argv[i], "-init"))
	{
	    i++;
	    j = 0;
	    init[0] = 0;

	    if (i > argc)
		fdprintf(2, "%s: no args after -init\r\n", argv[0]);
	    else if (argc - i + 1 > entries(init))
		fdprintf(2, "%s: too many args after -init. %d max\r\n",
				 argv[0], entries(init));
	    else
		for (; i <= argc && j < entries(init); i++, j++)
		    scopy(argv[i], init[j] = alloc(slength(argv[i])+1), -1);
	    init[j] = 0;    /* Terminate command list with 0 */

	    if (j == 0)
		LEAVE;
	}
	else if (argv[i][0] == '-')
	{
	    fdprintf(2, "%s: unknown flag '%s'\r\n", argv[0], argv[i]);
	    LEAVE;
	}
	else if (host_arg_index == 0)
	    host_arg_index = i;
	else
	{
	    fdprintf(2, "%s: extraneous argument '%s'\r\n", argv[0], argv[i]);
	    LEAVE;
	}
    }

    if (host_arg_index != 0)
    {
	lnum = gethost(argv[host_arg_index]);
	if (lnum <= 0)
	{
	    fdprintf(2, "Bad host specifier: %s\n", argv[host_arg_index]);
	    LEAVE;
	}
	netnum = lnum.l_network;
	cab.HostHigh = lnum.l_hoi;
	cab.HostLow = lnum.l_imp;
    }

/* Call up TCP */

    if (DidInit == 0)
	if (error = TcpInit(40))
	    fatal(0, "TcpInit failed. %s", TcpErr(error));

/* Initialize the CAB */

    cab.Network = netnum;
#ifdef NeverDefined
    cab.HostLow = hostnum & 077;	     /* Low 6 bits are imp */
    cab.HostHigh = (hostnum & 0300) >> 6; /* High 2 bits are host */
#endif
    cab.SrcPort = lport;
    cab.DstPort = fport;
    cab.PrecSend = 0;
    if (sprec) cab.PrecSend = sprec|0200;
    cab.PrecRecv = 0;
    if (rprec) cab.PrecRecv = rprec|0200;
    cab.SecurAbs = 0;
    if (asecur) cab.SecurAbs = asecur|0200;
    cab.SecurMax = 0;
    if (msecur) cab.SecurMax = msecur|0200;
    cab.TccCnt = ntcc;

/* Ready to open */

    if ((error = TcpOpen(&cab, &utcbp, opentype, 40)))
    {
	com_err(0, "TcpOpen failed. %s", TcpErr(error));
	LEAVE;
    }
/* It seems to have worked */

    NetConP = AllocConn();
    if (NetConP == -1)
    {
	com_err(0, "No more room for connections");
	LEAVE;
    }
    NetConP->UsePtySw = useptysw;
    NetConP->ExitOnClose = exitonclose;
    for (i = 0; i < entries(init); i++)
	NetConP->Init[i] = init[i];
    NetConP->SendSecur = asecur? asecur : msecur;
    if (host_arg_index != 0)
	ChgConn('e', NetConP, argv[host_arg_index]);
    else
	ChgConn('e', NetConP, 0);
    NetConP->UtcbP = utcbp;

/* Chain the UTCB back to our connection block (for CmdNet) */

    utcbp->UInfo1 = NetConP;

    if (DidInit == 0)
    {
	IoxEnter(&CmdNet, 0);	 /* One serves for all connections */
	awtenb(utcbp->CmdFds);	/* Be sure to awaken on that fd */
    }
    DidInit = 1;

    if (pre && (cab.PrecSend&0177) < CAT_I && (cab.PrecRecv&0177) < CAT_I)
    {
	cab.Network = 0;
	cab.HostHigh = 0;
	cab.HostLow = 0;
	cab.SrcPort = 1000 + (getuid() & 0377);
	cab.DstPort = 0;

#define IncPrec(prec) (((prec & 0177) + 1) | 0200)
	cab.PrecSend = IncPrec(cab.PrecSend);
	cab.PrecRecv = IncPrec(cab.PrecRecv);

	if (error = TcpOpen(&cab, &utcbp, 1 /*LISTEN*/, 20))
	    com_err(0, "Preemption open failed. %s", TcpErr(error));
	else
	{
	    fdprintf(2, "Listening on %d\r\n", cab.SrcPort);

	    np = AllocConn();
	    np->UsePtySw = useptysw;
	    np->ExitOnClose = exitonclose;
	    for (i = 0; i < entries(init); i++)
		np->Init[i] = init[i];
	    np->SendSecur = asecur? asecur : msecur;
	    ChgConn('e', np, "preempt");
	    np->UtcbP = utcbp;
	    np->StateP = AllocSB();
	    IoxEnter(&NetCmd, np);

/* Chain the UTCB back to our connection block (for CmdNet) */

	    utcbp->UInfo1 = np;

	    IoEnter(utcbp->SendFds, 0, &ToNet, np);
	    IoEnter(utcbp->RecvFds, &FromNet, 0, np);
	}
    }

    fdprintf(2, "Trying...\r\n");

    if (servsw)
    {
	signal(1, 1);
	signal(2, 1);
	signal(3, 1);
	NetConP->ExitOnClose = 1;
	NetConP->UsePtySw = 1;
	NoUsrEOF = 1;
    }
    else
    {
	signal(2, &netintr);
	IoEnter(1, 0, &ToUser, 0);
    }
}

/* -------------------------- C M D N E T --------------------------- */
/*
 * CmdNet(arg) is called when there is activity on th read side
 * of the TCP command port. It calls TcpCmd to find out what happened.
 * If the connection is just starting up (ESTAB or SYNRECD) perform
 * initialization sequence, including allocating a state block for it
 * if necessary.
 */
CmdNet(junk)
    int junk;	 /* Since CmdNet services all connections, this is meaningless */
{
    int error;
    int num, i;
    int c;
    struct UserTcb *utcbp;
    struct NetConn *connp;
    struct CmdBlk Cmdbuf;

    extern int verbose;  /* For urgent notice */
    extern FromNet(), ToNet();
    extern NetCmd();
    extern struct NetConn *NetConP;
    extern struct NetConn *PreP;

/* Get the command (if any) */

    Cmdbuf.CNFlags = 0;  /* Clear flags first -- tcplib bug */
    error = TcpCmd(&Cmdbuf);
    if (error)
    {
	if (error != ENOCHNG)
	    com_err(0, "TcpCmd failed. %s", TcpErr(error));
	if (error == ECMDCLS)
	    fatal(0, "TCP died. Exiting.");
	return(1);
    }
    utcbp = Cmdbuf.XPtr;
    connp = utcbp->UInfo1;

    num = utcbp->ConnIndex;
    if (Cmdbuf.CFlags & FSTATECHANGE)
    {
	switch(Cmdbuf.NewState)
	{
            case ESTAB:
            case SYNRECD:    /* Connection is for real now */
                if (connp->Type == PREEMPT)
                {
                    if (Cmdbuf.NewState == ESTAB)
                        fdprintf(2, "Preempting connection established\r\n");
                    PreP = connp;
                }
                else if (Cmdbuf.NewState == ESTAB)
                    fdprintf(2, "Established\r\n");
                else /* SYNRECD */ if (verbose)
                    fdprintf(2, "Syn Received\r\n");

                if (connp->UsePtySw)
                    usepty(connp);
                connp->UsePtySw = 0;
                if (connp->StateP == 0)
                {
                    connp->StateP = AllocSB();
                    if (connp == NetConP)
                        ChgSB(connp->StateP);
                    IoEnter(utcbp->SendFds, 0, &ToNet, connp);
                    IoEnter(utcbp->RecvFds, &FromNet, 0, connp);
                    IoxEnter(&NetCmd, connp);
                }
                if (connp->Init[0]) /* Execute initial command sequence */
                    for (i = 0; connp->Init[i] != 0; i++)
                        ExecCmd(connp->Init[i], slength(connp->Init[i]));
                connp->Init[0] = 0; /* Don't execute it any more... */
                break;
            case CLOSED:     /* We have both agreed it's closed */
                fdprintf(2, "Closed\r\n");
                netabort(connp, 1, 0);    /* Abort connection */
                break;
            case CLOSEWAIT: /* Other side did TcpClose */
                fdprintf(2, "Foreign process closed\r\n");
                llclose(connp);
                break;
            default:
                com_err(0, "%d: new state %d", num, Cmdbuf.NewState);
                break;
        }
        Cmdbuf.CFlags =& ~FSTATECHANGE;
    }
    if (Cmdbuf.CFlags & FURGENT)     /* Urgent information */
    {
        if (verbose)
            fdprintf(2, "Urgent count %s\r\n", locv(utcbp->UrgCount));
        BeginSynch(connp);  /* Take appropriate action */
        Cmdbuf.CFlags =& ~FURGENT;
    }
    if (Cmdbuf.CFlags & FDEAD)    /* Foreign process deaf */
    {
        if (verbose)
            fdprintf(2, "Foreign process not responding...\r\n");
        c = utcbp->CNState;
        if (c != ESTAB && c != CLOSEWAIT)
        {
            fdprintf(2, "Foreign process not responding, aborting\r\n");
            netabort(connp, 1, 0);
        }
        Cmdbuf.CFlags =& ~FDEAD;
    }
    if (Cmdbuf.CFlags & FALIVE)    /* Foreign process regained hearing */
    {
        if (verbose)
            fdprintf(2, "Foreign process responding again\r\n");
        Cmdbuf.CFlags =& ~FALIVE;
    }
    if (Cmdbuf.CFlags & FNETMSG)  /* Net information message */
    {
        if (Cmdbuf.NetMsg & NNOHOST)
            fdprintf(2, "Host not responding\r\n");
        if (Cmdbuf.NetMsg & 0177776)
            fdprintf(2, "Net Message = 0%o\r\n", Cmdbuf.NetMsg & 0177776);
        if (Cmdbuf.NetMsg == 0)
            fdprintf(2, "Empty net message\r\n");
        Cmdbuf.CFlags =& ~FNETMSG;
    }
    if (Cmdbuf.CFlags & FRESET)  /* Reset */
    {
        fdprintf(2, "Connection reset\r\n");
        Cmdbuf.CFlags =& ~FRESET;
    }
    if (Cmdbuf.CFlags & FREJECT) /* Rejecting */
    {
        fdprintf(2, "Foreign TCP rejecting...\r\n");
        Cmdbuf.CFlags =& ~FREJECT;
    }
    if (Cmdbuf.CFlags & FSECTOOHIGH)
    {
        fdprintf(2, "Security level out of range\r\n");
        Cmdbuf.CFlags =& ~FSECTOOHIGH;
    }
    if (Cmdbuf.CFlags & FPRECCHNG)
    {
        fdprintf(2, "Send precedence raised to %d\r\n", Cmdbuf.NewPrec);
        Cmdbuf.CFlags =& ~FPRECCHNG;
    }
    if (Cmdbuf.CFlags)
    {
        fdprintf(2, "Flags 0%o ignored", Cmdbuf.CFlags);
        Cmdbuf.CFlags = 0;
    }
    return(0);
}

/* -------------------------- T O N E T ----------------------------- */
/*
 * ToNet(fd, cap, connp) is called whenever there is capacity on the write side
 * of the SendFds. It calls TcpSend to do the actual work.
 * If there is no data to send, it checks for EOF, and does an llclose if it
 * is set.
 */
ToNet(fd, cap, connp)
    int fd;
    int cap;
    struct NetConn *connp;
{
    int error;
    struct UserTcb *utcbp;
    char buffer[CBUFSIZE];    /* So it can't overflow */
    register char *bp;
    register char *ubp;
#ifdef LOGGING
    extern int logging;
#endif

    if (Cempty(connp->ToNetBuf))
	if (connp->ToNetEOF == 1)
	{
	    llclose(connp);
	    connp->ToNetEOF = 2;    /* Dead! */
	    return(0);
	}
	else
	    return(1);	/* Nothing to send */

    if (connp->ToNetEOF == 2)  /* Connection dead, don't send */
	return(1);

    bp = buffer;
    while (!Cempty(connp->ToNetBuf))
	*bp++ = Cgetc(connp->ToNetBuf);
#ifdef LOGGING
    if (logging)
	Log(1, "r", &buffer, bp - &buffer, -1);
#endif
    utcbp = connp->UtcbP;
    if (connp->OutSynCnt) /* Urgent data */
	if (error = TcpSend(utcbp, buffer, connp->OutSynCnt, connp->SendSecur,
				  1/*EOL*/, 1/*URG*/))
	{
	    if (error != ENOSPC)
		com_err(0, "%d TcpSend failed. %s", utcbp->ConnIndex, TcpErr(error));
	    for (ubp = buffer; ubp < bp; ubp++)
		Cputc(*ubp, connp->ToNetBuf);
	    return(1);	/* Try again some other time */
	}
    ubp = buffer + connp->OutSynCnt;
    connp->OutSynCnt = 0;	  /* All urgent stuff delivered */
    if (ubp < bp)	   /* Anything not so urgent? */
	if (error = TcpSend(utcbp, ubp, bp - ubp, connp->SendSecur, 1/*EOL*/,
				  0/*Not URG*/))
	{
	    if (error != ENOSPC)
		com_err(0, "%d TcpSend failed. %s", utcbp->ConnIndex, TcpErr(error));
	    for (; ubp < bp; ubp++)
		Cputc(*ubp, connp->ToNetBuf);
	    return(1);
	}
    return(0);
}

/* -------------------------- F R O M N E T ------------------------- */
/*
 * FromNet(fd, cap, connp) is called when there is capacity on the read side of
 * the RcvFds. Data obtained from TcpReceive is put into FmNetBuf.
 */
FromNet(fd, cap, aconnp)
    int fd;
    int cap;
    struct NetConn *aconnp;
{
    register char *p;
    register struct NetConn *connp;
    int i;
    int error;
    int capbuf[2];     /* for The Last Capacity */
    int nread, nreq;
    int UrgFlag;
    char fnbuffer[CBUFSIZE];
#ifdef LOGGING
    extern int logging;
#endif

    connp = aconnp;

/* Compare eyes to stomach and adjust accordingly */

    if (connp->InSynCnt <= 0)
	nreq = CBUFSIZE - Clen(connp->FmNetBuf);
    else
	nreq = CBUFSIZE;
    if (nreq == 0)
	return(1);
    error = TcpReceive(connp->UtcbP, fnbuffer, nreq, &nread, &UrgFlag);
#ifdef LOGGING
    if (logging)
	Log(2, "r", &fnbuffer, nread, -1);
#endif
    if (error)
    {
	com_err(0, "%d TcpReceive failed. %s", connp->UtcbP->ConnIndex, TcpErr(error));
	return;
    }
    if (UrgFlag)
	BeginSynch(connp);

    for (p = fnbuffer; nread > 0; nread--, p++)
	Cputc(*p & 0377, connp->FmNetBuf);
/*
 * If we are in the CLOSED state, all that was left to do was read in
 * any data left in the receive port. So if the receive port is empty,
 * it is safe to abort the connection.
 *
 * The library really ought to save up the CLOSEWAIT state change until
 * the port has been emptied.
 */
    if (connp->CNState == CLOSED)
	if (capac(fd, capbuf, sizeof(capbuf)) == -1 || capbuf[0] == 0)
	    netabort(connp, 1, 0);
    return(0);
}

/* -------------------------- S E N D U R G ------------------------- */
/*
 * SendUrg(connp) marks the outgoing data as urgent. It should be called AFTER
 * the appropriate op (e.g. IP) has been sent. All the data in the ToNetBuf
 * is marked as urgent.
 */
SendUrg(connp)
struct NetConn *connp;
{

    connp->OutSynCnt = Clen(connp->ToNetBuf);
}

/* -------------------------- B E G I N S Y N C H ------------------- */
/*
 * BeginSynch() is called at the start of urgent data. It flushes pending
 * user data and notes that a datamark will be
 * along. Until it comes along, user-directed data should be flushed.
 * It also prints a bell on the error output.
 */
BeginSynch(connp)
    struct NetConn *connp;
{

    connp->InSynCnt = 1;
    Cinit(connp->ToUsrBuf);
    write(2, "\007", 1);
}

/* -------------------------- L L C L O S E ----------------------- */
/*
 * Close a TCP connection. All we do here is call TcpClose; eventually the
 * change in status will be seen by the CmdNet handler, which will do the actual
 * cleanup.
 */
llclose(connp)
    struct NetConn *connp;
{
    struct UserTcb *utcbp;
    int error;

    utcbp = connp->UtcbP;

    if (error = TcpClose(utcbp, 10))
        com_err(0, "%d TcpClose failed. %s", utcbp->ConnIndex, TcpErr(error));
    else
        fdprintf(2, "Closing...\r\n");
}

/* -------------------------- N E T A B O R T ----------------------- */
/*
 * Abort a TCP connection. Call TcpAbort, free up allocated resources.
 * If ExitOnClose, exit.
 */
netabort(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int error, num;
    int exsw;
    struct NetConn *connp;
    struct UserTcb *utcbp;
    extern struct NetConn *NetConP;
#ifdef LOGGING
    extern int logging;
#endif

    if (arg != 0)
	connp = arg;
    else
	connp = NetConP;
    utcbp = connp->UtcbP;

    if (argc == 0)
    {
	fdprintf(2, "Abort the current network connection\r\n");
	return;
    }
    if (argc != 1)
    {
	fdprintf(2, "netabort takes no arguments\r\n");
	return;
    }
    if (connp == 0)    /* Connection we're supposed to close doesn't exist */
    {
	fdprintf(2, "No connection\r\n");
	return;
    }
    num = utcbp->ConnIndex;
    if (error = TcpAbort(utcbp, 10))
	com_err(0, "TcpAbort(%d) failed. %s", num, TcpErr(error));
    else if (arg == 0)	/* User command, print message */
	fdprintf(2, "Aborted\r\n");
    connp->UtcbP = 0;	      /* So no one tries to use it (esp. CmdNet) */
    if (connp->StateP)
    {
	IoDelete(utcbp->SendFds);
	IoDelete(utcbp->RecvFds);
	if (connp == NetConP)
	    ChgMode(OrigMode());
	FreeSB(connp->StateP);
	connp->StateP = 0;
    }
    if (connp == NetConP)  /* Current connection -- zero it */
	NetConP = 0;
    exsw = connp->ExitOnClose;
    ChgConn('d', connp);
    free(connp);
    signal(2, 0);	     /* Interrupts kill once again */
    if (exsw)
    {
#ifdef LOGGING
	if (logging)
	    LogEnd();
#endif
	exit(0);
    }
    return;
}

/* -------------------------- Q U I T ------------------------------- */
/*
 * Quit. For TCP, this means closing the current connection and remembering
 * to exit when the red tape is done. If the first argument is nonzero, abort
 * instead.
 */
quit(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    extern struct NetConn *NetConP;
    extern int logging;

    if (argc == 0)
	fdprintf(2, "Close connection and return to command level\r\n");
    else if (argc != 1)
	fdprintf(2, "I take no arguments\r\n");
    else if (NetConP)
    {
	NetConP->ExitOnClose = 1;
	if (arg)
	    netabort(0, 1, 0);
	else
	    netclose(0, 1, 0);
    }
    else
    {
#ifdef LOGGING
	if (logging)
	    LogEnd();
#endif
	exit(0);
    }
}

/* -------------------------- S E T S E C U R ----------------------- */
/*
 * SetSecur() is called from the command processor to process the "security"
 * command. It just sets SendSecur to the specified value.
 */
SetSecur(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int temp;
    extern struct NetConn *NetConP;
    extern char *atoiv();

    if (argc == 0)
    {
        fdprintf(2, "Set security level to the specified numeric value\r\n");
        return;
    }
    if (argc != 2)
    {
        fdprintf(2, "Usage: %s security-level\r\n", argv[0]);
        return;
    }
    if (*atoiv(argv[1], &temp))
    {
        fdprintf(2, "%s: security level non-numeric\r\n", argv[0]);
        return;
    }
    NetConP->SendSecur = temp;
    return;
}

/* ----------------------- M O V E ----------------------------- */
/*
 * move() implements the ^move command. It just calls TcpMove.
 */
move(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int pid;
    int error;
    extern struct NetConn *NetConP;
    char *atoiv();

    if (argc == 0) /* Help */
    {
        fdprintf(2, "Move current connection to specified process\r\n");
        return;
    }
    if (argc != 2) /* Usage */
    {
        fdprintf(2, "Usage: %s process-id\r\n", argv[0]);
        return;
    }
    if (*atoiv(argv[1], &pid))
    {
        fdprintf(2, "%s: Non-numeric process-id\r\n", argv[0]);
        return;
    }
    if (NetConP == 0)
    {
        fdprintf(2, "%s: No connection to move.\r\n", argv[0]);
        return;
    }

    if (error = TcpMove(NetConP->UtcbP, pid, 10))
        com_err(0, "TcpMove(%d) failed. %s", NetConP->UtcbP->ConnIndex, TcpErr(error));

    return;
}

/* -------------------------- S T A T U S --------------------------- */
/*
 * Call TcpStatus on the current connection. Print out the info in a nice
 * format.
 */
Status(junk, argc, argv)
    int junk;
    int argc;
    char *argv[];
{
    int i;
    int error;
    struct ConnStat cs;
    extern struct NetConn *NetConP;

    if (argc == 0)
        fdprintf(2, "Print status of current connection\r\n");
    else if (argc != 1)
        fdprintf(2, "%s: No arguments expected.\r\n", argv[0]);
    else if (NetConP == 0)
        fdprintf(2, "%s: No current connection.\r\n", argv[0]);
    else if (error = TcpStatus(NetConP->UtcbP, &cs, 10))
        com_err(0, "%s: TcpStatus failed. %s.", argv[0], TcpErr(error));
    else
    {
        fdprintf(2, "State of connection: %d\r\n", cs.CState);
        fdprintf(2, "Foreign net: 0%o\r\n", cs.CNet&0377);
        fdprintf(2, "Foreign host: 0%o\r\n", cs.CHost&0377);
        fdprintf(2, "Foreign imp: 0%o\r\n", cs.CImp);
        fdprintf(2, "Local port: %d\r\n", cs.CLocPrt);
        fdprintf(2, "Foreign port: %d\r\n", cs.CForPrt);
        fdprintf(2, "Max security to net: %d\r\n", cs.CScMxOt&0377);
        fdprintf(2, "Min security to net: %d\r\n", cs.CScMnOt&0377);
        fdprintf(2, "Max security from net: %d\r\n", cs.CScMxIn&0377);
        fdprintf(2, "Min security from net: %d\r\n", cs.CScMnIn&0377);
        fdprintf(2, "Send precedence: %d\r\n", cs.CSndPrec&0377);
        fdprintf(2, "Recv precedence: %d\r\n", cs.CRcvPrec&0377);
        for (i = 0; i < cs.CNTcc; i++)
            fdprintf(2, "TCC[%d]: %d\r\n", i, cs.CTcc[i]);
        fdprintf(2, "Send window: %d\r\n", cs.CSndWind);
        fdprintf(2, "Receive window: %d\r\n", cs.CRcvWind);
        fdprintf(2, "# bytes awaiting ack: %d\r\n", cs.CAckData);
        fdprintf(2, "Data in send buffer: %d\r\n", cs.CSndBuff);
        fdprintf(2, "Data in receive buffer: %d\r\n", cs.CRecBuff);
        fdprintf(2, "# of segment received: %d\r\n", cs.CSegRecv);
        fdprintf(2, "# segments received with dup data: %d\r\n", cs.CDupSeg);
        fdprintf(2, "# discarded segments: %d\r\n", cs.CBusyRecv);
        fdprintf(2, "# retransmitted segments: %d\r\n", cs.CRetran);
    }
}

/* -------------------------- S T O P G O ------------------------------- */
/*
 * StopGo() expects either a 0 (stop) or 1 (resume) as its arg. The supplied
 * text arguments should be either -tcc <tccnum> or a host/net specifier
 * as in the netopen call.
 */
StopGo(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int i;
    int tcc;
    int error;
    int type;
    struct
    {
        int l_high;
        int l_low;
    };
    long host;
    long id;
    extern char *(atoiv());
    extern long gethost();

    if (argc == 0)
    {
        if (arg == 0)
            fdprintf(2, "Stop communications with specified host or tcc\r\n");
        else
            fdprintf(2, "Resume communications with specified host or tcc\r\n");
        return;
    }
    if (argc == 1)
    {
        fdprintf(2, "Usage:\t%s -tcc <tcc>\r\n\t%s host\r\n",
                        argv[0], argv[0]);
        return;
    }
    argc--;
    tcc = -1;
    host = 0;
    for (i = 1; i <= argc; i++)
    {
        if (seq(argv[i], "-tcc"))
        {
            if (i == argc || *atoiv(argv[++i], &tcc) || tcc < 0)
            {
                fdprintf(2, "%s: Bad or missing TCC.\r\n", argv[0]);
                return;
            }
            id.l_high = 0;
            id.l_low = tcc;
            type = 2;
        }
        else
        {
            host = gethost(argv[++i]);
            if (host <= 0)
            {
                fdprintf(2, "%s: Bad host specifier: \"%s\"\r\n", argv[0], argv[i]);
                return;
            }
            id = host;
            type = 1;
        }
    }

    if (tcc != -1 && host != 0)
    {
        fdprintf(2, "%s: Cannot specify both host and TCC.\r\n", argv[0]);
        return;
    }

    if (arg == 0)
        error = TcpStop(type, &id, 10);
    else
        error = TcpResume(type, &id, 10);

    if (error)
        fdprintf(2, "%s: %s failed. %s.\r\n",
                        argv[0], arg==0? "TcpStop" : "TcpResume", TcpErr(error));
}

/* -------------------------- G L O B A L S ------------------------- */
/*
 * Globals used within this file
 */
#ifdef LOGGING
    int logging 0;  /* Flag set by -log argument to netopen. */
#endif
