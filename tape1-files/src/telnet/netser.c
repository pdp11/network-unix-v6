#include "signal.h"
#include "stdio.h"
#include "telnet.h"
#include "netlib.h"
#ifdef TCP
#include "con.h"
#define SIG_NETINT SIGURG
#include "errno.h"
#endif TCP
#ifdef NCP
#include "ncpopen.h"
#endif NCP
#ifndef MODTTY
#include "sgtty.h"
#endif MODTTY
#ifdef MODTTY
#include "modtty.h"
#endif MODTTY

/*
 * New Telnet Network Server
 * Modified Jan 4 1978 by BBN(dan) to use explicit byte count
 * instead of null when copying from pty to network.
 * This fixes the bug whereby nulls in output would chop out
 * pieces of it...
 *
 * General reformatting and deletion of Old Telnet code Jan 20 '79 BBN(dan)
 * No code was actually changed (as verified by a cmp of the object files).
 *
 * Use /dev/net/longhost and expect long host numbers from fstat
 * jsq BBN 3-14-79
 * Use /dev/net/ncp jsq BBN 4-27-79
 * Try to accept binary mode jsq BBN 2Aug79
 *
 * Arrange for each side to kill the other if it dies.	jsq BBN 18Aug79
 *
 * ?Nov79 jsq BBN remember to close files if fork for netgrf in main fails.
 * 13Feb80 dan BBN if MODTTY, do modtty to get real intr char.
 * 22Oct80 dan BBN change to use signal.h; change
 *	    fdprintf(fout...) to printf(...) since fout == 1 anyway;
 *	    change to use sgtty.h.
 * 20Feb81 ado BBN tcp version, for vax in-kernel tcp.
 * 7mar81 bpc BBN changed to send interrupt character properly, even in the 
 *                absence of modtty, but with V7
 * 11Sep81 rfg BBN changed telcmd to do proper echo option negotiation for 
 *		  the VAX
 * 12nov81 ers BBN changed code in interrupt() to be ifdef'd on TIOCGETC
 *		instead of on V7.
 */

#define LEVEL8 RAW

/* Parameters */

#define NETBUFSIZE 60	/* Number of characters in network buffer */
char firstpty = 'A';	/* Letter of first pseudo-teletype */

/* -------------------------- G L O B A L S -------------------------------- */

int pid;		/* Process ID of subsidiary fork */
int netchan;		/* Channel descriptor for network */
				/* channel used for TELNET data */
int ptyin;		/* Channel number of PTY used for input */
int ptyout;		/* Channel number of PTY used for output */
int logecho = 1;	/* ignore 'do echo' on login */
char echoflag;		/* Indicates 'will echo' has been sent */
int BinaryIn;		/* binary option on */
int BinaryOut;		/* binary option on */
char *Now();		/* returns string with current date */
char *progname; 	/* who are we */
int junk;		/* Somewhere to throw away things */
#ifdef NCP
#define NETPARAMS ncpopen
#endif
#ifdef TCP
#define NETPARAMS con
#endif TCP
struct NETPARAMS openparams;
int pty;		/* Pty name */
int debugsw = 0, debugsok = SERVER_SOCKET;
char *arg1p;		/* Pointer to arg[1] */
int inter;		/* Indication that an interrupt came in */
int seq();
int ins();		/* INS interrupt routine */



/* -------------------------- M A I N ------------------------------- */
/*
 * Loop. Each time a connection becomes established, fork and call netgrf.
 * Wait for child (netgrf) to exit (which should be immediately).
 */

