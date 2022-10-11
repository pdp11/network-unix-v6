/*
 * Command processor, command table, and a lot of commands
 * Sometimes a command only exists to exercise a particular
 * subroutine. To aid in distinguishing, all command names
 * begin with Z.
 */
#include "stdio.h"
#include "signal.h"
#include "errno.h"
#ifdef BBN_VAX_UNIX
#	define VFORK vfork
#	define VEXIT _exit
#endif
#ifndef BBN_VAX_UNIX
#	define VFORK fork
#	define VEXIT exit
#endif
#include "tnio.h"
#include "telnet.h"
#ifdef NCP
#include "ncpopen.h"
#endif
#include "globdefs.h"
#include "ttyctl.h"
#ifdef TCP
#include "netlib.h"
#include "con.h"
#endif TCP

extern int termfd;
extern int errno;

struct ComEntry
{
    char *ComName;
    int (*ComFunc)();
    int ComInt;
    char *ComStr;
};

/*
 * Each ComEntry is called with four arguments. Normally, they are:
 *
 * A parameter: either an integer or a character string. The structure
 *	must have both because there is no way to statically initialize a
 *	union. It is assumed that the command takes a character string
 *	if ComStr is non-NULL; if it is NULL, the ComInt is supplied
 *	instead.
 *
 * The number of arguments that appeared on the command line -- as with
 *     UNIX main programs, this is at least 1;
 * A pointer to an array of pointers to arguments, as with UNIX main programs.
 *
 * Each function may also be called upon to explain what it does. In this call,
 *     the second argument will be 0 and the third argument will be the name
 *     of the entry.
 * The command table itself is at the end of the program to avoid the need
 *     to declare all the entries.
 */

/* -------------------------- C A L L C M D ------------------------- */
/*
 * Splits the line up into words separated by tabs or blanks. Looks up
 * the first word on the line in the command table (above), and calls the
 * indicated function as explained above. If the word is not found, it is
 * an error.
 */
callcmd(line)
    char *line;
{
    register char *linep, *eol;
    int linelen;
    char **argp;
    struct ComEntry *comp;
    int outside_arg;
    static char *arglst[100];
    extern struct ComEntry ComTab[];
    extern struct ComEntry * getcom();

    linelen = strlen(line);
    eol = &line[linelen-1]; /* Point to end */

/* Clear MSB of each byte, in case binary mode is on. */

    for (linep = line; linep <= eol; linep++)
	*linep &= 0177;

/* Make arg array point to beginning of each argument. */
/* Null-terminate it. */

    argp = arglst;
    outside_arg = 1;
    for (linep = line; linep <= eol; linep++)
	switch(*linep)
	{
	    case ' ':
	    case '\t':
		outside_arg++;
		*linep = 0;
		break;
	    default:
		if(outside_arg)
		{
		    *argp++ = linep;
		    outside_arg = 0;
		}
		break;
	}

/* Execute it */

    if (argp >= &arglst[sizeof(arglst) - 1])
	fprintf(stderr, "con: too many args. Max is %d.\r\n",
			sizeof(arglst) / sizeof(arglst[0]));
    else if (argp > arglst) /* At least a command name */
    {
	*argp = 0; /* To indicate last arg */
	if ((comp = getcom(arglst[0], ComTab)))
	{
	    arglst[0] = comp->ComName;	/* Give full name */
	    if (comp->ComStr != NULL)
		(*(comp->ComFunc))(comp->ComStr, argp - arglst, arglst);
	    else
		(*(comp->ComFunc))(comp->ComInt, argp - arglst, arglst);
	}
	else
	    cmderr(0, "Unrecognized command: %s\n", arglst[0]);
    }
}

/* -------------------------- G E T C O M --------------------------- */
/*
 * getcom(strp, comtab) takes a pointer to a command name and looks it up
 * in comtab, returning a pointer to its entry there.
 */
struct ComEntry *getcom(strp, comtabp)
char *strp;
struct ComEntry comtabp[];
{
    struct ComEntry *cp;            /* pointer to com table entry    */
    struct ComEntry *candidate;  /* ptr to candidate command      */
    register int nmatch;             /* num chrs matched on this try */
    register int bestmatch;         /* best match found yet */

    if (strp == 0)
        return(0);                /* can't match null string */

