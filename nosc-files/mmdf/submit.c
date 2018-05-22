#
/*
 *   MULTI-CHANNEL MEMO DISTRIBUTION FACILITY (MMDF)
 *
 *   Copyright (C) 1979  University of Delaware
 *
 *   This program and its listings may be copied freely by United States
 *   federal, state, and local government agencies and by not-for-profit
 *   institutions, after sending written notification to:
 *
 *      Professor David J. Farber
 *      Department of Electrical Engineering
 *      University of Delaware
 *      Newark, Delaware  19711
 *
 *          Telephone:  (302) 738-2405
 *
 *  Notification should include the name of the acquiring organization,
 *  name and contact information for the person responsible for maintaining
 *  the operating system, and license information if MMDF will be run on a
 *  Western Electric Unix(TM) operating system.
 *
 *  Others may obtain copies by arrangement.
 *
 *  The system was originally implemented by David H. Crocker, and the
 *  effort was supported in part by the University of Delaware and in part
 *  by contracts from the United States Department of the Army Readiness
 *  and Materiel Command and the General Systems Division of International
 *  Business Machines.  The system was built upon software initially
 *  developed by The Rand Corporation, under the sponsorship of the
 *  Information Processing Techniques Office of the Defense Advanced
 *  Research Projects Agency, and was developed with their cooperation.
 *
 *  The above statements must be retained with all copies of this program
 *  and may not be removed without the consent of the University of
 *  Delaware.
 **/
/* Fall, 78:  David H. Crocker: Completed initial coding                */
/* 15 Dec 78: dhc:  Try to stop auto-inclusion of aliases               */
/* 20 Dec 78: dhc:  Sender verification policy change: only local login */

#define VERBOSE 1     
#include "mailer.h"
#include "ffio.h"
#define FFAKE   0                 /* ff_read does no buffer stuffing  */
#define MAXNUM 077777

struct adrstruct    snoopadr;
#ifdef LOCDLVR
struct netstruct    net_loc;
#endif
#ifdef  ARPANET
struct netstruct    net_arpa;
#endif
#ifdef  POBOX
struct netstruct    net_pobox;
#endif
#ifdef  DIALSND
struct netstruct    net_dialsnd;
#endif

int
	userid,
	ownerid,
        lochostnum,		  /* local host number            */
/*            snoopnum,           ** host number of snoopbox      */
/*            sentprotect,          /* protection on files in mail  */
				  /*   directories                */
        highfd;			  /* # of fd's per process        */
extern int
            fout;		  /* for printf                   */

#define NUMFFBUFS   5
char    ffbufs[NUMFFBUFS][FF_BUF];
char
        chrcnv[],		  /* character conversion table   */
        locname[],		  /* local host name              */
        lhostr[],		  /* string version of lochostnum */
        nammailer[],		  /* name of mailer process       */
        prgmailer[],		  /* program name of mailer proc. */
        homeque[],		  /* directory containing mail    */
				  /*   queue directories          */
       *thefile,		  /* filename part of pathname    */
        aquedir[],		  /* name of directory where que- */
				  /*   ued mail addresses are put */
        tquedir[],		  /* temporary directory for      */
				  /*   aquedir file                */
        mquedir[],		  /* name of directory where que- */
				  /*   ued mail text is put       */
        snoopfile[],		  /* if exists, do snooping       */
       *logindir,		  /* path to initial work dir     */
				  /* Following are for gcread...  */
        nltrm[] "\n\377",
        colntrm[] ":\377",
        lsttrm[] ",\n\377",
        lwsptrm[] " \t\r\n\377",
        lstnultrm[] ",\n\000\377",
        nultrm[] "\000\377";

struct iobuf    tfilfd,
                mfilfd,
                cntlifd,
                logbuf;
struct adrstruct    adrstrt;

int     retloc,			  /* address for envret ()        */
       *echofd,			  /* handle on file for echochar  */
        passfd,			  /* handle on passwd file        */
        i,
        numacmpt,		  /* number of adrcmpt entries    */
        numadrs,		  /* current number of addresses  */
        msgflgs,		  /* in first line of taquedir    */
#define FLGFROM 1
#define OKFROM  2
	passfrom;                 /* whether to authenticate from */

char    earlyret,		  /* return from submit asap      */
        msgstat,		  /* status of proc'ing this msg  */
        msgend,			  /* null or eof encountered      */
        eofhit,			  /* to terminate program         */
        dbgmode,		  /* more verbosity               */
        domsg,
        snoopcnt,		  /* only needed at Rand          */
        rtn2sndr,		  /* return addr = submitter     */
       *sourcetxt,		  /* add txt to source-info      */
        useprot
{
    TRUE
}      ,			  /* protocol vs. parm control    */
 /* */  snoopall,		  /* always snoop messages        */
        delv DELMAIL,		  /* which ftp command to use     */
        domailer,		  /* fork mailer for send?        */
        takadrs,		  /* will be given (partial) list */
        bldadrs,		  /* extract addrs from message   */
        watchit,		  /* user watch immediate sends?  */
	trustflg,                 /* trust the submitter?         */
       *vianet,                   /* coming from net x            */
        vrfyflg,		  /* signal each addr acceptance  */
        username[10],		  /* from passwd file             */
       *moreline,		  /* for cmpntin to pass back     */
        testline[LINESIZE],       /* for authentication           */
       *tfilnam,		  /* file name in tquedir         */
       *afilnam,		  /* file name in aquedir         */
       *mfilnam,		  /* file name in mquedir         */
       *adrcmpt[5],              /* components containing adrs   */
        sndloc 'q',		  /* whether to wait for local or */
        sndnet 'q',		  /*   network messages           */
        haveloc,		  /* actually have local addrs    */
        havenet,		  /* actually have net addrs.     */
       *filpref;		  /* text to precede file refs    */

struct adrstruct   *adrfree ();
/****************** ADDRLST INPUT ROUTINES  ***************** */

