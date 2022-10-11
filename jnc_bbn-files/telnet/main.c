#include "cbuf.h"
#include "protocol.h"
#include "/sys/sys/h/sgttybuf.h"
#include "netconn.h"

#define EINTR 4        /* System call interrupted (by SIGINR) */


/* -------------------------- M A I N ----------------------------- */

main(argc, argv)
    int argc;
    char *argv[];
{
    char *p;
    int i, j;
    static char argbuf[512];     /* argv stuff copied into here */
    extern char *progname;
    extern char *arglist[];
    extern int prompt;
    extern process();
    extern FromUser();

/* Copy argument list where it will be safe from ravages of sbrk */

    for (i = 0, p = argbuf; i < argc && i < 50; i++)
    {
        arglist[i] = p;
        p = scopy(argv[i], p, &argbuf[sizeof(argbuf)-1]) + 1;
    }
    arglist[argc] = 0;    /* Make it useful for execs */

/* Initialize */

    progname = arglist[0];

/* Process -e (execute remaining args) */

    for (i = 1; i < argc; i++)
        if (seq(argv[i], "-e"))
        {
            for (j = i+1; j < argc; j++)
                ExecCmd(argv[j], slength(argv[j]));
            break;
        }

    argc = i; /* Ignore them */

/* If there are any other arguments, do a net open with them. */

    if (argc > 1)
        netopen(1, argc, argv); /* The 1 means exit on error */
    else
    {
#ifdef TELNET
        fdprintf(2, "User Telnet version 2.\r\n");
#endif
#ifdef THP
        fdprintf(2, "User THP version 2.\r\n");
#endif
        fdprintf(2, "Type \"%chelp\" for information.\r\n", prompt);
    }

/* Queue up user-stream handler. The handler argument of 0 is used
 * to indicate that this stream is the one from the user, and should
 * have command interpretation performed on it.
 */
    IoEnter(0, FromUser, 0, 0);    /* Read from standard input */

    IoRun(20);  /* Loop forever, awaiting 20 seconds */

    fatal(0, "Unexpected return from IoRun");
}

/* -------------------------- T O U S E R --------------------------- */
/*
 * ToUser copies from the ToUsrBuf for this connection to the user/pty stream.
 * If connp is 0, it is handling the user, and uses NetConP.
 */
ToUser(fd, cap, connp)
    int fd;
    int cap;
    struct NetConn *connp;
{
    extern int errno;
    extern struct NetConn *NetConP;

    if (cap < 0)
        cap = 32767;        /* Infinity */
    if (connp == 0)
        connp = NetConP;  /* Use current connection */

    if (cap > 0 && connp != 0 && !Cempty(connp->ToUsrBuf))
    {
        switch(Cwrite(fd, connp->ToUsrBuf, cap))
        {
            case -1:
                if (errno == EINTR)
                    break; /* Ignore it */
                else
                    fatal(errno, "writing user output");
            case 0:
                fatal(0, "writing user output returned EOF");
        }
        return(0);
    }
    else
        return(1);  /* Did nothing */
}

/* -------------------------- F R O M U S E R ----------------------- */
/*
 * FromUser() is called by IoRun() when there is input from the user or pty.
 * It uses UsingPty to decide which. If ToNetBuf is less than half full,
 * FromUser reads the input fd. If ToNetBuf is more than half full, then it reads
 * only if this is the user stream and command processing is turned on,
 * in which case it still only reads enough to fit (assuming worst case
 * expansion of each character), or at least one char, anyway. This algorithm
 * postpones the discarding of chars from the user as long as possible.
 */
FromUser(fd, cap, aconnp)
    int fd;
    int cap;
    struct NetConn *aconnp;
{
    register struct NetConn *connp;
    int nread, nreq;
    char inbuf[(CBUFSIZE/NETGROW)+1];
    int usersw;
    extern int process();
    extern int errno;
    extern int OldInput;
    extern int OldPrompt;
    extern struct NetConn *NetConP;
    extern int prompt;

    connp = aconnp;
    if (connp == 0)
        connp = NetConP;
    if (connp == 0 || connp->UsingPty == 0)
        usersw = 1;
    else
        usersw = 0;

    if (connp != 0)
    {
        if (connp->RemState == SUSP)
            return(1);
        nreq = CBUFSIZE - Clen(connp->ToNetBuf);
    }
    else
        nreq = 1;

    if (nreq < CBUFSIZE/2)
        if (usersw == 0 || prompt == -1)
            return(1);      /* Not enough room to make it worthwhile */
        else /* Command processing turned on, read anyway */
            if (nreq == 0)
                nreq = 1;