    candidate = 0;
    bestmatch = 0;
    for (cp = comtabp; cp->ComFunc; cp++)     /* linear srch */
    {
        if((nmatch = compar(strp, cp->ComName)) != 0)
            if (nmatch == -1)
                return(cp);     /* take exact match      */
            else if (nmatch > bestmatch)
            {
                candidate = cp;
                bestmatch = nmatch;
            }
            else
            {         /* had two candidates that matched same */
		fprintf(stderr, "Not unique\r\n");
                return(0);
            }
    }
    return(candidate);
}

/* -------------------------- C O M P A R --------------------------- */
/*
 * compar(s1, s2) compares s1 against s2. Returns -1 if they are equal.
 * Otherwise, if s1 is a prefix of s2, returns # chars in common.
 * Otherwise returns 0.
 */
compar(s1, s2)
char *s1;
char *s2;
{
    register char *sp1 ,*sp2;

    sp1 = s1;
    sp2 = s2;
    while (*sp1++ == *sp2++)
        if (*(sp1-1) == 0)
            return(-1);     /* Exact match */

    /* Strings differ. If first one ran out, return # chars matched. */
    if (*(sp1-1) == 0)
        return(sp1-s1-1);

    /* Not a match at all... */
    return(0);
}

/* -------------------------- Z N E T O P E N ----------------------- */
/*
 * Open a TELNET connection. Set NetConP to the connection pointer if
 * successful.
 */
znetopen(arg, argc, argv)
    int arg;
    int argc;
    char * argv[];
{
    NETCONN * connp = NULL;
    TTYMODE *modec = NULL;
    TTYMODE *modep = NULL;
    struct tchars *tp;
    int fd;
#ifdef NCP
    struct ncpopen ncpopen;
#endif
#ifdef TCP
    struct con tcpopen;
#endif
    extern int debug;
    extern int needprompt;
    extern NETCONN * NetConP;
    extern TTYMODE *NetModeP;
    extern NETCONN * telopen();
    extern NETCONN * telinit();
    extern TTYMODE * AllocMode();
    extern int telfd();
    extern int sendint();
    extern int from_net();
    extern int net2user();
    extern myecf(), myelf(), myaof(), mybreakf(), mygaf(), mysynchf();

    if (setexit())
    {
	if (modep != NULL)
	    FreeMode(modep);
	if (connp != NULL)
	    telfinish(connp);
	return;
    }

    if (argc == 0)
    {
	fprintf(stderr, "Open a connection to the specified host.\r\n");
	return;
    }

    if (NetConP != NULL)
	rcmderr(0, "Connection already open.\n");

    if (ArgFlag("-debug", argv))
	debug = 1;
    argc = ArgCount(argv);
    fprintf(stderr, "Trying...\r\n");
#   ifdef NCP
	argparse(argc, argv, &ncpopen);
	fd = open("/dev/net/ncp", &ncpopen);
	if (debug)
	 {
	    printf(" Host is 0x%X:", ncpopen.o_host);
	    printf("\nNCP structure:\n o_op = %d\n o_type = 0%o\n",
	     ncpopen.o__op, ncpopen.o__type);
	    printf(" o__id = %d\n o_lskt = 0%o\n o_fskt = 0%O\n",
	     ncpopen.o__id, ncpopen.o_lskt, ncpopen.o_fskt);
	    printf(" o__ohost = %d\n o_bysize = %d\n o_nmal = %d\n",
	     ncpopen.o__frnhost, ncpopen.o_bsize, ncpopen.o_nomall);
	    printf(" o__timo = %d\n o_relid = %d\n o_host = 0%O\n",
	     ncpopen.o_timeo, ncpopen.o_relid, ncpopen.o_host);
	  }
#   endif NCP
#   ifdef TCP
	argparse(argc, argv, &tcpopen);
	if ((fd = open("/dev/net/tcp",&tcpopen)) >= 0)
	    ioctl(fd, NETSETE, NULL); /* urgent assumed off, eols on. */
#   endif TCP

    if (fd == -1) 
         rcmderr(-1, "Cannot connect to foreign host.\r\n");

    fprintf(stderr, "Open\r\n");
    connp = telinit(fd);
    if (connp == NULL)
	rcmderr(-1, "Cannot get connection block.\n");
    modec = AllocMode();
    if (modec == NULL)
	rcmderr(-1, "Cannot get mode block.\n");
    NetConP = connp;
    NetModeP = modec;
    ChgMode(modec);
    SetFlags(modec, CRMOD, 0);
    modep = AllocMode();
    tp = &(modep->tm_tchars);
    tp->t_brkc = '\r';
    modep->tm_local = modec->tm_local&(~LCTLECH);
    ChgMode(modep);
    if (debug)
    {
	extern int verbose;
	telfunc(connp, EC, myecf);
	telfunc(connp, EL, myelf);
	telfunc(connp, AO, myaof);
	telfunc(connp, BREAK, mybreakf);
	telfunc(connp, GA, mygaf);
	telfunc(connp, SYNCH, mysynchf);
	verbose = 1;
    }
#ifdef AWAIT
    IoEnter(telfd(connp), from_net, 0, connp);
    IoxEnter(net2user, connp);
#endif
#ifndef AWAIT
    {
	extern int other_pid;
	extern int par_uid;

	other_pid = fork();
	if (other_pid == 0)
	{
	    signal(SIGINT, SIG_IGN);
	    signal(SIGQUIT, SIG_IGN);
	    other_pid = par_uid;
	    from_net(1, connp); /* Does not return */
	    abort();		/* If it returns, I want to know why */
	}
    }
#endif
    needprompt = 0;
    signal(SIGINT, sendint);
}

