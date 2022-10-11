#include "signal.h"
#include "netlib.h"
#ifdef NCP
#include "ncpopen.h"
#endif
#ifdef TCP
#include "con.h"
#include "errno.h"
#define SIGINR SIGURG
#endif
#include "tnio.h"
#include "telnet.h"

static NETCONN *curconn;    /* Current connection */
int AutoFlush = 1;
int AutoFill = 1;
int telwtlog = -1;
int telrdlog = -1;

/*
 * -------------------------- T E L I N I T ------------------------- */
/*
 * telinit(fd) assumes that fd has been opened to a network file descriptor
 * and initializes everything accordingly.
 */
NETCONN *
telinit(fd)
    int fd;
{
    register NETCONN *np;
    int i;
    NETCONN *prev_conn;
    extern NoAct(), AYTdefault(), BREAKdefault(), IPdefault();
    extern RcvSynch();
    extern char *malloc();

    np = (NETCONN *)malloc(sizeof(*np));
    if (np == NULL)
	return(NULL);

    prev_conn = curconn;
    curconn = np;   /* Save for RcvSynch() */
    np->NextConP = prev_conn;	/* And put in list */
    np->NetFD = fd;

#   ifdef TCP
      np->UrgCnt = 0;
      np->UrgInput = 0;
#   endif TCP
#   ifdef NCP
      np->INSCount = 0;
#   endif NCP
    np->ECf = NoAct;
    np->ELf = NoAct;
    np->AOf = NoAct;
    np->IPf = IPdefault;
    np->AYTf = AYTdefault;
    np->BREAKf = BREAKdefault;
    np->GAf = NoAct;
    np->SYNCHf = NoAct;
    Bsetbuf(np->ToUsrBuf, NULL, 0);
    Bfill(np->FmNetBuf, NULL, 0);
    Bsetbuf(np->ToNetBuf, NULL, 0);
    telflush(np);   /* Just to initialize ToNetBuf properly */
    np->ProtState = PROTINIT;
    for (i = 0; i < N_OPTIONS; i++)
    {
	np->OptState[i][0] = 0;
	np->OptState[i][1] = 0;
	np->Orig[i][0] = 0;
	np->Orig[i][1] = 0;
    }
    signal(SIGINR, RcvSynch);
    return(np);
}

/*
 * -------------------------- T E L R E A D ------------------------- */
/*
 * telread processes characters from the network,
 * handling TELNET controls via NetChar. Normal text is passed to the user.
 * Returns number of chars for user (which could be 0), or TELEOF or TELERR.
 */
telread(connp, bufp, bufsize)
    register NETCONN *connp;
    char *bufp;
    int bufsize;
{
    extern int AutoFlush;   /* Nonzero to enable auto telflush */
    extern int AutoFill;    /* Nonzero to enable auto telfill */
    extern int telrdlog;    /* >= 0 to enable logging of incoming data */

    if (bufsize <= 0)
    {
	extern int errno;

	errno = EINVAL;
	return(TELERR);
    }

    Bsetbuf(connp->ToUsrBuf, bufp, bufsize);
    if (Bleft(connp->FmNetBuf) == 0)
	if (AutoFill)
	{
	    int nfill;

	    nfill = telfill(connp);
	    if (nfill == TELEOF || nfill == TELERR)
		return(nfill);
	}

    while (Bleft(connp->FmNetBuf) > 0 && Bfree(connp->ToUsrBuf) > 0)
    {
	if (connp->ProtState != PROTINIT && Bfree(connp->ToNetBuf) < PROTMAX)
	    if (AutoFlush)
		telflush(connp);
	    else
		break;
	if (connp->ProtState != PROTINIT && Blen(connp->ToUsrBuf) > 0)
	    break;
	NetChar(connp, Bgetc(connp->FmNetBuf));
    }

    if (Blen(connp->ToNetBuf) > 0 && AutoFlush)
	telflush(connp);
    return(Blen(connp->ToUsrBuf));
}

