#include "protocol.h"
#include "llpro.h"
#include "cbuf.h"
#include "netconn.h"

/*
 * Command processor, command table, and a lot of commands
 * Other commands are scattered throughout THP/TELNET
 */

extern	help(), vswitch(), quit(), netsend(), exprog(), netopen(), netclose(),
	ChgConn(), option(), set(), execsh(), forkoff(), redir();
#ifdef THP
extern netabort(), SetSecur(), move(), Status(), StopGo();
#endif

struct ComEntry
{
    char *ComName;
    int (*ComFunc)();
    int ComArg;
};

/*
 * Each function in ComTab is called with three arguments. Normally, they are:
 *
 * The value of the ComArg field (third column below), which is sufficient
 *     for most commands;
 * The number of arguments that appeared on the command line -- as with
 *     UNIX main programs, this is at least 1;
 * A pointer to an array of pointers to arguments, as with UNIX main programs.
 *
 * Each function may also be called upon to explain what it does. In this call,
 *     the second and third arguments will be 0.
 */

struct ComEntry ComTab[]
{
    "help",         &help,       0,
    "?",            &help,       0,
    "verbose",      &vswitch,    'v',
    "brief",        &vswitch,    'b',
    "quit",         &quit,       0,
    "ip",	    &netsend,	 IP,
    "ao",	    &netsend,	 AO,
    "break",	    &netsend,	 BREAK,
    "ec",	    &netsend,	 EC,
    "el",	    &netsend,	 EL,
    "ayt",	    &netsend,	 AYT,
    "synch",	    &netsend,	 DM,
    "stty",	    &exprog,	 "/etc/nettty",
    "modtty",	    &exprog,	 "/bin/modtty",
    "open",	    &netopen,	 0,
    "connect",	    &netopen,	 0,
    "close",	    &netclose,	 0,  /* Clean close */
    "switch",       &ChgConn,    'c',
    "list",         &ChgConn,    'l',
#ifdef TCP
    "abort",        &netabort,   0,  /* Aborting close */
    "security",     &SetSecur,   0,
    "move",         &move,       0,
    "status",       &Status,     0,
    "stop",         &StopGo,     0,
    "resume",       &StopGo,     1,
#endif
    "do",           &option,     DO,
    "dont",         &option,     DONT,
    "will",         &option,     WILL,
    "wont",         &option,     WONT,
    "set-escape",   &set,	 0,
    "x",            &execsh,     0,
    "fork",         &forkoff,    0,
    "input",        &redir,      0,
#ifdef THP
    "suspend",      &netsend,    SUSPEND,
    "continue",     &netsend,    CONTINUE,
#endif
    0,              0,           0,
};

/* -------------------------- E X E C C M D ------------------------- */
/*
 * Splits the line up into words separated by tabs or blanks. Looks up
 * the first word on the line in the command table (above), and calls the
 * indicated function as explained above. If the word is not found, it is
 * an error.
 */
ExecCmd(line, linelen)
char line[];
int linelen;
{
    register char *linep, *eol;
    char **argp;
    struct ComEntry *comp;
    int outside_arg;
    static char *arglst[100];
    extern struct ComEntry ComTab[];

    line[linelen] = 0;	      /* Terminate it */
    eol = &line[linelen-1]; /* Point to end */

/* Clear MSB of each byte, in case binary mode is on. */

    for (linep = line; linep <= eol; linep++)
	*linep =& 0177;

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
	fdprintf(2, "con: too many args. Max is %d.\r\n",
			sizeof(arglst) / sizeof(arglst[0]));
    else if (argp > arglst) /* At least a command name */
    {
	*argp = 0; /* To indicate last arg */
	if ((comp = getcom(arglst[0], ComTab)))
	{
	    arglst[0] = comp->ComName;	/* Give full name */
	    (*(comp->ComFunc))(comp->ComArg, argp - arglst, arglst);
	}
	else
	    fdprintf(2, "?\r\n");
    }
}

/* -------------------------- G E T C O M ---------------------- */
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
                fdprintf(2, "Not unique\r\n");
                return(0);
            }
    }
    return(candidate);
}

/* -------------------------- C O M P A R ---------------------------- */
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

/* -------------------------- V S W I T C H ------------------- */
/*
 * vswitch(setting...) sets the verbose flag on or off.
 */