/* -------------------------- A R G P A R S E ----------------------- */
/*
 * Parse open command arguments. Put result in ncpopen structure.
 */
argparse(argc, argv, op)
    int argc;
    char * argv[];
#   ifdef NCP
	register struct ncpopen * op;
#   endif NCP
#   ifdef TCP
	register struct con * op;
#   endif TCP
{
    extern int debug;
    int host_arg_index;
    int i;
    extern int fsocket;
    char *logfile;
    extern int telwtlog, telrdlog;
    extern int ArgInt(), ArgFlag(), ArgCount();
    extern long ArgLong();
    extern char * ArgStr();

    if (argc == 1)
	rcmderr(0,
"Usage: %s [-ls #] [-fs #] [-t #] [-server] [-direct] [-init] [-specific] [host]\n",
	argv[0]);


/* Defaults */
#ifdef NCP
    op->o_type = 0;
    op->o_nomall = 0;
    op->o_host = 0L;

    if (ArgFlag("-server", argv))
    {
	op->o_type |= SERVER;
	op->o_lskt = SERVER_SOCKET;
    }
    op->o_lskt = ArgInt("-ls", 0, argv);
    op->o_fskt = ArgLong("-fs", (long)fsocket, argv);
    op->o_timeo = ArgInt("-t", 0, argv);
    op->o_relid = ArgInt("-fd", 0, argv); /* Useless right now */
    logfile = ArgStr("-log", NULL, argv);
    if (logfile)
    {
	telrdlog = telwtlog = creat(logfile, fmodes(logfile));
	if (telrdlog == -1)
	    cmderr(-1, "Log file \"%s\" cannot be created.\n", logfile);
    }
    if (ArgFlag("-direct", argv))
	op->o_type |= DIRECT;
    if (ArgFlag("-init", argv))
	op->o_type |= INIT;
    if (ArgFlag("-specific", argv))
	op->o_type |= SPECIFIC;
    if (ArgFlag("-duplex", argv))
	op->o_type |= DUPLEX;
    if (ArgFlag("-relative", argv)) /* Useless right now */
	op->o_type |= RELATIVE;
#endif NCP
#ifdef TCP
    op->c_mode = CONTCP | CONACT | ((debug)?CONDEBUG:0);
    op->c_sbufs = op->c_rbufs = 2;
    op->c_lo = op->c_hi = 0;

    if (ArgFlag("-server", argv))
    {
	op->c_mode &= ~CONACT;
	op->c_lport = SERVER_SOCKET;
	op->c_rbufs = 1;
    } else op->c_sbufs = 1;

    op->c_lport = ArgInt("-lp", 0, argv);
    op->c_fport = ArgInt("-fp", fsocket, argv);
    op->c_timeo = ArgInt("-t", 30, argv);
    logfile = ArgStr("-log", NULL, argv);
    if (logfile)
    {
	telrdlog = telwtlog = creat(logfile, fmodes(logfile));
	if (telrdlog == -1)
	    cmderr(-1, "Log file \"%s\" cannot be created.\n", logfile);
    }
/*  if (ArgFlag("-direct", argv))       /* what is all this stuff anyway???
	op->c_type |= DIRECT;
    if (ArgFlag("-init", argv))
	op->c_type |= INIT;
    if (ArgFlag("-specific", argv))
	op->c_type |= SPECIFIC;
    if (ArgFlag("-duplex", argv))
	op->c_type |= DUPLEX;*/
/*    if (ArgFlag("-relative", argv)) /* Useless right now */
/*	op->c_type |= RELATIVE;	*/
#endif TCP
    argc = ArgCount(argv);
    host_arg_index = 0;
    for (i = 1; i < argc; i++)
	if (argv[i][0] == '-')
	    rcmderr(0, "Unknown option: \"%s\".\n", argv[i]);
	else if (host_arg_index == 0)
	    host_arg_index = i;
	else
	    rcmderr(0, "Superfluous argument: \"%s\".\n", argv[i]);

    if (host_arg_index != 0)
    {
#   ifdef NCP
	op->o_host = gethost(argv[host_arg_index]);
	if (op->o_host == -1L)
#   endif
#   ifdef TCP
	op->c_fcon = gethost(argv[host_arg_index]);
	mkanyhost(op->c_lcon);
	if (isbadhost(op->c_fcon))
#   endif
	    rcmderr(0, "Unknown host specifier: \"%s\".\n", argv[host_arg_index]);
    }
#   ifdef NCP
      if (op->o_type == 0 && op->o_host == 0L)
#   endif NCP
#   ifdef TCP
	if (isanyhost(op->c_fcon))
#   endif TCP
	rcmderr(0, "No host specified.\n");
    return;
}