    if (nreq >= NETGROW)
        nreq =/ NETGROW;  /* Assume every char expands into maximum of protocol */

    if (nreq > cap && cap >= 0)
        nreq = cap;

    if (nreq > 0)
    {
RETRY:
        switch(nread = read(fd, inbuf, nreq))
        {
            case -1:
                if (errno == EINTR)
                    goto RETRY;
                fatal(errno, "reading user input");

            default:
                break;

            case 0:
                if (usersw && OldInput >= 0)
                {
                    movefd(OldInput, fd);
                    prompt = OldPrompt;
                    OldInput = -1;
                    awtenb(fd);
                    com_err(0, "Input file sent.");
                    goto RETRY;
                }
                IoDelete(fd);  /* Don't try to read any more */
                break;
        }
        if (usersw)
            process(connp, inbuf, nread);
        else
            servproc(connp, inbuf, nread);
        return(0);
    }
    return(1);
}

/* -------------------------- S E R V P R O C ----------------------- */
/*
 * Handle output of PTY going out over network.
 * If a pty puts out CR-LF, it should turn into CR-LF,
 * and if it puts out CR-CR-LF, it should turn into CR-NUL-CR-LF.
 * So just put out the CR, and remember. On the next char, if it is not LF,
 * put out a NUL first. The TELNET protocol really needs an IAC NEWLINE...
 */
servproc(aconnp, inbuf, nread)
    struct NetConn *aconnp;
    char *inbuf;
    int nread;
{
    register struct NetConn *connp;
    register char *top;
    register char c;
    register char *op;
    char *ip;
    static char outbuf[CBUFSIZE];
    int count;

    connp = aconnp;

    op = outbuf;
    top = &inbuf[nread];
    for (ip = inbuf; ip < top; ip++)
    {
	c = *ip;
	if (connp->ToNetCr && c != '\n')
	    *op++ = '\0';
	*op++ = c;
	connp->ToNetCr = 0;
	if (c == '\r' && connp->ToNetBin == 0)
	    connp->ToNetCr = 1;
    }
    count = op - outbuf;
    if (connp != 0 && count > 0)
	SendBuf(connp, count, outbuf);
}

/* -------------------------- U S E R C H A R ----------------------- */
/*
 * Handle chars going to user or pty. The protocol says that a CR will
 * be followed by LF if it is meant as a newline, or by a NUL if it
 * is meant as a 'real' CR. For a pty, this means that the char after
 * the CR should always be suppressed (as long as it is CR or NUL).
 * For a user, the full CR-LF sequence should be output, but CR-NUL
 * should be stripped of the NUL.
 */
UserChar(c, aconnp)
    char c;
    struct NetConn *aconnp;
{
    register struct NetConn *connp;
    extern int errno;

    connp = aconnp;
    if (c == '\r' && connp->ToUsrBin == 0)
    {
	Cputc(c, connp->ToUsrBuf);
	connp->ToUsrCr = 1;
    }
    else if (connp->ToUsrCr == 0)
	Cputc(c, connp->ToUsrBuf);
    else
    {
	if (c == '\0' || (c == '\n' && connp->UsingPty))
	    ;  /* Drop the char */
	else
	    Cputc(c, connp->ToUsrBuf);
	connp->ToUsrCr = 0;
    }
}

/* -------------------------- N E T I N T R ------------------------- */

netintr()
{
    extern struct NetConn *NetConP;

    SendCtrl(NetConP, IP, 0, 0);
    SendSynch(NetConP);
    signal(2, &netintr);
}

/* -------------------------- O P E N P T Y ------------------------ */
/*
 * openpty() finds the first available pty and opens it for reading an
 * writing. It first tries to open the given name; if that fails, it adds
 * "step" to the last letter and tries again. It continues until it finds
 * a pty it can open, in which case it returns the fd of the pty, or until
 * it runs out, in which case it returns -1.
 */
openpty(ptyname, step)
    char *ptyname;
    int step;
{
    register char *s1;
    int ptyfd;
    extern int errno;

/* Find end of device name */

    s1 = ptyname;
    while (*s1++) ;
    s1 =- 2;

    for (; ; (*s1) =+ step)
    {
        ptyfd = open(ptyname, 2);  /* Try this PTY */
        if (ptyfd >= 0)
            return(ptyfd);
        if (errno == 2)
            return(-1);    /* No such file.  Assume end of PTYs */
    }
}

#define CMDBEGIN 10
#define CMDEND 20
#define NONE 30