primeadrs (adrbuf)		  /* used by doaddrs              */
char   *adrbuf;
{				  /* we are being given these     */
    register int    tmp;
    register char  *strptr;

    if (((tmp = cntlin (strptr = adrbuf, lstnultrm)) <= 0) ||
	    (*strptr == '!' && strptr[1] == '\n') ||
				  /* terminate the list             */
	    msgend)		  /* this shouldn't happen          */
	return (NOTOK);		  /* eof                            */
    return (tmp);
}

struct iobuf   *infilfd;	  /* initialized by addrfil       */

filin (adrbuf)
char   *adrbuf;
{				  /* we are being given these     */
    register int    tmp;
    register char  *strptr;

    if ((tmp = gcread (infilfd, strptr = adrbuf, LINESIZE, lstnultrm)) <= 0)
	return (NOTOK);		  /* eof                            */
    return (tmp);
}

cmpntin (adrbuf)
char   *adrbuf;
{				  /* adrs extracted from message */
    static  morcmpnt;
    register int    tmp;
    register char  *strptr;

    if (((tmp = cntlin (strptr = adrbuf,
			(morcmpnt) ? ":,\n\000\377" : lstnultrm)) <= 0)
	    || msgend)
	return (NOTOK);		  /* eof                            */
    if (*strptr == '\0')
	return (NOTOK);		  /* end of headers                 */
    fwrite (&mfilfd, adrbuf, tmp);
    if (morcmpnt)
    {				  /* potential continuation line     */
	if (*strptr != ' ' && *strptr != '\t')
	{			  /* no such luck                    */
	    morcmpnt = FALSE;
	    strptr[tmp] = '\0';
	    moreline = strdup (strptr);
	    return (NOTOK);
	}
	for (; lwsp (*strptr); strptr++);
	if (strptr > adrbuf)
	    strcpy (strptr, adrbuf);
    }
    if (adrbuf[tmp - 1] == '\n')
	morcmpnt = TRUE;
    return (strlen (adrbuf));
}

/* MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN */

main (argc, argv)
int     argc;
char   *argv[];
{
    char   *argcptr;

    dbgmode = TRUE;
    dbglog ("Submit says hello.");
    pgminit (argc, argv);	  /* parse args, alloc buffers.   */
    userinit ();		  /* Get user's system name       */
    dirinit ();			  /* chdir into homeque           */

    for (envsave (&retloc); !eofhit;)
    {
	loopinit ();
	if (useprot && switchinit () < OK)
	    break;		  /* no commands from user        */
	queueinit ();		  /* set-up the files             */
	if (takadrs || !bldadrs)  /* if list being given,         */
	    adrlst (&primeadrs, TRUE);
				  /* acquire, canonicalize, store */
	if (msgend)
	    sysabrt (RPLSYN, "Input ended prior to processing text");
	stormsg ();		  /* queue & authenticage         */
	fflush (&mfilfd);
	close (mfilfd.fildes);    /* done with message text file  */
	storadrs ();		  /* put adr list into file       */
	fflush (&tfilfd);
	close (tfilfd.fildes);
	dbglog ("msg done");
	if (link (tfilnam, afilnam) == NOTOK)
	    sysabrt (RPLLOC, "unable to move file to mail queue");
	if (unlink (tfilnam) == NOTOK)
				  /* move to regular directory    */
	    sysabrt (RPLLOC, "unable to move file to mail queue");
	tfilfd.fildes = 0;
	mfilfd.fildes = 0;
	dbglog ("doing mailer?");
	send ();		  /* give the message a mailer    */
	if (!earlyret)
	    rplok ("Done");
	if (!eofhit)
	    alstfree ();
    }
    if (!eofhit)
	suckup ();
    dbglog ("normal exit");
    flush ();
    exit (msgstat / 10);
}
/* ***********************  UTILITIES  ***********************    */

char    echochar ()
{
    register char   c;

    if ((c = getc (&cntlifd)) > '\0')
    {
	if (putc (c, echofd) < 0)
	    sysabrt (RPLLOC, "output error");
    }
    else
    {
	msgend = TRUE;
	if (c == NOTOK)
	    eofhit = TRUE;
    }
    return (c);
}

cntlin (buffer, brkset)
char   *buffer;
char   *brkset;
{
    register int    tmp;
    register char  *strptr;
#ifdef VERBOSE
    dbglog ("cntlin ()");
#endif
    switch (tmp = gcread (&cntlifd, strptr = buffer, LINESIZE - 1, brkset))
    {
	case -1: 
	    sysabrt (RPLLOC, "Error reading data from control stream");
	case 0: 
	    dbglog ("\t EOF");
	    msgend = eofhit = TRUE;
	    break;
	default: 
	    if (strptr[tmp - 1] == '\0')
		msgend = TRUE;
	    else
		strptr[tmp] = '\0';
    }
#ifdef VERBOSE
    dbglog ("\treturning: \"%s\"", strptr);
#endif
    return (tmp);
}

char   *stralloc (numbytes)
int     numbytes;
{
    register char  *tmp;

    if ((tmp = alloc (numbytes)) == NOTOK)
	sysabrt ("No more storage available");
    return (tmp);
}
/* ********************  INITIALIZATIONS  ***********************    */

pgminit (argc, argv)
int     argc;
char   *argv[];
{
    int     tmp;

    for (tmp = 0; tmp < NUMFFBUFS; tmp++)
	ff_use (ffbufs[tmp]);
    for (tmp = 1; tmp < argc; tmp++)
    {
	if (*argv[tmp] == '-')
	{
	    useprot = FALSE;
	    parminit (&argv[tmp][1]);
	}
#ifdef ARPANET
	else
	    if (strequ (argv[tmp], "snoop"))
		snoopcnt = snoopall = TRUE;
#endif				  /* for use by FTP               */
    }

    fout = dup (1);		  /* force buffered output             */
    close (1);

 /* add directory part of pathname     */
    tfilnam = stralloc (strlen (&tquedir) + 15);
    afilnam = stralloc (strlen (&aquedir) + 15);
    mfilnam = stralloc (strlen (&mquedir) + 15);
    strcpy (&tquedir, tfilnam);
    thefile = tfilnam + strlen (&tquedir);
				  /* thefile = just file name            */
    strcpy (&mquedir, mfilnam);
    strcpy (&aquedir, afilnam);
}