/*
 * -------------------------- T E L F I L L ------------------------- */
/*
 * Fills FmNetBuf, if it is empty. Returns 0 if did nothing
 * (because FmNetBuf had data in it), TELERR, TELEOF, or # characters read.
 */
telfill(connp)
    register NETCONN * connp;
{
    int nread;
    extern int errno;

    if (Bleft(connp->FmNetBuf) == 0)
    {

#    ifdef TCP
	if (connp->UrgInput) connp->UrgInput = ChkUrgent(connp);
#    endif TCP

    RETRY:
	nread = read(connp->NetFD, connp->FmNetData, sizeof(connp->FmNetData));
	switch(nread)
	{
	default:
	    break;
	case 0:
#	ifdef TCP
	    {
		struct netstate tstat;
		ioctl(connp->NetFD,NETGETS, &tstat);
		if (tstat.n_state & 0377)
		    printf (" (Strange read completion. %o) ",
			tstat.n_state);
		else printf(" (Remote close.) ");
	    }
	
#	endif TCP
	    return(TELEOF);
	case -1:
	    if (errno == EINTR)
		goto RETRY;
#       ifdef TCP
	    else if (errno == ENETSTAT) {
		struct netstate tstat;
		ioctl(connp->NetFD,NETGETS, &tstat);
		if (tstat.n_state & URXTIMO) {
			printf("\nForeign host not responding\n");
			goto RETRY;
		} else if (tstat.n_state & URESET)
			printf("\nForeign host reset connection\n");
		else if (tstat.n_state & UURGENT)
			goto RETRY;
		cmderr(0, "error return from network (status %o).\n", 
			   tstat.n_state);
		return(TELERR);
	     }
#       endif TCP
	    else
	     {
		cmderr(errno, "error return from network.");
		return(TELERR);
	      }
	}
	if (telrdlog >= 0)
	    write(telrdlog, connp->FmNetData, sizeof(connp->FmNetData));
	Bfill(connp->FmNetBuf, connp->FmNetData, nread);
	return(nread);
    }
    return(0);
}

/*
 * -------------------------- T E L F L U S H ----------------------- */
/*
 * Flush accumulated data in ToNetBuf. Return values:
 *	0 - nothing to write;
 *	TELERR - error while writing;
 *	positive value - amount written
 */
telflush(connp)
    register NETCONN *connp;
{
    int nwrite;
    int retval;
    extern int errno;
    extern int telwtlog;

    retval = 0;
    if (Blen(connp->ToNetBuf) != 0)
    {
#	ifdef TCP
	if (connp->UrgCnt)
	 {			       /* send some bytes urgently */
	   nwrite =
	     write(connp->NetFD, Baddr(connp->ToNetBuf), connp->UrgCnt);
	   ioctl(connp->NetFD, NETRSETU, NULL);
	   connp->UrgCnt = 0;	       /* NETSETU was done by SendUrg */
	 } else nwrite = 0;
	if (nwrite >= 0) nwrite +=
	   write(connp->NetFD, Baddr(connp->ToNetBuf) + connp->UrgCnt,
	    Blen(connp->ToNetBuf) - connp->UrgCnt);
#	endif TCP
#	ifdef NCP
	 nwrite =
	 write(connp->NetFD, Baddr(connp->ToNetBuf), Blen(connp->ToNetBuf));
#	endif NCP

	switch(nwrite)
	{
	default:
	    retval = nwrite;
	    if (telwtlog >= 0)
		write(telwtlog, Baddr(connp->ToNetBuf), Blen(connp->ToNetBuf));
	    break;
	case 0:
	    errno = 0;
	    retval = TELERR;
	    break;
	case -1:
	    retval = TELERR;
	    break;
	}
    }
    Bsetbuf(connp->ToNetBuf, connp->ToNetData, sizeof(connp->ToNetData));
    return(retval);
}