/* -------------------------- P R O C E S S ------------------------- */
/*
 * Read from input buffer inbuf and put in command buffer cline (contained in
 * this routine) or output buffer via SendBuf.
 * It is assumed that there is enough room in the output buffer.
 * This routine identifies and executes the user's commands,
 * echoing them on fd 2 if necessary, and follows every CR with LF.
 * When the end of user input is reached (cnt = 0), it calls quit().
 *
 * This routine contains static variables. It can only be used on one stream
 * at a time in a process.
 */
process(connp, inbuf, cnt)
    struct NetConn *connp;
    char inbuf[];
    int cnt;
{
    register char *p;	  /* Points into inbuf */
    static char outbuf[CBUFSIZE];     /* Static just to keep stack size down */
    register char *op;	  /* Points into outbuf */
    int type;
    int c;	      /* Unsigned char */
    int junk[3];      /* For stty/gtty */
    static int cmdstate;  /*
			   * 0 -- not within command
			   * 1 -- cmdchar just typed
			   * other: within command
			   */
    static char cline[256];
    static char *clinep;
    static trunc;
    extern int prompt;
    extern int NoUsrEOF;

    if (cnt == 0)
    {
	if (NoUsrEOF == 0)
	    quit(0, 1, 0);
	return;
    }

    op = outbuf;
    for (p = inbuf; cnt > 0; p++, cnt--)
    {
	c = *p & 0377;

	if ((c&0177) == prompt)
	    type = CMDBEGIN;
	else if ((c&0177) == '\n' || (c&0177) == '\r')
	    type = CMDEND;
	else
	    type = NONE;

	switch(type + cmdstate)
	{
	case CMDBEGIN + 0:    /* Start receiving command */
	    cmdstate = 1;
	    clinep = cline;
	    trunc = 0;
	    if ((GetFlags(OrigMode())&ECHO) && ! (GetFlags(CurMode())&ECHO))
		write(2, &c, 1);
	    break;
	case NONE + 0:	      /* Ordinary char -- send it */
	case CMDBEGIN + 1:    /* Doubled command char -- send it */
	case CMDEND + 0:      /* CMDEND outside command mode -- send it */
	    *op++ = c;
	    if (c == '\r' && connp->ToNetBin == 0)    /* Follow CR with LF */
		*op++ = '\n';
	    cmdstate = 0;
	    break;
	case NONE + 1:	       /* Ordinary char in command mode */
	case NONE + 2:
	case CMDBEGIN + 2:
	    if ((GetFlags(OrigMode())&ECHO) && ! (GetFlags(CurMode())&ECHO))
		write(2, &c, 1);
	    if (clinep < &cline[sizeof(cline)])
		*clinep++ = c;
	    else
		trunc = 1;  /* Remember we are losing chars */
	    break;
	case CMDEND + 1:      /* Empty command line */
	case CMDEND + 2:      /* Execute command line */
	    if ((GetFlags(OrigMode())&ECHO) && ! (GetFlags(CurMode())&ECHO))
		write(2, "\r\n", 2);
	    if (trunc)
		fdprintf(2, "Command line too long -- ignored.\r\n");
	    else
	    {
/* Flush what we have in case command involves it */
		if (connp != 0 && op - outbuf > 0)
		    SendBuf(connp, op - outbuf, outbuf);
		op = outbuf;
/*
 * If the user input is a terminal, and it was not originally in raw mode
 * but is now, then do canonicalization (line editing).
 */
		if (gtty(0, junk) != -1
		&& !(GetFlags(OrigMode())&RAW) && GetFlags(CurMode())&RAW)
		    canon(cline, GetMode(OrigMode())->s_erase,
				     GetMode(OrigMode())->s_kill,
				     '\\');
		ExecCmd(cline, clinep - cline);
	    }
	    cmdstate = 0;
	    break;
	}
    }
    if (connp != 0 && op - outbuf > 0)
	SendBuf(connp, op - outbuf, outbuf);
    return;
}

/* ------------------------- U S E P T Y --------------------------- */
/* usepty() is a server utility. It opens a pty and attaches it
 * via FromUser and ToUser.
 */
usepty(connp)
    struct NetConn *connp;
{
    int fd;
    struct sgttybuf ttybuf;
    extern int termfd;
    extern int FromUser(), ToUser();

    if ((fd = openpty("/dev/ptyA", 1)) == -1)
    {
        SendBuf(connp, 7, "Sorry\r\n");
        netclose(0, 1, 0);
        com_err(0, "Unable to get pty");
        return;    /* Hopefully remote user will close and leave */
    }

/* Default is not to echo. Kludge this by setting speed to 134.5 baud. */

    gtty(fd, &ttybuf);
    ttybuf.s_ospeed = ttybuf.s_ispeed = B134;
    stty(fd, &ttybuf);