userinit ()
{				  /* who is running me?                */
    int     tmp;
    char    uidstr[6];
    register char   c,
                   *charptr;

    if ((passfd = ff_open ("/etc/passwd", 0)) == NOTOK)
	sysabrt (RPLLOC, "Cannot open password file");

    for (userid = getuid (), ownerid = (userid >> 8) & 0377, userid =& 0377;
	    (tmp = ff_read (passfd, &username, sizeof username, colntrm)) > 0;
	    ff_read (passfd, FFAKE, MAXNUM, nltrm))
				  /* do our own getpw, in order        */
    {				  /* to use the ff_ routines           */
	if (tmp < 0)
	    sysabrt (RPLLOC, "Error reading password file");
	username[tmp - 1] = '\0';
	ff_read (passfd, FFAKE, MAXNUM, colntrm);
				  /* skip encrypted password           */
	uidstr[ff_read (passfd, &uidstr, sizeof uidstr, colntrm) - 1]
	    = '\0';		  /* get user number so                */
				  /* we know who you are...            */
	ff_read (passfd, FFAKE, MAXNUM, colntrm);
	ff_read (passfd, FFAKE, MAXNUM, colntrm);
				  /* skip group & GCOS info             */
				  /* get the initial working directory  */
	testline[ff_read (passfd, &testline, LINESIZE, colntrm) - 1] = '\0';
	logindir = strdup (testline);
	if (userid == atoi (&uidstr))
	    return (TRUE);
    }
    sysabrt (RPLLOC, "Your user id isn't in password file!");
}

dirinit ()
{
    filpref = gwdir ();		  /* save original working dir name       */
    if (chdir (homeque) == NOTOK) /* change wdir into mail queues         */
	sysabrt (RPLLOC, "Unable to change directory.");
    setuid (getuid () & 0377);    /* so we have the user's privileges     */
				  /*   to access address files            */
}

loopinit ()
{
    earlyret =
	haveloc =
	havenet =
	msgend = FALSE;
    msgstat = OK;
    snoopcnt = snoopall;	  /* if FTP always include snoop  */
    echofd = &mfilfd;
}

switchinit ()
{
    register **strptr;

    if (vianet)
	free (vianet);
    for (strptr = adrcmpt; *strptr; free (*strptr));

    numadrs =
	numacmpt =
	sourcetxt =
	msgflgs = 0;
    passfrom =
	takadrs =
	bldadrs =
	rtn2sndr =
	watchit =
	vrfyflg =
	vianet =
	domailer = FALSE;
    delv = DELMAIL;		  /* default to MAIL command      */
				  /* return full message          */
    sndloc =			  /* default is not to wait for   */
	sndnet = 'q';		  /*   any deliveries             */

    switch (cntlin (&testline, nltrm))
    {
	case NOTOK: 
	    msgabrt ("RPLPARM, Error reading switches");
	case 0: 
	    return (NOTOK);
    }
    parminit (&testline);
}

parminit (str)
char   *str;
{
    if (!parmset (str))
	msgabrt (RPLPARM, "Invalid parameter specification");

    if (!useprot)
    {
	domsg = TRUE;
	if (vrfyflg)
	    sysabrt (RPLPARM, "Verification only allowed in protocol mode");
    }
    if (trustflg)
	passfrom = (userid == 0 || userid == ownerid || vianet)
			? OKFROM : FLGFROM;
    if (watchit)
    {
	if (sndloc == 's')
	    sndloc = 'w';
	if (sndnet == 's')
	    sndnet = 'w';
    }
    if (!domailer)		  /* queueing is de facto             */
	sndloc = sndnet = 's';
}

parmset (parmlst)
char   *parmlst;
{
    char   *strtptr;
    char tchar;
    register int    ind;
    register char  *ptr;


    for (ptr = parmlst;;)
	switch (*ptr++)		  /* read until end of list       */
	{
	    case '\n': 
	    case '\0': 		  /* end (shouldn't happen)       */
		return (TRUE);
	    case ' ': 
	    case '\t': 
		continue;
	    case 'a': 
		delv = DELTTY;    /* alert, thru xsem command     */
		break;
	    case 'c': 
		msgflgs =| RETCITE;
				  /* only cite return mail, don't */
		break;		  /*   send full copy             */
	    case 'd': 
		dbgmode = TRUE;
		break;
	    case 'f': 
		switch (*(ptr = getparm (strtptr = ptr, &sourcetxt)))
		{
		    case ',': 
		    case '*': 
			ptr++;
		}
		break;		  /* added to Source-Info field   */
	    case 'g': 		  /* 'x' [name *(',' name)] '*'   */
		takadrs = TRUE;   /* extract addrs from cmpnts    */
	    case 'i': 		  /* indirection "via" network    */
		switch (*(ptr = getparm (strtptr = ptr, &vianet)))
		{case ',':
		 case '*':
		    ptr++;
		}
		break;
	    case 'l': 
		sndloc = 's';     /* for local mail,              */
		domailer = TRUE;  /* fork a mailer                */
		break;
	    case 'm': 
		delv = DELMAIL;   /* use ftp mail command         */
		break;
	    case 'n': 
		sndnet = 's';     /* for "net" mail,              */
		domailer = TRUE;  /* fork a mailer                */
		break;
	    case 'r': 
		rtn2sndr = TRUE;  /* don't read it from input     */
		break;
	    case 's': 
		delv = DELBOTH;   /* use ftp xsen command         */
		break;
	    case 't': 		  /* trust me; assumed authentic  */
				  /* must be root                 */
		trustflg = TRUE;
		break;
	    case 'u': 		  /* no verify, no trust          */
		passfrom = FLGFROM; /* include disclaimer           */
		break;
	    case 'v': 
		vrfyflg = TRUE;   /* Certify each address           */
		break;
	    case 'w': 
		watchit = TRUE;   /* user wants to watch delivery */
		break;
	    case 'x': 		  /* 'x' [name *(',' name)] '*'   */
		bldadrs = TRUE;   /* extract addrs from cmpnts    */
		while (numacmpt < 10)
		{		  /* get component name           */
		    switch (*(ptr = getparm (strtptr = ptr,
						&(adrcmpt[numacmpt++]))))
		    {
			case ',':
			    ptr++;
			    continue;
			case '*':
			    ptr++;
			default:
			    break;
		    }
		    break;
		}
		adrcmpt[numacmpt] = 0;
		break;
	}
    return (FALSE);
}