main(argcnt,argvec)
    int argcnt;
    char *argvec[];
{
    extern char * atoiv();
    extern char *errmsg();

    if ((argcnt >= 2) && (debugsw=seq(argvec[1],"-debug"))
	&& (argcnt >= 3) && (*atoiv(argvec[2],&debugsok)))
	    printf("Bad socket, sorry.\n"), exit(-1);

/*  if(fork()) exit(0);     /* so we get adopted by init immediately */

    progname = argvec[0];
    setbuf(stdout, NULL);
	    signal(SIGHUP,  SIG_IGN);
    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIG_NETINT,  SIG_IGN);
    arg1p = argvec[1];
    close(2); dup(1);		  /* redirect error dev */
    printf("Telnet Server awaiting connection %s", Now());
    for (;;)
	{
#ifdef NCP
	openparams.o_type = SERVER;	 /* Server ICP */
	openparams.o_nomall = 1024; /* For block-mode terminals */
	openparams.o_fskt = 0;
	openparams.o_lskt = SERVER_SOCKET;
	openparams.o_host = 0;
	while ((netchan = open("/dev/net/ncp",&openparams)) < 0)
	    {
		printf("Can't open /dev/net/ncp: %s; %s", errmsg(0), Now());
		sleep(DAEMSLEEP);
	    }
#endif NCP
#ifdef TCP
	if(fork()) exit(0);	/* so tcb's show right owner */
	openparams.c_mode = CONTCP | ((debugsw)?CONDEBUG:0);
	openparams.c_sbufs = 2;
	openparams.c_rbufs = 1;
			/* server socket is also server port */
	openparams.c_lport = (debugsw?debugsok:SERVER_SOCKET);
	openparams.c_fport =
	  openparams.c_lo = openparams.c_hi = 0;
	mknetaddr(openparams.c_fcon, 0, 0, 0);
	mkanyhost(openparams.c_lcon);
	while ((netchan = open("/dev/net/tcp",&openparams)) < 0)
	    {
	        printf("can't open /dev/net/tcp: %s; %s",
		 errmsg(0), Now());
		sleep(DAEMSLEEP);
	     }
	ioctl(netchan, NETSETE, NULL);
#endif TCP
	if ((pid = fork())<0)
	    {
	    printf("No processes left! %s", Now());
	    }
	else if (pid==0)	  /* Child invokes netgrf */
	    {
	    netgrf();
	    exit(0);
	    }
	else			  /* Parent waits for child */
	    {
	    while(pid != wait(&junk));
	    }
	close(netchan);
	}
    }

/* -------------------------- N E T G R F --------------------------- */
/*
 * Fork. Parent exits immediately; child gets a pty, sets command line,
 * and forks again. Child is telxmt, parent is telrcv.
 */
netgrf()
{

    if ((pid=fork())>0)
	exit(0);		  /* Parent exits immediately so logger wait wakes up */
    else if (pid<0)
	{
	printf("No processes left %s", Now());
	write(netchan,"Sorry, not enough resources available.\r\n",41);
	exit(1);
	}
    if (openpty()<0)
	{
	printf("No ptys %s", Now());
	write(netchan,"Sorry, no ptys\r\n",16);
	exit(1);
	}
    setss();

    if ((pid = fork())<0)
	{
	printf("No processes %s", Now());
	write(netchan,"Sorry, no processes\r\n",21);
	exit(1);
	}
    if (pid==0)
	telxmt();	   /* Child handles net xmit side */
    else
	telrcv();	   /* Parent handles net receive side */
}


/* -------------------------- T E L R C V --------------------------------- */
/*
 * Read from net via telgetc; call telcmd on IAC-preceeded stuff,
 * send everything else to pty. On CRLF/CRNUL sequences, only the CR
 * is passed to pty.
 */
telrcv()
{
    unsigned char c;
    extern int BinaryOut;

    sleep(2);			  /* Give getty a chance to do stty */

    for(;;)
    {
	c = telgetc();		      /* Fetch a character */
	if (c==IAC)		      /* IAC? */
	    telcmd();			/* Yes, handle special command */
	else if (inter <= 0)
	{
	    write(ptyin, &c, 1);	  /* Otherwise, just enter it */
	    if (!BinaryOut && (c=='\r'))	      /* CR? */
	    {
		c = telgetc();		/* Yes, get next */
		if (c && c!='\n')	/* If not LF or NUL, write it anyway */
		    write(ptyin, &c, 1);
	    }
	}
    }
}

/* -------------------------- T E L C M D --------------------------- */
/*
 * Process New Telnet protocol. Reply to option negotiations.
 */