vswitch(val, argc, argv)
    char val;
    int argc;
    char *argv[];
{
    extern int verbose;

    if (argc == 0)
    {
        if (val == 'v')
            fdprintf(2, "Announce all subsequent option negotiation\r\n");
        else
            fdprintf(2, "Announce only important option negotiations (e.g. 'Remote echo')\r\n");
        return;
    }
    else if (argc != 1)
    {
        fdprintf(2, "I take no arguments\r\n");
        return;
    }
    switch(val)
    {
        case 'v':
            verbose = 1;
            fdprintf(2, "Verbose mode\r\n");
            break;
        case 'b':
            verbose = 0;
            fdprintf(2, "Brief mode\r\r\n");
            break;
        default:
            fdprintf(2, "vswitch(%c)?\r\n", val);
            break;
    }
    return;
}

/* -------------------------- E C H O -------------------------- */

echo(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    if (argc == 0)
        fdprintf(2, "Echo its arguments (for debugging purposes)\r\n");
    else
    {
        fdprintf(2, "arg: %d\r\n", arg);
        for (; argc; argc--)
            fdprintf(2, "%s ", *argv++);
        fdprintf(2, "\r\n");
    }
}

/* -------------------------- N E T S E N D ------------------- */

netsend(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    extern struct NetConn *NetConP;

    if (argc > 1)
    {
	fdprintf(2, "I take no arguments\r\n");
	return;
    }

    if (argc == 0)
	switch(arg)
	{
	    case AO:
		fdprintf(2, "Abort output (send an Abort-Output and Synch)\r\n");
		return;
	    case BREAK:
		fdprintf(2, "Break; that is, send a Break and Synch\r\n");
		return;
	    case IP:
		fdprintf(2, "Interrupt process (send IP and Synch)\r\n");
		return;
	    case AYT:
		fdprintf(2, "Ask foreign host if it is still alive (send AYT)\r\n");
		return;
	    case EC:
		fdprintf(2, "Erase last character (send EC)\r\n");
		return;
	    case EL:
		fdprintf(2, "Erase to beginning of line (send EL)\r\n");
		return;
	    case DM:
		fdprintf(2, "Send a Synch\r\n");
		return;
#ifdef THP
	    case SUSPEND:
		fdprintf(2, "Suspend the connection\r\n");
		return;
	    case CONTINUE:
		fdprintf(2, "Continue the connection\r\n");
		return;
#endif
	}

    if (NetConP == 0)
    {
	fdprintf(2, "No active connection\r\n");
	return;
    }

    switch(arg)
    {
#ifdef THP
	case SUSPEND:
	    SendCtrl(NetConP, SUSPEND, 0, 0);
	    NetConP->LocState = SUSPING;
	    break;
	case CONTINUE:
	    SendCtrl(NetConP, CONTINUE, 0, 0);
	    NetConP->LocState = ACTIVE;
	    break;
#endif
	case AO:
	case BREAK:
	case IP:
	    SendCtrl(NetConP, arg, 0, 0);
	    SendSynch(NetConP);
	    break;
	case AYT:
	case EC:
	case EL:
	    SendCtrl(NetConP, arg, 0, 0);
	    break;
	case DM:
	    SendSynch(NetConP);
	    break;
	default:
	    fdprintf(2, "netsend(0%o)??\r\n", arg);
	    break;
    }
}

/* -------------------------- H E L P ----------------------------- */

help(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int i;
    extern struct ComEntry ComTab[];

    if (argc == 1)     /* Give each command and have it show-and-tell */
    {
#ifdef TELNET
	fdprintf(2, "TELNET commands\r\n");
#endif
#ifdef THP
	fdprintf(2, "THP commands\r\n");
#endif
	fdprintf(2, "Only enough of each command to uniquely identify it is needed\r\n\n");
	for (i = 0; ComTab[i].ComFunc; i++)
	{
	    fdprintf(2, "%-11s", ComTab[i].ComName);
	    (*(ComTab[i].ComFunc)) (ComTab[i].ComArg, 0, &ComTab[i].ComName);
	}
    }
    else if (argc == 0)
	fdprintf(2, "Briefly explain each command\r\n");
    else
    {
	fdprintf(2, "Sorry, individual explanations not implemented yet\r\n");
	fdprintf(2, "Type 'help' for a list of all commands and their purposes\r\n");
    }
    return;
}

/* -------------------------- E X E C S H ----------------------- */
/*
 * Execute the rest of the line as a shell command.
 * If there were no arguments, invoke a subshell.
 */
char shellcmd[] "/bin/sh";