/* -------------------------- Z V E R B O S E ------------------------- */
/*
 * Turns the verbose flag on or off.
 */
zverbose(val, argc, argv)
    char val;
    int argc;
    char *argv[];
{
    extern int verbose;

    if (argc == 0)
    {
        if (val == 'v')
	    fprintf(stderr, "Announce all subsequent option negotiation\r\n");
        else
	    fprintf(stderr, "Announce only important option negotiations (e.g. 'Remote echo')\r\n");
        return;
    }
    else if (argc != 1)
    {
	fprintf(stderr, "I take no arguments\r\n");
        return;
    }
    switch(val)
    {
        case 'v':
            verbose = 1;
	    fprintf(stderr, "Verbose mode\r\n");
            break;
        case 'b':
            verbose = 0;
	    fprintf(stderr, "Brief mode\r\r\n");
            break;
        default:
	    fprintf(stderr, "vswitch(%c)?\r\n", val);
            break;
    }
    return;
}

/* -------------------------- Z S E N D C T L ----------------------- */

zsendctl(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    extern NETCONN *NetConP;

    if (argc > 1)
    {
	fprintf(stderr, "I take no arguments\r\n");
	return;
    }

    if (argc == 0)
	switch(arg)
	{
	    case AO:
		fprintf(stderr, "Abort output (send an Abort-Output and Synch)\r\n");
		return;
	    case BREAK:
		fprintf(stderr, "Break; that is, send a Break and Synch\r\n");
		return;
	    case IP:
		fprintf(stderr, "Interrupt process (send IP and Synch)\r\n");
		return;
	    case AYT:
		fprintf(stderr, "Ask foreign host if it is still alive (send AYT)\r\n");
		return;
	    case EC:
		fprintf(stderr, "Erase last character (send EC)\r\n");
		return;
	    case EL:
		fprintf(stderr, "Erase to beginning of line (send EL)\r\n");
		return;
	    case SYNCH:
		fprintf(stderr, "Send a Synch\r\n");
		return;
	    case GA:
		fprintf(stderr, "Send a Go-Ahead\r\n");
		return;
	}

    if (NetConP == 0)
    {
	fprintf(stderr, "No active connection\r\n");
	return;
    }
    sendctl(NetConP, arg);
}

/* -------------------------- Z H E L P ----------------------------- */