telcmd()
{
    unsigned char negbuf[3];
    unsigned char command;
    struct sgttyb status;	  /* Buffer for stty and gtty */
    static int amode;
    extern int BinaryIn, BinaryOut;
    command = telgetc();	  /* Get a character */
    switch (command)
	{
	case BREAK:
	case IP:
	    interrupt(ptyin);
	    break;
	case AYT:
	    write(netchan, "\007", 1);
	    break;
	case EC:
	case EL:
	    gtty(ptyin, &status);
	    write(ptyin, (command == EC) ? &status.sg_erase : &status.sg_kill, 1);
	    break;
#    ifdef TCP
	case DM:
	    inter = ChkUrgent();
	    break;
#    endif TCP

	case SB:		    /* Begin subnegotiation? */
/* Look for end of subnegotiation */
	    do
		{
		while(command != IAC)
		    command = telgetc();
		}
	    while (telgetc() != SE);
	    break;
	case WILL:
	case WONT:
	case DO:
	case DONT:

/* Negotiate. Only echo option implemented. */
/* and binary option.  jsq BBN 8/2/79 */

	    negbuf[0] = IAC;	      /* Interpete As Command */
	    negbuf[1] = DONT+WONT-((command+1)&0376); /* Negative ack. */
	    negbuf[2] = telgetc();    /* Set code for option */
	    if (negbuf[2] == 1 && (command == DO || command == DONT))  /* Echo option? */
		{
		gtty(ptyin,&status);
		if (command == DO)    /* User wants us to echo */
		    {
		    if (!logecho)	/* ignore login 'will echo' (getty does it for us) */
			status.sg_flags |= ECHO;
		    negbuf[1] = WILL;
		    }
		else		      /* User doesn't want us to echo */
		    {
			status.sg_flags &= ~ECHO;
			negbuf[1] = WONT;
		    }
		logecho = 0;
		stty(ptyin, &status);  /* Set desired echo mode */
		if (echoflag == 0)    /* Was this a response to our WILL ECHO? */
		    {
		    echoflag++; 	/* If so don't do anything */
			break;
		    }
		}

#define LEVMASK (~ECHO) /* everything but echo */

		if (negbuf[2] == 0) {	/* binary negotiation */
			gtty(ptyin,&status);
			if (command == DO) {
			    if ((BinaryIn + BinaryOut) == 0) {
				amode = status.sg_flags&LEVMASK;
				status.sg_flags = ((status.sg_flags & ~LEVMASK) | (LEVEL8|RAW));
			    }
			    BinaryIn = 1;
			    negbuf[1] = WILL;
			} else if (command == WILL) {
			    if ((BinaryIn + BinaryOut) == 0) {
				amode = status.sg_flags&LEVMASK;
				status.sg_flags = ((status.sg_flags & ~LEVMASK) | (LEVEL8|RAW));
			    }
			    BinaryOut = 1;
			    negbuf[1] = DO;
			} else if (command == DONT) {
			    if (BinaryIn && ((BinaryIn + BinaryOut) == 1))
				status.sg_flags = ((status.sg_flags & ~LEVMASK) | (amode & ~LEVEL8));
			    BinaryIn = 0;
			    negbuf[1] = WONT;
			} else {
			    if (BinaryOut && ((BinaryIn + BinaryOut) == 1))
				status.sg_flags = ((status.sg_flags & ~LEVMASK) | (amode & ~LEVEL8));
			    BinaryOut = 0;
			    negbuf[1] = DONT;
			}
			stty(ptyin, &status);
		}
	    write(netchan,negbuf,3);  /* Send off confirmation/denial to net */
	    break;
	case IAC:			    /* Not really a command */
	    write(ptyin, &command, 1);	      /* but a character */
	    break;
	}
    }

/* -------------------------- T E L X M T --------------------------- */
/*
 * Copy from pty to net. Double IACs.
 */
char ttybuf[NETBUFSIZE+1];	/* This size is best for NetUNIX */

telxmt()
{
    int bytcnt;
    register char *s1,*s2;

/* Send 'WILL ECHO' and a reminder */

    write(netchan,"\377\373\001\r\n",5);
    if (netchan > 2)
	printf("%s %s %s", progname, arg1p, Now());

/* Copy characters from output buffer to network */

    while((bytcnt = read(ptyout, ttybuf, sizeof(ttybuf))) >= 0)
	{
	s1 = ttybuf;
	s2 = s1;     /* Initialize pointers */
	for (;;)
	    {
	    while ((s1 < &ttybuf[bytcnt]) && (((*s1) & 0377) != IAC))
		s1++;	 /* Find end or IAC */
	    if (s1 < &ttybuf[bytcnt])
		{
		write(netchan, s2, s1 - s2 + 1);
		s2 = s1;
		s1++;
		}
	    else
		{
		write(netchan, s2, s1 - s2);
		break;
		}
	    }
	}
}

/* -------------------------- O P E N P T Y ------------------------- */

char *ptyname = "/dev/ptyx";	/* Where to look for pseudo-teletypes */

extern int errno;		/* Error code on failure */

