/*
 * Simple-minded user telnet - 2 process version
 */
#include "stdio.h"
#include "signal.h"
#include <errno.h>
#include "globdefs.h"
#include "tnio.h"
#include "telnet.h"
#include "ttyctl.h"

#define BUFSIZE 256

main(argc, argv)
    int argc;
    char * argv[];
{
    extern char * progname;
    extern int fsocket;
    extern int par_uid;
    int sendint();
#ifdef SIGTSTP
    int ususpend();
#endif

    par_uid = getpid();
    progname = argv[0];
    option(1, OPT_ECHO);    /* Accept remote echo */
    option(1, OPT_SUPPRESS_GA);/* And suppress-go-ahead */
    fsocket = SERVER_SOCKET;
    if (argc > 1)
	znetopen(1, argc, argv);
    signal(SIGQUIT, SIG_IGN);
#ifdef SIGTSTP
    sigset(SIGTTIN, ususpend);
    sigset(SIGTTOU, ususpend);
    sigset(SIGTSTP, ususpend);
#endif
    from_user(0);  /* Read stdin */
}

/* -------------------------- F R O M _ U S E R --------------------------- */
/*
 * Copy from terminal and process what you get, sending it to command
 * processor or network as appropriate.
 */
from_user(fd)
    int fd;
{
    int nread;
    char buf[BUFSIZE];
    extern NETCONN * NetConP;
    extern int needprompt;
    extern char prompt[32];
    extern int errno;

    for(;;)
    {
	if (needprompt)
	{
	    write(2, prompt, strlen(prompt));
	    needprompt = 0;
	}
	check_done();
	nread = read(fd, buf, sizeof(buf));
	if (nread == -1)
	    if (errno == EINTR)
		continue;
	    else
	    	cmderr(-1, "Error occurred while reading standard input.\n");
	process(buf, nread, NetConP);
	check_done();
    }
}

/* -------------------------- F R O M _ N E T ----------------------- */
/*
 * Copy from net to terminal.
 */
from_net(fd, connp)
    int fd;
    NETCONN * connp;
{
    int nread;
    char buf[BUFSIZE];
    extern int other_pid;
    extern int ndone;

    for (;;)
    {
	check_done();
	nread = telread(connp, buf, sizeof(buf));
	if (nread == TELEOF || nread == TELERR)
	{
	    if (nread == TELERR)
		cmderr(-1, "Error occurred while reading network.\n");
	    ndone++;
	}
	else if (nread > 0)
	    write(fd, buf, nread);
	check_done();
    }
}

/* -------------------------- C H E C K _ D O N E ------------------- */
/*
 * Examine the environment (all the done flags, etc.) and if done,
 * kill other process (if it exists) and exit.
 */
check_done()
{
    extern NETCONN * NetConP;
    extern int ndone;
    extern int udone;
    extern int xdone;
    extern int other_pid;

    if
    (
	xdone ||    /* Aborting exit */
	(
	  (udone || ndone) &&	/* Otherwise make sure net quiescent */
	  (NetConP == NULL || telempty(NetConP) == 0)
	)
    )
    {
	if (NetConP != NULL)
#	ifdef TCP
	    if (udone) telfinish(NetConP); else
#	endif TCP
	    telclose(NetConP);
	ChgMode(OrigMode());
#	ifdef TCP
	    if (udone && other_pid) wait(&other_pid); else
#	endif TCP
	if (other_pid)
	    kill(other_pid, SIGKILL);
	exit(0);
    }
    return(1);
}
/* ------------------------------------------------------------------ */

sendint()
{
	extern NETCONN *NetConP;

	signal(SIGINT, sendint);
	sendctl(NetConP, IP);
}

#ifdef SIGTSTP
ususpend(signo)
int signo;
{
	static TTYMODE *oldmode;

	oldmode = ChgMode(OrigMode());
	sigset(signo, SIG_DFL);
	kill(0, signo);
	sigset(signo, ususpend);
	ChgMode(oldmode);
}
#endif