/*
 * -------------------------- T E L W R I T E ----------------------- */
/*
 * Write the specified data onto the net. Doubles IACs.
 * Returns # characters from user actually written, or TELERR.
 * The number will be equal to the number supplied.
 * If AutoFlush is turned off, TELERR will be returned if the entire buffer
 * cannot all be written.
 */
telwrite(connp, bufp, bufsize)
    register NETCONN *connp;
    char * bufp;
    int bufsize;
{
    struct GBuffer fm_user;
    struct PBuffer to_net;
    extern int AutoFlush;

/* Save ToNetBuf if we might have to back up */

    PBcopy(connp->ToNetBuf, to_net);

/* Put the data in ToNetBuf */

    Bfill(fm_user, bufp, bufsize);
    while (Bleft(fm_user) > 0)
    {	register int c;

	if (Bfree(connp->ToNetBuf) < 2)    /* Allow room for doubled IAC */
	    if (AutoFlush)
		telflush(connp);
	    else
		break;
	c = Bgetc(fm_user);
	Bputc(c, connp->ToNetBuf);
	if (c == IAC)
	    Bputc(c, connp->ToNetBuf);
    }

    if (AutoFlush && Blen(connp->ToNetBuf) > 0)
	telflush(connp);

    if (Bleft(fm_user) > 0)
    {
	PBcopy(to_net, connp->ToNetBuf);
	return(TELERR);
    }
    else
	return(bufsize);
}

/*
 * -------------------------- S E N D C ----------------------------- */
/*
 * Sends one character, by putting it in ToNetBuf. Calls telflush if necessary.
 */
SendC(connp, c)
    register NETCONN * connp;
    int c;
{
    if (Bfree(connp->ToNetBuf) == 0)
	if (AutoFlush)
	    telflush(connp);

    Bputc(c, connp->ToNetBuf);
}

/*
 * -------------------------- R C V S Y N C H ------------------- */
/*
 * Called when an NCP INS signal is received. Since it is a signal,
 * there's no way to know what connection it came in on.
 * Just assume it's the current connection.
 */
RcvSynch()
{
    register NETCONN *connp;

    signal(SIGINR, RcvSynch);
    connp = curconn;	/* Saved by telinit */
#   ifdef NCP
	connp->INSCount++;
#   endif NCP
#   ifdef TCP
	if (!(connp->UrgInput))
	    connp->UrgInput=ChkUrgent(connp);
#   endif TCP
    (*(connp->SYNCHf))(connp);
}

/*
 * -------------------------- T E L F I N I S H --------------------- */
/*
 * Free up resources allocated to the given connection.
 * If we can, flush pending data.
 */
telfinish(connp)
    register NETCONN *connp;
{
    register NETCONN * * pp;
    extern int AutoFlush;

    if (AutoFlush)
	telflush(connp);

#   ifdef TCP
	    ioctl(connp->NetFD, NETCLOSE, NULL);
#   endif TCP

    for (pp = &curconn; *pp != NULL; pp = &((*pp)->NextConP))
	if (*pp == connp)
	{
	    *pp = connp->NextConP;
	    break;
	}

    free(connp);
}

/*
 * -------------------------- T E L C L O S E ----------------------- */
/*
 * Close a TELNET connection opened via telopen().
 */
telclose(connp)
    register NETCONN *connp;
{
    int fd;

    fd = connp->NetFD;
    telfinish(connp);
    close(fd);
}

/*
 * -------------------------- S E N D U R G ------------------------- */
/*
 * Called to send urgent notification on the given connection.
 * For NCP, this is just a sendins().
 */
SendUrg(connp)
    register NETCONN *connp;
{
#   ifdef NCP
    sendins(connp->NetFD);
#endif NCP
#ifdef TCP		/* urgent on, eols still on. */
    if (!(connp->UrgCnt))
	 ioctl(connp->NetFD, NETSETU, NULL);
    connp->UrgCnt = Blen(connp->ToNetBuf);
    telflush(connp);
#endif TCP
}
/*
 * ----------------------- C h k U r g e n t ------------------------
 *
 * called to check if connection is still in urgent state.
 *
 */