int openpty()
{
    register char *s1;

/* Find end of device name */

    s1 = ptyname;
    while (*s1++)
	;
    s1 -= 2;

    *s1 = firstpty; /* Make name of first PTY */
    for (;;)	    /* Step through ptys looking for an unused one */
	{
	ptyin = open(ptyname, 2);  /* Try this PTY */
	ptyout = ptyin; 	   /* Holdover from pipe version */
	if (ptyin>=0)
	    {
	    pty = (*s1)&0177;
	    return(ptyin);
	    }
	if (errno==2)
	    return(-1);   /* No such file.  Assume end of PTYs */
	(*s1)++;
	}
}

/* -------------------------- S E T S S ----------------------------- */

char argstr[] = "ttyX ";

setss()
{
    register char *p, *q;
    struct NETPARAMS statparams;

    if ((q=arg1p) == 0)
	return;
    argstr[3] = pty;
    p = argstr;
    while (*p)
	{
	if (*q)
	    *q++ = *p++;
	else
	    return;
	}
#ifdef NCP
    if (fstat(netchan,&statparams) >= 0)
#endif NCP
#ifdef TCP
    if (ioctl(netchan,NETGETS,&statparams) >= 0)
#endif TCP
	{
#	ifdef NCP
	    p = hostname(statparams.o_host);
#	endif NCP
#	ifdef TCP
	    p = hostname(statparams.c_fcon);
#	endif TCP
	while (*p)
	    {
	    if (*q)
		*q++ = *p++;
	    else
		return;
	    }
	}
	*q = '\0';
}

/* -------------------------- T E L G E T C ------------------------- */

telgetc()
{
    unsigned char c;
    register int i;

#   ifdef NCP
    struct sgttyb ttytype;
    i = read(netchan,&c,1);
    if (inter)			/* check for interrupts */
    {
	inter = 0;		/* DM will be ignored */
	gtty(ptyout, &ttytype); /* return NUL if in raw mode, else DEL */
	return((ttytype.sg_flags & RAW) ? 0 : 0177);
    }
#   endif NCP
#   ifdef TCP
    while ((i=read(netchan,&c,1)) < 0)
    {
	extern errno;
	struct netstate stat;

	if (errno == ENETSTAT)   
	    if (ioctl (netchan, NETGETS, &stat) < 0 || 
	        (stat.n_state != URXTIMO && stat.n_state != UURGENT))
		{
		    i = 0;
		    break;
		}
    }
#   endif TCP

    if (i == 0)
    {
	kill(pid, 9);		/* Kill other side */
	exit(1);
    }

    return(c&0377);
}

/*--------------------------- I N S --------------------------------- */

ins()
{
    inter++;		    /* note that an interrupt has occurred */
    signal(SIG_NETINT, ins);
}

/* -------------------------- N O W --------------------------------- */

char *Now()
{
		long now;
		extern char *ctime();

		time(&now);
		return(ctime(&now));
}

/* -------------------------- I N T E R R U P T --------------------- */
/*
 * Send interrupt to process on other side of pty.
 * If it is in raw mode, just write NUL; otherwise, write intr char.
 * Intr char is DEL unless modtty implemented, in which case it is
 * whatever the user said it is.
 *  .. 5mar81 bpc .. Send proper interupt char even if not moddty (but
 *        V7)
 * 12nov81 ers Changed to ifdef on TIOCGETC, instead of on V7.
 */
interrupt(fd)
    int fd;
{
    struct sgttyb status;
#ifndef MODTTY
#   ifdef TIOCGETC
	struct tchars tchars;
#   endif
#endif
    char cint;
#ifdef MODTTY
    struct modes modbuf;
    int old12;
#endif

    gtty(fd, &status);
    if (status.sg_flags & RAW)
        write(fd, "\0", 1);
    else
    {
        cint = '\177';
#ifdef MODTTY
        old12 = signal(12, 1);  /* Don't want "bad system call" signal */
        if (modtty(fd, M_GET|M_MODES, &modbuf, sizeof(modbuf)) != -1)
            cint = modbuf.t_intr;
        signal(12, old12);
#else
#ifdef TIOCGETC
	if (ioctl (fd, TIOCGETC, &tchars) != -1)
	    cint = tchars.t_intrc;
#endif TIOCGETC
#endif MODTTY
        write(fd, &cint, 1);
    }
}
/*
 * ----------------------- C h k U r g e n t ------------------------
 *
 * called to check if connection is still in urgent state.
 *
 */

#ifdef TCP
ChkUrgent()
{
	struct netstate tstat;
	ioctl(netchan,NETGETS, &tstat);
	return(tstat.n_state & UURGENT);
 } 
#endif TCP