getparm (pstrtadr, strptr)
char   *pstrtadr;
char **strptr;                   /* where to put dup'd str */
{
    int     retval;
    register char tchar;
    register char  *strtadr;

    for (strtadr = pstrtadr;; strtadr++)
	switch (*strtadr)
	{
	    case '*': 
	    case ',': 
	    case '\0': 
	    case '\n': 
		tchar = *strtadr;
		*strtadr = '\0';
		*strptr = strdup (pstrtadr);
		*strtadr = tchar;
		return (strtadr);   /* c'est tout   */
	}
}

queueinit ()
{
    long int    ltmp;
    int     tmp;
    char    longstr[11];	  /* for creation time            */

    crname (thefile);		  /* filename part into afilnam   */
    strcpy (thefile, &(mfilnam[strlen (&mquedir)]));
    strcpy (thefile, &(afilnam[strlen (&aquedir)]));
/*  dbglog ("tname = \"%s\"", tfilnam);         */
				  /* create file in temp directory */
    flush ();
    if (fcreat (tfilnam, &tfilfd) < 0)
	sysabrt (RPLLOC, "Can't create address file to be queued.");
				  /* strictly sequential output   */

    time (&ltmp);		  /* Initialize contents          */
    litoa (ltmp, &longstr);       /* 1. Full creation time string */
    fwrite (&tfilfd, &longstr, strlen (&longstr));
    putc ('/', &tfilfd);	  /* 2. Delivery status           */
    itoa (msgflgs, &longstr);
    fwrite (&tfilfd, &longstr, strlen (&longstr));
				  /* 3. Flags: only retcite, now  */
    putc ('\n', &tfilfd);	  /* 4. New line...               */
    if (rtn2sndr)		  /* 5. return address            */
    {
	fwrite (&tfilfd, username, strlen (username));
	putc ('\n', &tfilfd);
    }
    else
	for (echofd = &tfilfd; echochar () != '\n';);
    echofd = &mfilfd;
}
/* ************  ADDRESS LINKED-LIST PRIMITIVES  *************** */

addadr (padr)
struct adrstruct   *padr;
{
    char   *mbox;
    register struct adrstruct  *adr,
                               *curadr,
                               *lastptr;
/*  dbglog ("addadr ()");       */
    for (adr = padr, curadr = adrstrt.nxtadr, lastptr = &adrstrt;
	    curadr != 0; lastptr = curadr, curadr = curadr -> nxtadr)
    {
	switch (adrcmpr (adr, curadr))
	{
	    case 1: 
		continue;
	    case 0: 		  /* duplicate entry              */
		return;
	}
	break;			  /* => adrcmpr returned NOTOK       */
    }
    numadrs++;
    lastptr -> nxtadr = adr;
    adr -> nxtadr = curadr;
}

storadrs ()
{
    char   *snooptr,
           *tmpadr;
    register struct adrstruct  *curadr;
    register int    tmp;

    dbglog ("storadrs:  %d addrs...", numadrs);

#ifdef SNOOP
    if (snoopcnt && (snoopadr.dlv_code != '\0') && !access (snoopfile, 0))
    {
	for (tmpadr = stralloc (tmp = sizeof *curadr),
		snooptr = &snoopadr; tmp >= 0; tmp--)
	    tmpadr[tmp] = snooptr[tmp];
	addadr (tmpadr);
    }
#endif

    for (curadr = adrstrt.nxtadr; curadr != 0; curadr = curadr -> nxtadr)
	adrstor (curadr);
}

alstfree ()
{
    register struct adrstruct  *curadr;

    for (curadr = adrstrt.nxtadr; curadr != 0; curadr = adrfree (curadr));
    adrstrt.nxtadr = 0;
    numadrs = 0;
}

struct adrstruct
                   *adrfree (padr)
struct adrstruct   *padr;
{
    register struct adrstruct  *curadr,
                               *nxtadr;

    if ((curadr = padr) == 0)
	return (0);
    if (curadr -> smbox != 0)
	free (curadr -> smbox);
    if (curadr -> shostr != 0)
	free (curadr -> shostr);
    nxtadr = curadr -> nxtadr;
    free (curadr);
    return (nxtadr);
}

/* ****************  ADDRESS PARSING ROUTINES  **************** */

adrlst (inproc, errabrt)
int     (*inproc) (),		  /* process to acquire adr strs;   */
				  /* it decides when to end stream  */
        errabrt;		  /* abort on error?                */
{				  /* process stream of addresses    */
    struct adrstruct   *tstadr;
    int     adrlen;
    char    adrline[LINESIZE],
           *strtptr;
    register char  *strptr;
/*  dbglog ("adrlst ()");       */
    for EVER
    {
	zero (adrline, LINESIZE);
	if ((adrlen = ((*inproc) (strptr = &adrline))) <= 0)
	    break;
	strptr[adrlen - 1] = '\0';
	if (!rmcmnt (strptr, strptr) && errabrt)
		if (vrfyflg)
			procerr (RPLUSR, "Bad comment \"%s\"", strptr);
		else
			msgabrt (RPLUSR, "Bad comment \"%s\"", strptr);
	compress (strptr, strptr);/* strip lead, trail & extra space  */
	switch (*strptr)
	{
	    case '\0': 		  /* ignore blank lines          */
		continue;
	    case '<': 		  /* indirect through a file      */
		for (strptr++; lwsp (*strptr); strptr++);
				  /* skip leading white space      */
		addrfil (strptr, errabrt);
		continue;
	}
	if (!chkadr (strtptr = strptr) && errabrt)
	{
	    strptr = &adrline[LINESIZE - 1];
	    *strptr = '\0';
	    while (strptr >= &adrline && *strptr-- == '\0');
	    for (; strptr >= &adrline; strptr--)
		if (*strptr == '\0')
		    *strptr = ' ';
	    if (vrfyflg)
		procerr (RPLUSR, "Unknown address \"%s\"", strtptr);
	    else
		msgabrt (RPLUSR, "Unknown address \"%s\"", strtptr);
	}
	else
	    if (vrfyflg && errabrt)
		rplok (strtptr);
    }
}