execsh(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int old2, old3, junk;
    register char *p;
    int oldmode;
    int subsh;
    extern int errno;
    extern int prompt;

/*
 * The arguments to this command (argv[1] on) are joined back together
 * and fed to the shell. Joining back together is, of course, klugy...
 */
    if (argc <= 0)
        fdprintf(2, "Pass the rest of the line to the shell for execution\r\n");
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
            *p = ' ';        /* Replace with blank */
        }
        old2 = signal(2, 1);
        old3 = signal(3, 1);
        oldmode = ChgMode(OrigMode());
        switch(fork())
        {
            case -1:
                fdprintf(2, "No processes!\r\n");
                break;
            case 0:
                signal(2, 0);
                signal(3, 0);
                if (subsh)
		    execl(shellcmd, "-net-shell", 0);
                else
		    execl(shellcmd, "-net-shell", "-c", argv[1], 0);
                com_err(errno, "Attempting to execute %s", shellcmd);
                exit(0);        /* Child process */
            default:
                wait(&junk);
                signal(2, old2);
                signal(3, old3);
                fdprintf(2, "%c\n", prompt);
                ChgMode(oldmode);
                break;
        }
    }
}

/* -------------------------- E X P R O G ---------------------- */
/*
 * Invoke the command whose name is "arg". This direct execution
 * does not change the tty modes to "local". Hence any program
 * invoked by this command must put out "\r\n" to end each line,
 * rather than just "\n", so that if CRMOD has been cleared the command's
 * output will still print properly. (The change of "\n"s to "\r\n"s is
 * all that distinguishes /etc/netty from /bin/stty.)
 */
exprog(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int junk;
    extern int errno;
    extern char *errmsg();
    extern char *progname;
    extern int prompt;

    if (argc == 0)
	fdprintf(2, "Invoke the UNIX %s command\r\n", argv[0]);
    else
    {
	switch(fork())
	{
	    case -1:
		fdprintf(2, "No processes!\r\n");
		break;
	    case 0:
		execv(arg, argv);
		fdprintf(2, "%s: %s. Attempting to execute %s\r\n",
				progname, errmsg(errno), arg);
		exit(0);      /* Child process */
	    default:
		wait(&junk);
		fdprintf(2, "%c\r\n", prompt);
		break;
	}
    }
}

/* -------------------------- S E T --------------------------------- */
/*
 * set(arg, argc, argv) sets the variable to the character described.
 * Currently, only the escape character can be set.
 */
set(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    extern int prompt;

    if (argc == 0)
	fdprintf(2, "Set the escape character (currently '%c')\r\n", prompt);
    else if (argc != 2 || ! (seq(argv[1], "off") || argv[1][1] == '\0'))
	fdprintf(2, "Usage: %s {c|off}\r\n", argv[0]);
    else if (seq(argv[1], "off"))
	prompt = -1;
    else
	prompt = argv[1][0];
}

/* -------------------------- F O R K O F F -------------------------- */
/*
 * forkoff() is used by the server to fork off another instance of
 * THP/TELNET. It forks and execs its argument, supplying the same arguments
 * that THP/TELNET was called with originally. (Note that we can't just
 * call main again, since that would eventually overflow the stack.)
 */
forkoff(arg, argc, argv)
int arg;
int argc;
char *argv[];
{
    int i;
    extern int errno;
    extern char *arglist[];

    if (argc == 0)
    {
        fdprintf(2, "Fork off a new server\r\n");
        return;
    }
    if (argc != 2)
    {
        fdprintf(2, "Usage: %s filename\r\n", argv[0]);
        return;
    }
    while ((i = fork()) == -1)
         sleep (10);
    if (i)
        return;  /* Parent returns */
    for (i = 3; i < 16; i++)    /* 16 is hopefully enough */
        close(i);
    errno = 0;
    execv(argv[1], arglist);
    com_err(errno, "%d. Attempting to exec %s", errno, argv[1]);
    exit(1);	/* Child process (forkoff) */
}

/* -------------------------- C H G C O N N --------------------------- */
/*
 * ChgConn handles the multiple-connection database, which is entirely
 * internal to this routine. It is both a command, called as
 *      ChgConn(type, argc, argv)
 * and an internal function, called as
 *      ChgConn(type, connp, name)
 * It switches off its type, as follows:
 *     'e' (Enter connection) function
 *         Assign name, if nonzero, to the given connp. If it is not already
 *         in the connection table, enter it.
 *     's' (Suspend) function
 *         Make the current connection (if any) the default connection.
 *         Switch to normal tty modes and zero NetConP.
 *     'c' (Change) command
 *         If an argument is given, search for that connection.
 *         If no argument is given, use DefP.
 *         Point DefP at the current connection, if any.
 *     'l' (List) command
 *         List the saved connections. Indicate which is default, etc.
 *     'd' (Delete) function
 *         Delete (unsave) connp.
 */