zhelp(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int i;
    extern struct ComEntry ComTab[];

    if (argc == 1)     /* Give each command and have it show-and-tell */
    {
	fprintf(stderr, "TELNET Commands\r\n");
	fprintf(stderr,
"Only enough of each command name to uniquely identify it need be typed.\r\n\n");
	for (i = 0; ComTab[i].ComFunc; i++)
	{
	    fprintf(stderr, "%-11s", ComTab[i].ComName);
	    if (ComTab[i].ComStr != NULL)
		(*(ComTab[i].ComFunc))(ComTab[i].ComStr, 0, &ComTab[i].ComName);
	    else
		(*(ComTab[i].ComFunc))(ComTab[i].ComInt, 0, &ComTab[i].ComName);
	}
    }
    else if (argc == 0)
	fprintf(stderr, "Briefly explain each command.\r\n");
    else
    {
	fprintf(stderr, "Sorry, individual explanations not implemented yet\r\n");
	fprintf(stderr, "Type 'help' for a list of all commands and their purposes\r\n");
    }
    return;
}

/* -------------------------- Z S H E L L --------------------------- */
/*
 * Execute the rest of the line as a shell command.
 * If there were no arguments, invoke a subshell.
 */

zshell(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int (*oldint)();
    int (*oldquit)();
    int junk;
    register char *p;
    char *shellcmd;
    TTYMODE *oldmode;
    int subsh;
    extern int errno;
    extern int escape;
    extern int inputfd;
    extern TTYMODE *ChgMode();
    extern TTYMODE *OrigMode();
    extern char *getenv();
    int (*signal())();

/*
 * The arguments to this command (argv[1] on) are joined back together
 * and fed to the shell. Joining back together is, of course, klugy...
 */
    if (argc <= 0)
	fprintf(stderr, "Pass the rest of the line to the shell for execution\r\n");
    else
    {
	if (argc == 1)
	    subsh = 1;
	else
	    subsh = 0;
	argc--; /* Now it equals the number of args */
	for (p = argv[1]; --argc > 0;) /* Loop (argc minus 1) times */
	{
	    while(*++p)     /* Get to the null... */
		;
	    *p = ' ';	     /* Replace with blank */
	}
	oldmode = ChgMode(OrigMode());
	if ((shellcmd = getenv("SHELL")) == NULL)
		shellcmd = "/bin/sh";
	switch(VFORK())
	{
	    case -1:
		fprintf(stderr, "No processes!\r\n");
		break;
	    case 0:
		oldint = signal(SIGINT, SIG_DFL);
		oldquit = signal(SIGQUIT, SIG_DFL);
		if (inputfd != 0)
		{
		    close(0);
		    dup(inputfd);
		}
		if (termfd != 1)
		{
		    close(1);
		    dup(termfd);
		}
		close(2);
		dup(1);
		if (subsh)
		    execl(shellcmd, SHELL_0, 0);
		else
		    execl(shellcmd, SHELL_0, "-c", argv[1], 0);
		cmderr(errno, "Attempting to execute %s", shellcmd);
		VEXIT(0);	/* Child process */
	    default:
		oldint = signal(SIGINT, SIG_IGN);
		oldquit = signal(SIGQUIT, SIG_IGN);
		wait(&junk);
		signal(SIGINT, oldint);
		signal(SIGQUIT, oldquit);
		fprintf(stderr, "%c\n", escape);
		ChgMode(oldmode);
		break;
	}
    }
}

/* -------------------------- Z E X E C ----------------------------- */
/*
 * Invoke the command whose name is "arg". This direct execution
 * does not change the tty modes to "local". Hence any program
 * invoked by this command must put out "\r\n" to end each line,
 * rather than just "\n", so that if CRMOD has been cleared the command's
 * output will still print properly. (The change of "\n"s to "\r\n"s is
 * all that distinguishes /etc/nettty from /bin/stty.)
 */
zexec(arg, argc, argv)
    char *arg;
    int argc;
    char *argv[];
{
    int junk;
    extern int errno;
    extern char *errmsg();
    extern int escape;

    if (argc == 0)
	fprintf(stderr, "Invoke the UNIX %s command\r\n", argv[0]);
    else
    {
	switch(VFORK())
	{
	    case -1:
		fprintf(stderr, "No processes!\r\n");
		break;
	    case 0:
		if (termfd != 1)
		{
		    close(1);
		    dup(termfd);
		}
		close(2);
		dup(1);
		execvp(arg, argv);
		cmderr(errno, "Cannot execute %s\r\n", arg);
		VEXIT(0);
	    default:
		wait(&junk);
		fprintf(stderr, "%c\r\n", escape);
		break;
	}
    }
}