addrfil (strptr, errabrt)
char   *strptr;
int     errabrt;
{
    char    filename[LINESIZE];

    dbglog ("addrfil ()");
    strcpy (strptr, strcpy ((*strptr == '/') ? "" : filpref, filename));
    infilfd = stralloc (sizeof *infilfd);
    zero (infilfd, sizeof *infilfd);
    if (fopen (filename, infilfd) < OK)
	msgabrt (RPLUSR, "Can't get addresses from file %s", filename);
    adrlst (&filin, errabrt);     /* file inherits abort-ability   */
    close (infilfd -> fildes);
    free (infilfd);
    return (OK);
}

chkadr (tstline)
char   *tstline;
{
    register struct adrstruct  *tstadr;
    register char  *ptr;
    int mboxlen;
    char    netcode,
            hostr[LINESIZE],
            adrstr[LINESIZE];

#ifdef VERBOSE
    dbglog ("chkadr (%s)", tstline);
#endif
				  /* DBG HANDLE NETWORK NAME */
    for (ptr = tstline; lwsp (*ptr); ptr++);
    if (ptr > tstline)		  /* delete leading lwspaces */
	strcpy (ptr, tstline);
/*  netcode = NOTOK;*/
    if ((ptr = ghostpart (tstline, mboxlen = strlen (tstline), hostr))
	    == NOTOK)
	netcode = LOCDLVR;
    else                          /* net was explicitly referenced       */
    {
	if ((netcode = net2code (hostr)) >= OK)
	    ptr = ghostpart (tstline, ptr - tstline, hostr);
	while (ptr > tstline && lwsp (*ptr))
	    ptr--;
	mboxlen = ptr - tstline + 1;    /* length of mbox str   */
    }
    switch (netcode)
    {
	default: 		  /* no net reference                   */
#ifdef ARPANET
	case ARPANET: 		  /* At least it THINKS it's a hostname */
	    if (host2first (&net_arpa, TRUE, hostr, adrstr) ||
		    digit (adrstr[0]))
	    {			  /* A "remote" host               */
		if (!strequ (locname, adrstr))
		{
		    strcpy (adrstr, hostr);
		    netcode = ARPANET;
		    havenet = TRUE;
		    break;
		}
		netcode = LOCDLVR;
	    }
#endif
#ifdef POBOX
	case POBOX: 
	  if (netcode==NOTOK)
	    if (host2adr (&net_pobox, TRUE, hostr, adrstr)) {
		if (!strequ( locname, adrstr))
			{	  /* A "remote" host               */
			netcode = POBOX;
			havenet = TRUE;
			strcpy (adrstr, hostr);
			break;
			}
		netcode = LOCDLVR;
	    }
#endif
#ifdef DIALSND
	case DIALSND: 
	  if (netcode==NOTOK)
	    if (host2adr (&net_dialsnd, TRUE, hostr, adrstr)) {
		if (!strequ( locname, adrstr))
		    {	  /* A "remote" host               */
		    netcode = DIALSND;
		    havenet = TRUE;
		    strcpy (adrstr, hostr);
		    break;
		    }
		netcode = LOCDLVR;
	    }
#endif
#ifdef LOCDLVR
	case LOCDLVR: 
	    if (netcode == LOCDLVR)
	    {			  /* no hostname => lochost */
		blt (tstline, adrstr, mboxlen);
		adrstr[mboxlen] = '\0';
		if (aliasrch (adrstr))
		{		  /* local host -> local mbox      */
		    dbglog ("alias = %s", tstline);
		    return (TRUE);/* if ok, it's been added        */
		}
		if (passrch (adrstr))
		{
		    hostr[0] = '\0';
		    netcode = LOCDLVR;
		    haveloc = TRUE;
		    break;
		}
		netcode = NOTOK;
		break;
	    }
#endif
    }
    if (netcode == NOTOK)
	return (FALSE);

    tstadr = stralloc (sizeof *tstadr);
    tstadr -> dlv_code = netcode;
    tstadr -> shostr = (netcode == LOCDLVR) ? 0 : strdup (hostr);
    tstadr -> smbox = ptr = stralloc (mboxlen + 1);
    blt (tstline, ptr, mboxlen);
    ptr[mboxlen] = '\0';
    addadr (tstadr);		  /* ok, by this point             */

#ifdef SNOOP
    chksnoop (tstadr);
#endif
    return (TRUE);
}

ghostpart (adrline, adrlen, destr)
char   *adrline;
int     adrlen;
char *destr;            /* put "hostname" into here     */
{
    register char  *strptr;
    int     tmp;
    char   *hostptr;
    char   *endptr;

/*  dbglog ("ghostpart ()");    */
    for (strptr = &(adrline[adrlen - 1]); lwsp(*strptr); strptr--);
    for (endptr = strptr; ; hostptr = strptr--)
    {
	if (strptr <= adrline)
	    return (NOTOK);
	switch (*strptr)
	{
	    default:
		continue;
	    case '@':
	    case '%':
		break;
	    case ' ':
	    case '\t':
		for (strptr--; (strptr > adrline) && lwsp (*strptr);
			strptr--);
		switch (*strptr)          /* got one now                  */
		{
		    case '@':
		    case '%':
			break;
		    default:              /* " at "??                     */
			if (equal (&strptr[-1], "AT", 2))
			    strptr--;
			else              /* tsk tsk                      */
			    return (NOTOK);
		}
		break;
	}
	break;
    }

    if (destr)
    {
	blt (hostptr, destr, tmp = (endptr - hostptr + 1));
	destr[tmp] = '\0';
	compress (destr, destr);
    }
    return (&strptr[-1]);
}