    connp->UsingPty = 1;
    termfd = fd; /* Terminal (for tty mode ctrl) is now pty */
    IoEnter(fd, &FromUser, &ToUser, connp);
    return;
}

/* -------------------------- C A N O N ----------------------------- */
/*
 * canon(line, erase, kill, esc) applies erase and kill processing
 * to the given line. esc is the escape character. The line is replaced.
 */
canon(line, erase, kill, esc)
    char *line;
    char erase;
    char kill;
    char esc;
{
    register char *ip;
    register char *op;

    for (ip = op = line; *ip; ip++)
    {
        if (*ip == erase)
        {
            if (op > line)
                op--;
        }
        else if (*ip == kill)
            op = line;
        else if (*ip == esc && (ip[1] == erase || ip[1] == kill))
            *op++ = *++ip;
        else
            *op++ = *ip;
    }
    *op = '\0';
}

/* -------------------------- M O V E F D --------------------------- */
/*
 * movefd(fd1, fd2) moves the device on fd1 to fd2. fd2 is closed to
 * assure that the move will work.
 */
movefd(fd1, fd2)
    int fd1;
    int fd2;
{
    int i;
    extern int errno;

    close(fd2);
    while((i = dup(fd1)) < fd2)
        if (i == -1)
            fatal(errno, "Cannot move fd %d.", fd1);
        else
            close(i);
}

/* -------------------------- R E D I R ----------------------------- */
/*
 * redir(arg, argc, argv) redirects the user input. If a file is specified,
 * it opens it, and moves the fd to the input or output. If an fd is specified,
 * it uses it directly.
 */
redir(arg, argc, argv)
    int arg;
    int argc;
    char *argv[];
{
    int fd;
    extern int errno;
    extern int OldInput;
    extern int OldPrompt;
    extern int prompt;
    extern char *atoiv();

    if (argc == 0)
        fdprintf(2, "Redirect user input\r\n");
    else if (argc == 1 || argc > 3)
        fdprintf(2, "Usage: %s -fd #\r\n\t %s filename\r\n",
                         argv[0], argv[0]);
    else
    {
        OldPrompt = prompt;  /* In case it is changed */
        if (seq(argv[1], "-fd"))
        {
            if (argc < 3)
                fdprintf(2, "%s: No fd specified.\r\n", argv[0]);
            else if (*atoiv(argv[2], &fd))
                fdprintf(2, "%s: Non-numeric fd specified: %s\r\n",
                                 argv[0], argv[2]);
        }
        else if ((fd = open(argv[1], 0)) == -1)
        {
            com_err(errno, "Cannot open input file\r\n");
            return;
        }
        OldInput = dup(0);
        movefd(fd, 0);
        prompt = -1;    /* Turn off command interpretation */
        awtenb(0);
    }
}

/* -------------------------- A L L O C O N N ----------------------- */
/*
 * AllocConn() allocates a connection block and initializes it for
 * the normal kind of connection.
 */
struct NetConn *
AllocConn()
{
    register struct NetConn *np;

    np = alloc(sizeof(*np));
    if (np == -1)
	return(-1);

    np->CName[0] = '\0';/* To be initialized later by ChgConn */
    np->StateP = 0;	/* To be initialized later, when conn estab */
    np->ExitOnClose = 0;
    np->Init[0] = 0;
    np->SendSecur = 0;
    np->UtcbP = 0;	/* To be initialized later, if TCP */
    np->NetFD = -1;	/* To be initialized later, if NCP */
    np->UsePtySw = 0;
    np->UsingPty = 0;
    np->Type = NORMAL;
    np->LocState = ACTIVE;
    np->RemState = ACTIVE;
    np->OutSynCnt = 0;
    np->InSynCnt = 0;
    np->RecSize = 0;
    np->ToNetBin = 0;
    np->ToNetCr = 0;
    np->ToUsrBin = 0;
    np->ToUsrCr = 0;
    np->ToNetEOF = 0;
    np->ToUsrEOF = 0;
    Cinit(np->ToNetBuf);
    Cinit(np->ToUsrBuf);
    Cinit(np->FmNetBuf);
    return(np);
}

/* -------------------------- F R E E C O N N ----------------------- */
/*
 * FreeConn frees a connection block.
 */
FreeConn(connp)
    struct NetConn *connp;
{
    free(connp);
}

/* -------------------------- G L O B A L S ----------------------- */
/*
 * These globals are used only within this file. Putting them at the
 * end forces them to be explicitly declared within every routine
 * that actually uses them, encouraging visibility.
 */

int OldInput -1;
int OldPrompt;