/* -------------------------- Z N E T C L O S E --------------------- */
/*
 * Close a network connection.
 */
znetclose(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    extern NETCONN *NetConP;
    extern int udone;

    if (argc == 0)
	fprintf(stderr, "Close the current network connection\r\n");
    else if (argc != 1)
	fprintf(stderr, "%s takes no arguments\r\n", argv[0]);
    else if (NetConP == 0)    /* Connection we're supposed to close doesn't exist */
	return;
    else
	udone++;
}

/* -------------------------- Z S E N D O P T ----------------------- */
/*
 * Start the negotiation process for the requested option.
 */
zsendopt(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int optno;
    extern NETCONN *NetConP;

    if (argc == 0)
    {
	fprintf(stderr, "Try to negotiate the specified option.\r\n");
	return;
    }

    if (setexit())
	return;

    if (argc != 2)
    {
	int i, nopts;

	fprintf(stderr, "Usage: %s option\r\n", argv[0]);
	fprintf(stderr, "Available options are:");
	nopts = 0;
	for (i = 0; i < 255; i++)   /* All possible option numbers */
	{
	    extern char *optname();
	    char * name = optname(i);

	    if (name != NULL)
	    {
		if (nopts % 3 == 0)
		    fprintf(stderr, "\r\n\t  ");
		fprintf(stderr, "%-20s", name);
		nopts++;
	    }
	}
	fprintf(stderr, "\r\n");
	return;
    }

    if (NetConP == NULL)
    {
	fprintf(stderr, "No active connection\r\n");
	return;
    }

    optno = optnum(argv[1]);
    if (optno == -1)
	rcmderr(0, "\"%s\" ambiguous.\r\n", argv[1]);
    if (optno == -2)
	rcmderr(0, "\"%s\" not recognized.\r\n", argv[1]);

    if (sendopt(NetConP, arg, optno))
	rcmderr(0, "Could not request option \"%s\".\r\n", argv[1]);
    return;
}

/* -------------------------- Z O P T I O N ------------------------- */
/*
 * Accept or refuse the given option.
 */
zoption(arg, argc, argv)
    int arg;	/* 0 refuse, nonzero accept */
    int argc;
    char * argv[];
{
    int optno;
    extern int optnum(), option();

    if (argc == 0)
    {
	if (arg)
	    fprintf(stderr, "Accept the specified option.\r\n");
	else
	    fprintf(stderr, "Refuse the specified option.\r\n");
	return;
    }
    if (setexit())
	return;

    if (argc == 2)
    {
	optno = optnum(argv[1]);
	if (optno == -2)
	    rcmderr(0, "Option \"%s\" not recognized.\r\n", argv[1]);
	if (optno == -1)
	    rcmderr(0, "Option \"%s\" ambiguous.\r\n", argv[1]);
	if (option(arg, optno))
	    rcmderr(0, "Cannot enter option.\r\n");
    }
    else
	rcmderr(0, "Usage: %s option\r\n", argv[0]);
    return;
}

/* -------------------------- Z S E N D S U B ----------------------- */
/*
 * Send a subnegotiation.
 */
zsendsub(arg, argc, argv)
    int arg;
    int argc;
    char * argv[];
{
    int i;
    char buffer[64];
    int optno;
    extern char * atoiv();
    extern NETCONN * NetConP;

    if (argc == 0)
    {
	fprintf(stderr, "Send the specified subnegotiation sequence.\n");
	return;
    }
    if (argc == 1)
    {
	fprintf(stderr, "Usage: %s optnum [ byte ] ...\n", argv[0]);
	return;
    }
    if (setexit())
	return;
    optno = optnum(argv[1]);
    if (optno == -1)
	rcmderr(0, "\"%s\" ambiguous.\r\n", argv[1]);
    if (optno == -2)
	rcmderr(0, "\"%s\" not recognized.\r\n", argv[1]);
    for (i = 2; i < argc; i++)
    {
	int value;

	if (*atoiv(argv[i], &value) != '\0')
	    rcmderr(0, "\"%s\" is not numeric.\r\n", argv[i]);
	buffer[i-2] = value;
    }
    sendsub(NetConP, optno, buffer, i-2);
}