ChgConn(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int i, j;
    struct NetConn *connp;
    char *lastp;
    char *endp;
    static struct NetConn *DefP;  /* Default for switch */
#define MAXSAVED 5
    static struct NetConn *Saved[MAXSAVED]; /* Saved connections, initially 0 */
    extern struct NetConn *NetConP;
    extern struct NetConn *PreP;
    extern netintr();

/* 'd' means destroy connection block pointed to by argc */

    if (arg == 'd')
    {
        connp = argc;
        for (i = 0; i < MAXSAVED; i++)
            if (connp == Saved[i])
                Saved[i] = 0;
        return;
    }

/* 'e' means enter connection block pointed to by argc */
/* Whose name is given by argv */

    if (arg == 'e')
    {
        connp = argc;
        if (connp == 0)
        {
            com_err(0, "Zero connp fed to ChgConn('e')");
            abort();
        }
        lastp = &connp->CName[sizeof(connp->CName)-1];
        if (argv != 0)                  /* Assign name */
            scopy(argv, connp->CName, lastp);
        for (i = 0; i < MAXSAVED; i++)  /* Check if already in table */
            if (Saved[i] == connp)
                return;  /* Already in table */

        for (i = 0; i < MAXSAVED; i++)  /* Enter it */
            if (Saved[i] == 0)
            {
                Saved[i] = connp;
                break;
            }
        if (i >= MAXSAVED)              /* Was there room? */
        {
            fdprintf(2, "(Connection not saved)\r\n");
            return;
        }
        for (j = 0; j < MAXSAVED; j++)  /* Eliminate duplicate name */
        {
            if (Saved[j] == 0 || j == i)
                continue;
            if (seq(connp->CName, Saved[j]->CName) || connp->CName[0] == '\0')
            {
                endp = connp->CName + slength(connp->CName);
                if (endp >= lastp-1)
                    endp--;
                scopy(locv(0, i), endp, lastp);
            }
        }
        return;
    }

    if (arg == 's')    /* If there is a current connection, suspend it */
    {
        if (NetConP == 0)
            return;
        ChgMode(OrigMode()); /* TTY */
        signal(2, 0);        /* intr */
        DefP = NetConP;
        NetConP = 0;
        return;
    }

    if (argc == 0)
        switch(arg)
        {
            case 'c':
                fdprintf(2, "Switch between connections\r\n");
                return;
            case 'l':
                fdprintf(2, "List saved connections\r\n");
                return;
        }

    if (argc > 2)
    {
        fdprintf(2, "Usage: %s [ conn-name ]\r\n", argv[0]);
        return;
    }

    if (arg == 'c')    /* Change to a saved-away connection */
    {
        connp = NetConP;
        if (argc <= 1)
        {
            if (DefP)
            {
                NetConP = DefP;
                ChgSB(NetConP->StateP);
            }
            else
                fdprintf(2, "No default connection\r\n");
        }
        else
        {
            for (i = 0; i < MAXSAVED; i++)
                if (Saved[i] != 0)
                    if (seq(Saved[i]->CName, argv[1]))
                    {
                        NetConP = Saved[i];
                        ChgSB(NetConP->StateP);
                        signal(2, &netintr);     /* Just in case */
                        break;
                    }
            if (i >= MAXSAVED)
                fdprintf(2, "%s: No such connection: %s\r\n", argv[0], argv[1]);
        }
        if (connp != 0)
            DefP = connp;
        return;
    }

    if (arg == 'l')    /* List connections */
    {
        for (i = 0; i < MAXSAVED; i++)
        {
            if (Saved[i] == 0)
                continue;
            if (Saved[i] == NetConP)
                fdprintf(2, "-> ");
            else
                fdprintf(2, "   ");
            fdprintf(2, "%s ", Saved[i]->CName);
            if (Saved[i] == DefP)
                fdprintf(2, "(default) ");
            if (Saved[i] == PreP)
                fdprintf(2, "(preempting) ");
            if (!Cempty(Saved[i]->ToUsrBuf))
                fdprintf(2, "(pending output) ");
            if (!Cempty(Saved[i]->ToNetBuf))
                fdprintf(2, "(pending input) ");
            fdprintf(2, "\r\n");
        }
        fdprintf(2, "\r\n");
        return;
    }

    com_err(0, "ChgConn called with bad arg 0%o\n", arg);
    return;
}

/* -------------------------- N E T C L O S E ----------------------- */
/*
 * Close a network connection. All we do here is set ToNetEOF; eventually
 * the protocol-specific ToNet routine will finish sending data in the buffer
 * and close the connection.
 */
netclose(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    extern struct NetConn *NetConP;

    if (argc == 0)
        fdprintf(2, "Close the current network connection\r\n");
    else if (argc != 1)
        fdprintf(2, "netclose takes no arguments\r\n");
    else if (NetConP == 0)    /* Connection we're supposed to close doesn't exist */
        return;
    else
        NetConP->ToNetEOF = 1;
}