#ifdef TCP
ChkUrgent(connp)
   register NETCONN *connp;
{
	struct netstate tstat;
	ioctl(connp->NetFD,NETGETS, &tstat);
	return(tstat.n_state & UURGENT);
 } 
#endif TCP
/*
 * -------------------------- T E L E M P T Y ----------------------- */
/*
 * Return status of buffers.
 */
telempty(connp)
    register NETCONN * connp;
{
    int retval;

    retval = 0;
    if (Bleft(connp->FmNetBuf) > 0)
	retval += 1;
    if (Blen(connp->ToNetBuf) > 0)
	retval += 2;
    return(retval);
}

/*
 * -------------------------- T E L F D ----------------------------- */
/*
 * Return the file descriptor of the connection.
 */
telfd(connp)
    register NETCONN * connp;
{
    return(connp->NetFD);
}

/* -------------------------- A U T O F I L L ----------------------- */

autofill(flag)
    int flag;
{
    AutoFill = flag;
}

/* -------------------------- A U T O F L U S H --------------------- */

autoflush(flag)
    int flag;
{
    AutoFlush = flag;
}

/*
 * -------------------------- T E L F U N C ------------------------- */
/*
 * Set function handler.
 */
typedef int func();
func *telfunc(connp, control, handler)
    register NETCONN * connp;
    int control;
    int (*handler)();
{
    int (*old_handler)();

    switch(control)
    {
    case EC:
	old_handler = connp->ECf;
	connp->ECf = handler;
	break;
    case EL:
	old_handler = connp->ELf;
	connp->ELf = handler;
	break;
    case AO:
	old_handler = connp->AOf;
	connp->AOf = handler;
	break;
    case IP:
	old_handler = connp->IPf;
	connp->IPf = handler;
	break;
    case BREAK:
	old_handler = connp->BREAKf;
	connp->BREAKf = handler;
	break;
    case AYT:
	old_handler = connp->AYTf;
	connp->AYTf = handler;
	break;
    case GA:
	old_handler = connp->GAf;
	connp->GAf = handler;
	break;
    case SYNCH:
	old_handler = connp->SYNCHf;
	connp->SYNCHf = handler;
	break;
    default:
	old_handler = NULL;
	break;
    }
    return(old_handler);
}

/*
 * -------------------------- T E L O P E N ------------------------- */
/*
 * telopen(hostnum) opens a connection to the specified host.
 */
NETCONN *
telopen(hostnum)
netaddr	hostnum;
{
    int fd;
#   ifdef NCP
     static struct ncpopen openparams;
#   endif NCP
#   ifdef TCP
     static struct con openparams;
#   endif TCP;
    extern int errno;

#   ifdef NCP
        openparams.o_type = 0;
        openparams.o_fskt = SERVER_SOCKET;
        openparams.o_lskt = 0;
        openparams.o_nomall = 0;
        openparams.o_host = hostnum;
    
        fd = open("/dev/net/ncp", &openparams);
#   endif NCP
#   ifdef TCP
        openparams.c_mode = CONTCP | CONACT;
        openparams.c_fport = SERVER_SOCKET;
        openparams.c_lport = 0;
        openparams.c_sbufs = 1;
	openparams.c_rbufs = 2;
        openparams.c_lo = openparams.c_hi = 0;
        openparams.c_fcon = hostnum;
	mkanyhost(openparams.c_lcon);
	openparams.c_timeo = 0;
    
        if ((fd = open("/dev/net/tcp", &openparams)) >= 0)
	    ioctl(fd, NETSETE, NULL);/* urgent assumed off, eols on. */
#   endif TCP
    if (fd == -1)
	return(NULL);
    return(telinit(fd));
}

/* ------------------------------------------------------------------ */