rmcmnt( from, to) /* remove comments from an address string */
 register char *from, *to;
 { int inlit, incom;
 inlit = FALSE;
 incom = 0;
 for (;;from++)
  { switch (*from)
   {
   case '\0': /* end of the string */
    *to = *from;
    return( inlit || (incom != 0) ? FALSE : TRUE );
   case QUOTE: /* next char is quoted */
    if (incom > 0) /* in a comment */
     { from++; continue;}
    else /* to string must get both chars */
     { *to++ = *from++; break;}
   case '"':
    inlit = (inlit ? FALSE : TRUE);
    break;
   case LCMNTDLIM: /* left comment delimiter */
    if (!inlit)
     {
     incom++;
     continue;
     }
    else break;
   case RCMNTDLIM: /* right comment delimiter */
    if (!inlit)
     if (incom > 0)
      { incom--;
      if (incom == 0) *to++ = ' ';
      continue;
      }
     else
      { for (; (*to++ = *from) != '\0'; *from++) ; /* copy rest of string */
      return(FALSE); /* bad comment nesting */
      }
   }
  if ( incom == 0) *to++ = *from;
  }
 }

net2code (str)
char   *str;
{
    extern struct netstruct *nets[];
    register struct netstruct **netptr;

    for (netptr = nets; *netptr != 0; netptr++)
	if (strequ (str, (*netptr) -> net_spec))
	    return ((*netptr) -> net_code);

    return (NOTOK);
}

code2net (code)
char    code;
{
    extern struct netstruct *nets[];
    register struct netstruct **netptr;

    for (netptr = nets; *netptr != 0; netptr++)
	if (code == (*netptr) -> net_code)
	    return (*netptr);     /* handle on entire entry       */

    return (NOTOK);
}

#ifdef SNOOP
chksnoop (tstadr)
struct adrstruct   *tstadr;
{				  /* Policy for snooping          */
    if ((tstadr -> shnum ^ lochostnum) & 077)
				  /* Same IMP as us?               */
	snoopcnt = TRUE;	  /* if not, big brother is watching */
}
#endif

struct aliastruct		  /* previous aposinfo's are stored     */
{				  /* on aliasrch's stack when aliasrch  */
    char   *abufpos;		  /* curr pos in aliasbuf          */
    char    aliasbuf[LINESIZE];   /* adr-part of alias entry              */
}                  *curalias;

aliasin (buf)
char   *buf;			  /* output buffer            */
{
    register char  *toptr,
                   *fromptr;

    if (*(fromptr = (curalias -> abufpos)) == '\0')
	return (NOTOK);		  /* end of list  */

    for (toptr = buf;; fromptr++, toptr++)
	switch (*toptr = *fromptr)
	{
	    case ',': 
	    case '\n': 
	    case '\0': 
		*toptr = '\0';
		curalias -> abufpos = fromptr;
		return (toptr - buf + 1);
	}
}

aliasrch (mbox)
char   *mbox;
{
    struct aliastruct  *oldalias,
                        newalias;
    long int    oldpos;
    int     retval;

    oldpos = (curalias == 0) ? 0 : ff_pos (net_loc.net_tabfd);
    if (host2adr (&net_loc, FALSE, mbox,
	    newalias.abufpos = &newalias.aliasbuf))
    {				  /* recurse, using rest of alias line      */
	oldalias = curalias;
	curalias = &newalias;
	adrlst (&aliasin, FALSE);
	curalias = oldalias;
	retval = TRUE;
    }
    else
	retval = FALSE;
    ff_seek (net_loc.net_tabfd, oldpos);
    return (retval);
}

passrch (pname)
char   *pname;
{
    int     tmp;
    char    user[20];
    register char  *name;

    ff_seek (passfd, 0, 0);
    name = pname;
    while ((tmp = ff_read (passfd, &user, sizeof user, colntrm)) > 0)
    {
	user[tmp - 1] = '\0';
	if (strequ (name, user))  /* does name match?             */
	    return (TRUE);	  /*   then return success        */
	else			  /* skip rest of line            */
	    if ((tmp = ff_read (passfd, FFAKE, MAXNUM, nltrm)) < 0)
		break;		  /* shouldn't happen...          */
    }
    if (tmp < 0)
	sysabrt (RPLLOC, "Error reading during search of password file");
    return (FALSE);		  /* return failure               */
}

adrcmpr (padr1, padr2)
struct adrstruct   *padr1,
                   *padr2;
{
    register int    tmp;
    register struct adrstruct  *adr1,
                               *adr2;

    if ((tmp = ((adr1 = padr1) -> dlv_code - (adr2 = padr2) -> dlv_code))
	    < 0)
	return (NOTOK);
    if (tmp > 0)
	return (1);
#ifdef NVRCOMPIL
    if ((tmp = adr1 -> shnum) > 0)
    {				  /* host ref is numeric          */
	if ((tmp = (tmp - adr2 -> shnum)) < 0)
	    return (NOTOK);
	if (tmp > 0)
	    return (1);
    }
    else			  /* have to do char compare      */
#endif
	switch (lexrel (adr1 -> shostr, adr2 -> shostr))
	{
	    case 1: 
		return (1);
	    case NOTOK: 
		return (NOTOK);
	}
    return (lexrel (adr1 -> smbox, adr2 -> smbox));
}

adrstor (padr)
struct adrstruct   *padr;
{
    register struct adrstruct  *curadr;
    char    anum[7];

    curadr = padr;

/*  dbglog ("%c %8s/%s",
	    curadr -> dlv_code, curadr -> shostr, curadr -> smbox);     */

    putc (curadr -> dlv_code, &tfilfd);
    putc ((curadr -> dlv_code == LOCDLVR)
	    ? sndloc : sndnet, &tfilfd);
    putc (delv, &tfilfd);
    if (curadr -> dlv_code != LOCDLVR)
    {
#ifdef NVRCOMPIL
	if (curadr -> shnum <= 0)
#endif
	    fwrite (&tfilfd, curadr -> shostr, strlen (curadr -> shostr));
#ifdef NVRCOMPIL
	else
	{			  /* nope...                  */
	    itoa (curadr -> shnum, &anum);
	    fwrite (&tfilfd, &anum, strlen (&anum));
	}
#endif
	putc (' ', &tfilfd);      /* separate host ref & mailbox  */
    }
    fwrite (&tfilfd, curadr -> smbox, strlen (curadr -> smbox));
    putc ('\n', &tfilfd);
}