/* -------------------------- Z E X I T ----------------------------- */
/*
 * Exit from telnet without waiting for connection to stabilize.
 */
zexit(arg, argc, argv)
    int arg;
    int argc;
    char * argv[];
{
    extern int xdone;

    if (argc == 0)
    {
	fprintf(stderr, "Exit from TELNET immediately.\r\n");
	return;
    }
    xdone = 1;
    check_done();
}

/* -------------------------- Z S E T ------------------------------- */
/*
 * Sets the variable to the character described.
 */
zset(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    register char * ap;
    register int * charp;
    extern int escape;

    if (argc == 0)
    {
	fprintf(stderr, "Set special characters.\r\n");
	return;
    }
    if (argc != 3)
    {
	fprintf(stderr, "Usage: %s <function> { <char> | off }\r\n", argv[0]);
	fprintf(stderr, " where <function> is currently only 'escape' ('esc')\r\n");
	return;
    }
    ap = argv[1];
    if ((strcmp(ap, "escape") == 0) || (strcmp(ap, "esc") == 0))
	charp = &escape;
    else
    {
	fprintf(stderr, "No such function: \"%s\".\r\n", ap);
	return;
    }
    ap = argv[2];
    if (ap[1] == '\0')
	*charp = ap[0] & 0377;
    else if (strcmp(ap, "off") == 0)
	*charp = -1;
    else
    {
	fprintf(stderr, "Sorry, only a literal character or 'off' may be specified.\r\n");
	return;
    }
}

/* -------------------------- C O M T A B --------------------------- */

extern zinput();

struct ComEntry ComTab[] =
{
    "help",	    zhelp,	0,	NULL,
    "?",	    zhelp,	0,	NULL,
    "verbose",	    zverbose,	'v',	NULL,
    "brief",	    zverbose,	'b',	NULL,
    "ip",	    zsendctl,	IP,	NULL,
    "ao",	    zsendctl,	AO,	NULL,
    "break",	    zsendctl,	BREAK,	NULL,
    "ec",	    zsendctl,	EC,	NULL,
    "el",	    zsendctl,	EL,	NULL,
    "ayt",	    zsendctl,	AYT,	NULL,
    "synch",	    zsendctl,	SYNCH,	NULL,
    "ga",	    zsendctl,	GA,	NULL,
    "stty",	    zexec,	0,	"/bin/stty",
#ifdef MODTTY
    "modtty",	    zexec,	0,	"/bin/modtty",
#endif MODTTY
    "set",	    zset,	0,	NULL,
    "open",	    znetopen,	0,	NULL,
    "connect",	    znetopen,	0,	NULL,
#ifdef AWAIT
    "do",	    zsendopt,	TN_DO,	NULL,
    "dont",	    zsendopt,	TN_DONT,NULL,
    "will",	    zsendopt,	TN_WILL,NULL,
    "wont",	    zsendopt,	TN_WONT,NULL,
    "sub",	    zsendsub,	0,	NULL,
    "accept",	    zoption,	1,	NULL,
    "refuse",	    zoption,	0,	NULL,
#endif
    "close",	    znetclose,	0,	NULL, /* Clean close */
    "x",	    zshell,	0,	NULL,
    "quit",	    zexit,	0,	NULL,
#ifdef NOT_YET_IMPLEMENTED
    "input",	    zinput,	0,	NULL,
#endif
    0,		    0,		0,	NULL,
};

/* -------------------------- A N N O U N C E ----------------------- */
/*
 * Announce being called with given argument.
 */
announce(arg)
    char * arg;
{
    fprintf(stderr, "%s", arg);
}

/* -------------------------- M Y E C F ----------------------------- */
myecf() { announce("<EC>"); }
/* -------------------------- M Y E L F ----------------------------- */
myelf() { announce("<EL>"); }
/* -------------------------- M Y A O F ----------------------------- */
myaof() { announce("<AO>"); }
/* -------------------------- M Y I P F ----------------------------- */
mygaf() { announce("<GA>"); }
/* -------------------------- M Y B R E A K F ----------------------- */
mybreakf() { announce("<BREAK>"); }
/* -------------------------- M Y S Y N C H ------------------------- */
mysynchf() { announce("<SYNCH>"); }

/* ------------------------------------------------------------------ */