/* ******************  MESSAGE TRANSFER ROUTINES  ********************* */

stormsg ()
{
    int     tmp;
    char    gotsender,
            gotfrom,
	    authorok;
    register char   *tstptr;

    gotsender =
	gotfrom =
	authorok = FALSE;

    dbglog ("stormsg ()");
    if (fcreat (mfilnam, &mfilfd) < 0)
	sysabrt (RPLLOC, "Can't create text file to be queued.");

    if (passfrom)
	authorok = gotfrom = gotsender = TRUE;
    if (passfrom == FLGFROM || sourcetxt != 0)
	dosrcinfo ();
    if (vianet)
	switch (vianet[0])
	{
#ifdef ARPANET
	    case ARPANET:
		dobody ();      /* treat whole message as body  */
		return;
#endif
#ifdef POBOX
	    case POBOX:
		dovia ();
#endif
	}
    while (!msgend && (tmp = cntlin (&testline, ":\n\000\377")) > 0
	    && testline[0] != '\0')
    {
#ifdef VERBOSE
	dbglog ("\tdid cmpnt name input");
#endif
	if (testline[tmp - 1] == '\0')
	    tmp--;
	fwrite (&mfilfd, &testline, tmp);
tstfld: 
	switch (testline[0])
	{
	    case ':': 
		msgabrt (RPLSYN, "Component without a name");
	    case '\n': 		  /* "empty" component => end of headers  */
		dobody (gotsender, gotfrom, authorok);
		return;
	    case ' ': 		  /* continuation line                    */
	    case '\t': 
		cntline2mfil (tmp);
		continue;
	}
	if (testline[tmp - 1] != ':')
	    msgabrt (RPLSYN, "No component name terminator");
	testline[tmp - 1] = '\0';
	compress (testline, testline);
				  /* remove extra lwsp        */
	if (strequ ("Sender", &testline) && !passfrom)
	{   if (gotsender++ > 0)
		msgabrt (RPLSYN, "Only one Sender component allowed");
	    if (!(authorok = chksndr ()))
		msgabrt (RPLSYN, "Invalid specification in Sender component");
	    continue;
	}

	if (strequ ("From", &testline))
	{   dbglog ("doing From");
	    tmp = chksndr ();
	    gotfrom++;
	    if (!gotsender)
		authorok = tmp;
	    if (vianet)
		doviareply ();
	    continue;
	}
	if (bldadrs && numacmpt > 0 && isadrcmpt (&testline))
	{   adrlst (&cmpntin, TRUE);
	    if (moreline != 0)
	    {   strcpy (moreline, testline);
		tmp = strlen (&testline);
		free (moreline);
		moreline = 0;
		goto tstfld;      /* "stuff" the input queue   */
	    }
	    break;
	}
	cntline2mfil (0);
    }
    dbglog ("doing Body");
    if (gotsender > 0 && gotfrom == 0)
	msgabrt (RPLSYN,
		"A From component must be present when a Sender one is used");
    if (!authorok)
	msgabrt (RPLSYN, "No valid author specification present");
    if (!msgend)		  /* process the body             */
	dobody ();
}

dosrcinfo ()
{
    puts ("Source-Info: ", &mfilfd);
    if (passfrom == FLGFROM)
    {
	strcpy ("From (and/or Sender) name not authenticated.\n",
		testline);
	fwrite (&mfilfd, testline, strlen (testline));
	if (sourcetxt != 0)   /* pad for second line of text  */
	    puts ("     ", &mfilfd);
    }
    if (sourcetxt != 0)
    {
	fwrite (&mfilfd, sourcetxt, strlen (sourcetxt));
	if (sourcetxt[strlen (sourcetxt) - 1] != '\n')
	    putc ('\n', &mfilfd);
    }
}

dovia ()
{
    struct netstruct *netptr;
    char srchost[LINESIZE];

    netptr = code2net (vianet[0]);
    host2first (netptr, TRUE, &vianet[1], srchost);

/* Via: sourcehost to local-host use sourcehost%originating-net */
/* Via: <UDel-EE to Rand-Unix use UDel-EE%POBox>                */

    puts ("Via: <", &mfilfd);
    puts (srchost, &mfilfd);
    puts (" to ", &mfilfd);
    puts (locname, &mfilfd);
    puts (" use ", &mfilfd);
    puts (srchost, &mfilfd);
    putc ('%', &mfilfd);
    puts (netptr -> net_spec, &mfilfd);
    putc ('>', &mfilfd);
    putc ('\n', &mfilfd);
}

cntline2mfil (curlast)
    int curlast;
{
    register int tmp;

    for (tmp = curlast; !msgend && testline[tmp - 1] != '\n';
	fwrite (&mfilfd, &testline, tmp))
    {
#ifdef VERBOSE
	dbglog ("rest of line:");
#endif
	tmp = cntlin (&testline, nltrm);
    }
}

doviareply ()
{
    register char *ptr;

    dbglog ("Doing Reply-to");
    puts ("Reply-to: ", &mfilfd);
    for (ptr = testline; lwsp(*ptr); ptr++);
    for EVER
    {   switch (*ptr)
	{ case  '\0':
	  case  ' ':
	  case  '\t':
	  case  '@':
	    break;
	  default:
	    putc (*ptr, &mfilfd);
	    ptr++ ;
	    continue;
	}
	break;
    }
/*    puts (testline, &mfilfd);                 */
/*    if (gtsrcname (vianet, testline))         */
/*    {                                         */
/*        putc ('%', &mfilfd);                  */
/*        puts (testline, &mfilfd);             */
/*    }                                         */
    puts (" at ", &mfilfd);
    puts (locname, &mfilfd);
    putc ('\n', &mfilfd);
}

dobody (gotsender, gotfrom, authorok)
    char gotsender,
	gotfrom,
	authorok;
{
    register int bodlen;
    register char c;


    for (; !msgend && (bodlen = cntlin (&testline, nultrm)) > 0;
		fwrite (&mfilfd, &testline, bodlen))
    {
	    if ((c = testline[bodlen - 1]) == '\0' && bodlen > 1)
		c = testline[--bodlen - 1];
    }
	if (c != '\n')		  /* msgs must end with newline           */
	    putc ('\n', &mfilfd);
}

isadrcmpt (name)
char   *name;
{
    register int    entry;

    for (entry = 0; entry < numacmpt && adrcmpt[entry]; entry++)
	if (strequ (name, adrcmpt[entry]))
	    return (TRUE);
    return (FALSE);
}

gtsrcname (pvianet, obuf)
char    *pvianet,
       *obuf;
{
    struct netstruct   *netptr;
    char   *ptr;

    netptr = code2net (*pvianet);   /* get net info         */
    host2first (netptr, TRUE, &pvianet[1], obuf);
    for (ptr = obuf; *ptr; ptr++);
    *ptr++ = '%';
    strcpy (netptr -> net_spec, ptr);
				  /* Net name     */
    return (TRUE);
}

/* *******************  AUTHENTICATE SENDER  ************************ */

chksndr ()
{
    char hostr[LINESIZE];
    char mbox[LINESIZE];
    char   *ptr;
    register struct adrstruct  *tstadr;
    register int    tmp;

    dbglog ("chksndr: ");
    if ((tmp = cntlin (&testline, nltrm)) <= 0)
	return (FALSE);
    fwrite (&mfilfd, &testline, tmp);
    testline[--tmp] = '\0';
    if ((ptr = ghostpart (&testline, tmp, hostr)) == NOTOK)
	return (NOTOK);
    blt (testline, mbox, ptr - testline);
    mbox[ptr -testline] = '\0';
    compress (mbox, mbox);
    if (!strequ (hostr, locname) || !passrch (&mbox))
	return (FALSE);           /* bad nost or unknown user name        */
    return (TRUE);
}

/* ************************  FORK MAILER  ***************************   */

#define FRKEXEC 1
#define SPNEXEC 2
#define FRKWAIT 1
#define FRKPIGO 2
#define FRKCIGO 4
#define EXECR   8		  /* Use execr, instead of execv          */

send ()
{
    register char   c,
                   *q;
    register int    host;	  /* destination host, for snoop  */
    int     proctyp,
            dowait,
            retval;
    extern int  regfdary[];

    proctyp =
	dowait = 0;

    if (watchit)		  /* wait for fork                */
    {
	proctyp = FRKEXEC;
	dowait = FRKWAIT | FRKPIGO;
    }
    else			  /* otherwise let it run by      */
    {
	rplok ("Submited");
	regfdary[1] = NOTOK;      /* won't be outputting to caller      */
	earlyret = TRUE;	  /* as soon as possible          */
	if (sndloc == 's' || sndnet == 's')
	    proctyp = SPNEXEC;
	else
	    return;		  /* no forking at all            */
    }

    if (!domailer ||
	    ((!haveloc || sndloc == 'q') && (!havenet || sndnet == 'q')))
	return;
    regfdary[1] = fout;
    retval = newpgml (proctyp, dowait | FRKCIGO, &regfdary,
	    &prgmailer, &nammailer, thefile, "-d", (watchit) ? "-w" : 0, 0);

/*    if ((dowait & FRKWAIT) && (retval >> 8 == NOTOK))                 */
/*        procerr ("?unable to invoke mailer; retval = %o", retval);     */
}
/* **********************   UTILITY ROUTINES  ************************* */

suckup ()
{
    if (!msgend)
	while (gcread (&cntlifd, FFAKE, MAXNUM, nultrm) > 0);
}

/*   Print the error indicated in the cerror cell                       */

int     errno,
        sys_nerr;
char   *sys_errlist[];


inexit (errchar)		  /* exit if input from           */
char    errchar;		  /*   communications pipe error  */
{
    int     errcode;

    sysabrt (RPLXFER, "input error: code = %d", errcode = errchar);
}

scram (c)
char    c;
{
/*  unlink (tfilnam);       DBG */
    inexit (c);
}

clrfiles ()
{
    if (tfilfd.fildes > 0)
    {
	close (tfilfd.fildes);
	unlink (tfilnam);
	unlink (afilnam);
    }
    if (mfilfd.fildes > 0)
    {
	close (mfilfd.fildes);
	unlink (mfilnam);
    }
    flush ();
/*    if (useprot || vrfyflg)        */
/*        write (fout, "/\n", 2);    ** signal error completion    */
}

rplok (str)
char   *str;
{
    if (!(useprot || vrfyflg))
	return;
    putchar ('0');
/*  printf ("%3d", RPLOK);                      */
    if (str != 0)
	printf (" %s", str);
    putchar ('\n');
    flush ();
}

sysabrt (code, a, b, c, d, e, f)
int     code;
char   *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    if (envsave (&retloc))      /* have msgabrt return to here. */
	msgabrt (code, a, b, c, d, e, f);
    exit (code / 10);
}

msgabrt (code, a, b, c, d, e, f)
int     code;
char   *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    msgstat = code;
    procerr (code, a, b, c, d);   /* includes "nack" flag         */
    suckup ();
    clrfiles ();
    if (useprot)
	envrest (&retloc);
    exit (code / 10);
}

procerr (code, a, b, c, d, e, f)
int     code;
char   *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    char    doerrno;

    doerrno = FALSE;
    log ("*** Submit error:");
    log (a, b, c, d, e, f);
    if (errno > 0 && errno < sys_nerr)
    {
	doerrno = TRUE;
	log ("\t[ %s ]", sys_errlist[errno]);
    }

    if (earlyret)
	return;

    if (useprot)
	putchar ('/');		  /* DGB keep same interface, switch to code */
/*      printf ("%3d ", code);      when other pgms converted           */
    else
	printf ("Submit: ");
    printf (a, b, c, d, e, f);
    if (doerrno)
	printf (" [ %s ]", sys_errlist[errno]);
    putchar ('\n');
    flush ();
}
