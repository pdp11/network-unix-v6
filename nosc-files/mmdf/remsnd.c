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
#include "./dialdir/rmerr.h"
#include "mailer.h"
#define VERBOSE 1     
#define DL_TRIES 2
extern struct adr   adr;
extern int  domsg;
extern int  errno;
extern char
	locname[],
	remslalog[],
	remmaslog[],
	script[],
	remsndport[],
	dlslalog[],
	dlslatrn[],
	dlmaslog[],
	dlmastrn[];
extern char *sender;
struct netstruct    net_arpa;
struct iobuf    pinbuf,
                poutbuf;
int     eoslen;
int     highfd;
int     dlmode;			  /* DBG */
int     timerest;
int     dialerr;
int     dllognum,
        dlionum;
int     fout;
int     uid;
int     gid;
int     infd;			  /* handle on source text              */
int     savenv;
int     timeout ();
char    dbgmode;
char    eosstr[];
char   *txtname;
char    errline[100];
char   *userdir;
char   *username;
/*    MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN     */

main (argc, argv)
int     argc;
char   *argv[];
/* if run in user mode:                                              */
/* [name,] infd, namebuf.fildes, resbuf.fildes, logbuf.files,txtfile */
{
/*  Determine master (master under Deliver?) / slave mode and then call */
/*  appropriate mode's executive.                                       */
/*  Open logging file, if not already opened (my Deliver)               */

    int     thelogfd;

    dbgmode = TRUE;
				  /*  highfd = 25;    *//* oonly at rand?      
				     */
    eoslen = strlen (eosstr);
    siginit ();
    if (getusr () < OK)
	sysabrt (RPLLOC, "Unable to obtain password info on user");
    if (argc == 1)
    {				  /* called as "slave" from shell       */
	dlmode = 's';
	if ((thelogfd = open (remslalog, 5)) > 0)
	    initlog (thelogfd);
	log ("[Slave]");
	servit ();
    }
    else
    {
	dlmode = 'm';
	if ((thelogfd = open (remmaslog, 5)) > 0)
	    initlog (thelogfd);
	if (argc > 2)
	{                         /* running under Deliver             */
	    initcom (argc, argv, FALSE);
	    naminit (argc, argv); /* do we print msgs to user?  */
	}
	else                      /* running just as "pickup"          */
	    if (equal (argv[1], "-w", 2))
		domsg = TRUE;
	douser (argc, argv);      /* "arpanet" delivery mode            */
    }
}

naminit (argc, argv)
int     argc;
char   *argv[];
{
    int     result;

    result = getname ();
    domsg = (adr.adr_hand == 'w');
    switch (result)
    {				  /* 1st addr has init info             */
	case NOTOK: 		  /* no more messages                   */
	case DONE: 		  /* end of adr list                    */
	    sysabrt (DONE, 0);
	case OK: 
	    domsg = (adr.adr_hand == 'w');
	    return (OK);
	default: 
	    sysabrt (RPLLOC, "Problem reading first name");
    }
}
/* *********************  USER/MASTER MODE  ************************* */

douser (argc, argv)
int     argc;
char   *argv[];
{
    int     result;
    int     triesleft;
    char    vianet[LINESIZE];
    char    maysend;

/*  Invoke private protocol, if user has one.  Otherwise, send out      */
/*  waiting mail first, then pickup up mail held by remote site.        */

/*  RETURN: none */

#ifdef ARPANET
    vianet[0] = ARPANET;	  /* DBG hack to get hostname     */
#else
    vianet[0] = POBOX;	  /* DBG hack to get hostname     */
#endif
    strcpy (locname, &vianet[1]);
/*    if ((result = privpgm (argc, argv)) != TRYMAIL)                   */
/*        sysabrt (result, "Ending with privpgm check");  /*DBG FIX THIS*/

    signal (SIGCLK, &timeout);
    maysend = (argc > 2) ? TRUE : FALSE;

    for (triesleft = DL_TRIES; triesleft > 0; triesleft--)
    {
	if (!envsave (&timerest))
	    break;              /* TIMEOUT DURING DIALUP; give up       */
	if (triesleft < DL_TRIES)
	{
	    dlclose ();
	    log ("***  REDIALING  ***");
	    printx ("\n\tre-");
	}
	else
	    log ("dialing");
	printx ("dialing... ");
/*      alarm (600);    TRUST ED'S CODE...      */
/*	if (dlopen ("../dialrand", "/dev/tty5", "/rand_ms/dl.log",
		    "/rand_ms/dl.trn", 1, 0) < OK) */
	if (dlopen (script, remsndport, dlmaslog,
		    dlmastrn, 1, 1) < OK)
				  /* debug, but no tty output     */
	{
	    printx ("couldn't connect... ");
	    log ("unable to connect");
	    continue;             /* maybe just busy => try again       */
	}
/*      alarm (0);      */
	printx ("connnected... ");
	if (!envsave (&timerest))
	    continue;           /* TIMEOUT AFTER CONNECTIN; retry?      */

	if (maysend)
	{                         /* TRANSMISSION: SEND MESSAGES        */
	    if (xdlwrite ("submit\n", 7) < OK || rem_rsp () < OK ||
		    (result = netsbmit (argc, argv)) < OK ||
		    xdlwrite (0, 0) < OK)
	    {
		printx ("problem with remote submission process... ");
		continue;
	    }
	    maysend = FALSE;      /* done with sending                  */
	    close (infd);	  /* don't need these any more          */
	    close (atoi (argv[2]));
	    close (atoi (argv[3]));
	}
				  /* RECEPTION: PICKUP MESSAGES         */
	if (xdlwrite ("pickup\n", 7) < OK || rem_rsp () < OK ||
		(result = r2l_sbmit (vianet)) < OK ||
		xdlwrite ("end\n", 4) < OK)
	{
	    printx ("problem with remote pickup\n\t");
	    continue;
	}
	printx ("normal end\n");
	sysabrt (result, 0);
    }				  /* BELOW THIS is error handling     */
    if (maysend)
	faknetsbmit ();		  /* just pass DEADs back to Deliver      */

    sysabrt (HOSTDEAD, "Remote site not available");
}
/* *****************  SERVER/SLAVE MODE  **************************** */

servit ()
{
    int     result;
    char    linebuf[LINESIZE];
    int     c;

/* Slave has been called up and awaits commands from caller.            */
/* Submit takes mail from caller and submits it into slave's mail       */
/* destributor.  Pickup, sends POBox mail out to caller.                */

/* RETURN: none  */

    signal (SIGCLK, &timeout);
    if (!envsave (&timerest))
	sysabrt (HOSTDEAD, "Timeout");
				  /* other side took too long; give up    */

    /* if (dlstart ("/mnt/dcrocker/dl.log", "/mnt/dcrocker/dl.trn", 1) < OK)*/
    if (dlstart (dlslalog, dlslatrn, 1) < OK)
	sysabrt (RPLLOC, "Error with dlstart");
				  /* DBG */

    for EVER
    {				  /* WHAT IS TRUE END OF SESSION  ?     */
	zero (&linebuf, sizeof linebuf);
	if (dlrcv (&linebuf, &c) <= 0)
	    sysabrt (dialerr, "Error return getting command");
	log ("Command received: \"%s\"\n", linebuf);
	if (c == 0 || strequ (linebuf, "end\n"))
	    sysabrt (DONE, 0);
	linebuf[c] = '\0';
	if (strequ (linebuf, "submit\n"))
	{
	    log ("[ Submit ]\n");
	    if (xdlwrite ("0\n", 2) < OK)
				  /* ack                    */
		sysabrt (HOSTDEAD, "xdlwrite error in servit");
#ifdef POBOX
	    linebuf[0] = POBOX;
#endif
	    strcpy (username, &linebuf[1]);
	    if ((c = r2l_sbmit (linebuf)) < OK)
		sysabrt (c, "r2l_sbmit error");
	    continue;
	}
	if (strequ (linebuf, "pickup\n"))
	{
	    log ("[ Pickup ]\n");
	    if (xdlwrite ("0\n", 2) < OK)
				  /* ack                     */
		sysabrt (HOSTDEAD, "xdlwrite error in servit");
	    if ((c = pickup ()) < OK)
		sysabrt (c, "pickup error in servit");
	    continue;
	}
	xdlwrite ("/unknown mode\n", 14);
    }
}
/* ****************  PRIVATE USER PROTOCOL?  *********************** */

getusr ()
{
    char    pwline[LINESIZE];
    register char  *ptr,
                   *strt;

/*  RETURN: OK, NOTOK    */

    if (getpw (getuid () & 0377, &pwline) < OK)
	return (NOTOK);
    for (strt = ptr = pwline; *ptr != ':'; ptr++);
				  /* user name                          */
    *ptr++ = '\0';		  /* login directory                    */
    username = strdup (strt);
    while (*ptr++ != ':');	  /* skip a field                       */
    uid = atoi (ptr);		  /* get uid                            */
    while (*ptr++ != ':');	  /* skip over it                       */
    gid = atoi (ptr);		  /* get gid                            */
    while (*ptr++ != ':');	  /* skip over it]                      */
    for (ptr = strt; *ptr != ':' && *ptr != '\n'; ptr++);
    *ptr = '\0';		  /* login directory                    */
    userdir = strdup (strt);
    return (OK);
}

privpgm ()
{
    extern char userrem[];
    register char  *p,
                   *q;
    register int    result;
    int     filesiz;
    int     osavenv;
    int     i;
    int     temp;
    int     status;
    char   *userprog;
    int     f;

/*  RETURN: OK, TRYMAIL, TRYAGN, HOSTERR, TIMEOUT       */

    userprog = strcat (userdir, userrem);
    if (access (userprog, 1) < OK)
				  /* Is it executable?                  */
    {
	free (userprog);
	return (TRYMAIL);
    }
    log ("User program...");
    switch (f = fork ())
    {
	case 0: 
	    if (infd != 0)
	    {
		close (0);	  /* NO! this is a terrible thing to do */
		dup (infd);       /*  "                                 */
		close (infd);     /*  "                                 */
	    }
	    setgid (gid);
	    setuid (uid);
	    execl (userprog, userrem, adr.adr_delv, txtname, sender, 0);
	    perror ("can't exec user program");
	    exit (TRYAGN);
	case NOTOK: 
	    free (userprog);
	    perror ("Can't fork user program");
	    exit (TRYAGN);
    }
    free (userprog);
    osavenv = savenv;
    if (envsave (&savenv))
    {
	filesiz = infdsize ();
				  /*  alarm (filesiz * 10 + 30);        */
	while ((temp = waita (&status)) != f && temp != NOTOK);
/*      alarm (0);      */
    }
    else
    {
	result = TIMEOUT;
	error ("user program timed out");
	kill (f, 9);
    }
    savenv = osavenv;
    if (temp == NOTOK)		  /* This shouldn't happen              */
	result = TRYAGN;
    else
    {
	result = (status >> 8) & 0377;
	if (result < MINSTAT || result > MAXSTAT)
	    result = HOSTERR;
    }
/*    if (result == OK)     */
     /*         msg ("delivered")*/ ;
    return (result);
}
/* *******************  USER-MODE SUBMIT  ************************* */
/*              (as if arpanet delivery module)                   */

#define GETNADR 0
#define GETNMSG 1
char    getnmode;

faknetsbmit ()
{
    dbglog ("Fake netsbmit");
    putresp (HOSTDEAD, "done disappeared");

    if (getnmode == GETNMSG && newfile () != OK)
	return;         /* died during msg xmission; sync with parent   */
    for EVER
	switch (getname ())
	{
	    case DONE: 
		putresp (HOSTDEAD, "listend & still gone");
		if (newfile () != OK)
		    return;
		break;
	    case OK: 
		putresp (HOSTDEAD, "still gone");
		break;
	    default: 
		return;
	}
}

char    adrok;			  /* got at least one address ok'd        */

netsbmit (argc, argv)
int     argc;
int     argv;
{
    int     result;
    int     hoststat;

/*  To Deliver, this process looks like the network (Arpanet) sending   */
/*  module.  It transmits a sequence of messages, according to          */
/*  the addresses that are passed to it from Deliver, via getname.      */
/*  An end-of-address-list causes the text of the message actually      */
/*  to be sent.                                                         */

/*  RETURN: OK / NOTOK / DONE / HOSTDEAD */

    printx ("starting delivery...\n");

    getnmode = GETNADR;           /* sending addresses                  */
    netsbinit ();
    for EVER			  /* iterate thru list                  */
    {				  /* we already have and adr            */

	if (l2net_adr () == HOSTDEAD)
	    return (HOSTDEAD);
	switch (result = rem_rsp ())
	{
	    case OK: 
		putresp (T_OK, 0);/* TEMP ok, since only sent adr     */
		adrok = TRUE;
		break;
	    case NOTOK: 
		putresp (NODEL, errline);
		break;
	    case HOSTDEAD: 
		return (HOSTDEAD);
	    default: 
		log ("*** Bad name resp (%d)", result);
		putresp (NOTOK, "Bad response");
	}
	result = getname ();
/*      dbglog ("getname return (%d)", result);     */
	switch (result)
	{
	    case DONE: 		  /* end of adr list                    */
		xdlwrite (0, 0);  /* signal end of it                   */
		getnmode = GETNMSG;
		switch (l2net_msg ())
				  /* xfer text of message               */
		{
		    case NOTOK: 
			putresp (NOTOK, "error reading message file");
			break;
		    case HOSTDEAD: 
			return (HOSTDEAD);
		    case OK: 
			result = rem_rsp ();
			dbglog ("putresp (%d)", result);
			if (result == DONE)
			    result = OK;
			putresp (result, (result == OK) ? 0 : errline);
			break;    /* the real result of the xmission    */
		    default: 
			return (NOTOK);
		}
		switch (result = newfile ())
		{
		    case OK:
			break;
		    case DONE:
			return (OK);
		    default:
			return (result);
		}
		result = getname ();
/*              dbglog ("getname, for 1st adr = %d", result);   */
		switch (result)
		{		  /* 1st addr has init info             */
		    case NOTOK:   /* no more messages                   */
		    case DONE:    /* end of adr list                    */
			return (DONE);
		    case OK: 
			break;
		    default: 
			return (NOTOK);
		}
		domsg = (adr.adr_hand == 'w');
		netsbinit ();     /* init then drop into name xfer */
	    case OK: 
		continue;
	    case NOTOK: 	  /* no more messages                   */
	    default: 
		return (NOTOK);
	}
    }
}

netsbinit ()
{
    int     result;
    register char  *p;

/*  RETURN: xdlwrite() return value        */

    adrok = FALSE;
    p = nmultcat ("     ",
	    (adr.adr_delv == DELMAIL) ? "m"
	    : (adr.adr_delv == DELTTY) ? "a"
	    : "s",
	    sender, "\n", 0);
    result = xdlwrite (p, strlen (p));
    free (p);
    return (result);
}
/* *************  SEND ADDRESS(ES) & MESSAGE TEXT  ****************** */

l2net_adr ()
{				  /* specify addr to remote host        */
    char   *p;
    char    linebuf[LINESIZE];
    register int    c;

/*  RETURN: xdlwrite() return value */

 /* build submit adr spec line         */
    dbglog ("l2net_adr()");
    if (adr.adr_name[c = (strlen (adr.adr_name) - 1)] == '\n')
	adr.adr_name[c] = '\0';
				  /* get rid of the newline char        */
    for (p = adr.adr_name; *p != ' '; p++);
    *p++ = '\0';		  /* terminate host number              */
    p = strcpy (p, &linebuf);
    p = strcpy (" at ", p);       /* got the user name                  */
    host2first (&net_arpa, TRUE, adr.adr_name, p);
    printx ("%s: ", linebuf);
    strcpy ("\n", &linebuf[strlen (linebuf)]);

    return ((xdlwrite (linebuf, strlen (linebuf)) < OK) ? HOSTDEAD : OK);
				  /* send the address                   */
}

l2net_msg ()
{
    int     retval;
    int     count;
    char    buf[512];
    char    lastchar;

/*  RETURN: OK, NOTOK, HOSTDEAD */

    printx ("sending...");
    dbglog ("l2net_msg()");
    seek (infd, 0, 0);		  /* Beginning of message               */
    if (adrok)			  /* got at least one adr ok'd          */
	while ((count = read (infd, buf, sizeof buf)) > 0)
	{			  /* write text of message              */
	    printx (".");
	    if ((retval = dlbuffo (buf, count)) < OK)
		return (HOSTDEAD);
	    lastchar = buf[count - 1];
	    dbglog ("l2net_msg()");
	}
/*  dbglog ("count = %d, retval = %d\n", count, retval);        */
    if (!adrok || count < 0)
	retval = xdlwrite (0, 0);
    else
    {
	if (lastchar != '\n')
	    dlbuffo ("\n", 1);
	retval = dlbuffo (0, 0);
    }
/*  dbglog ("count = %d, retval = %d\n", count, retval);        */
    return ((count < OK) ? NOTOK : (retval < OK) ? HOSTDEAD : OK);
}
/*  ****************  REMOTE -> SUBMIT  ****************************  */

r2l_sbmit (vianet)
char   *vianet;			  /* coming in through what net?  */
{
    extern char prgsubmit[];
    extern char namsubmit[];
    extern int  regfdary[];
    struct pipstruct    sbpin,
                        sbpout;
    int     result;
    int     tmp,
            tmp2;
    int     sbmitid;

/*  Submit mail to local mail system from remote sites.  Has to         */
/*  mediate between possibly different protocols, tho current           */
/*  style is (of course) almost compatible, since we own the world.     */

/*  RETURN: OK / DONE / result  */

    dbglog ("r2l_sbmit ()");

    printx ("\nchecking for mail to pickup... ");

    if (pipe (&sbpin) < OK || pipe (&sbpout) < OK)
	sysabrt (NOTOK, "unable to pipe() in r2l_sbmit");

/*  dbglog ("fd (sbpin[prd = %d, pwrt = %d], sbpout[prd = %d, pwrt = %d])",
	    sbpin.prd, sbpin.pwrt, sbpout.prd, sbpout.pwrt);    */
    regfdary[0] = sbpout.prd;
    regfdary[1] = sbpin.pwrt;
    regfdary[2] = 2;
    for (tmp = 3; tmp < highfd; regfdary[tmp++] = NOTOK);
    dbglog ("Forking %s", prgsubmit);
/*  tmp = logfd;    */
/*  logbuf.fildes = logfd = NOTOK;      */
    if ((sbmitid = newpgml (FRKEXEC, 0, regfdary, prgsubmit, namsubmit, 0))
	    == -1)
	sysabrt (NOTOK, "Unable to newpgml in r2l_sbmit");

/*  logbuf.fildes = logfd = tmp;        */
    close (sbpout.prd);
    close (sbpin.pwrt);
    zero (&pinbuf, sizeof pinbuf);
    zero (&poutbuf, sizeof pinbuf);
    pinbuf.fildes = sbpin.prd;
    poutbuf.fildes = sbpout.pwrt;


    while ((result = r2l_sbinit (vianet)) == OK &&
				  /* mail mode & return address         */
	    (result = r2l_adr ()) == OK &&
				  /* address list                       */
	    (result = r2l_msg ()) == OK &&
				  /* message text                       */
	    (result = l2r_rsp ()) == OK);
				  /* submitssion status                 */

    printx ("done\n");
    close (sbpout.pwrt);
    close (sbpin.prd);

    while ((tmp = wait (&tmp2)) != sbmitid && tmp != NOTOK);
    dbglog ("Ending submit = %d", tmp2 >> 8);

    return ((result < OK) ? result : OK);
}
/* *****************  PROTOCOL TO LOCAL SUBMIT ******************* */

r2l_sbinit (vianet)
char   *vianet;
{
    int     c;
    char   *tmpstr;
    char    linebuf[LINESIZE];
    char    initbuf[LINESIZE];

    adrok = FALSE;
    zero (&linebuf, LINESIZE);
    switch (dlinrec (linebuf, &c))
    {
	case NOTOK: 
	    return (HOSTDEAD);
	case OK: 
	    return (DONE);
	case 1: 
	    break;
	default: 
	    log ("illegal dlinrec retval");
	    return (NOTOK);
    }
    switch (c)
    {
	case 0: 
	case 1: 
	    return (DONE);
    }
    linebuf[c - 1] = '\0';

    switch (linebuf[5])		  /* mailing mode                 */
    {
	case '\0': 
	    return (DONE);
	case 'a': 
	case 'm': 
	case 's': 
	    break;		  /* valid code                   */
	default: 		  /* invalid code                 */
	    log ("illegal mode value: %c\n", linebuf[5]);
	    return (NOTOK);
    }
    if (vianet)
    {
	initbuf[0] = 'i';
	tmpstr = strcpy (vianet, &initbuf[1]);
	*tmpstr++ = '*';
    }
    else
	tmpstr = initbuf;
    *tmpstr++ = linebuf[5];
    tmpstr = strcpy ("lmntv\n", tmpstr);
    tmpstr = strcpy (&linebuf[6], tmpstr);
    tmpstr = strcpy (" at ", tmpstr);
    tmpstr = strcpy (locname, tmpstr);
    tmpstr = strcpy ("\n", tmpstr);

    return ((pipwrite (initbuf, tmpstr - initbuf) < OK) ? TRYAGN : OK);
}


r2l_adr ()
{
    char    linebuf[LINESIZE];
    int     c;

/*  RETURN: OK / rem2loc */

    dbglog ("r2l_adr ()");
    while ((c = rem2loc ()) == OK)
    {
	if (l2r_rsp () == HOSTDEAD)
	    return (HOSTDEAD);
	dbglog ("r2l_adr ()");
    }
    if (c == DONE)
	pipwrite ("!\n", 2);
    return ((c == DONE) ? OK : c);
}

r2l_msg ()
{
    int     count;
    static int  msgnum;
    int     retval;
    char    buf[512];
    char    lastchar;

/*  RETURN: OK / HOSTDEAD / TRYAGN */

    printx ("%d, ", ++msgnum);
    dbglog ("r2l_msg ()");
    while ((retval = dlinrec (buf, &count)) > 0)
    {				  /* write text of message             */
	if ((retval = fpwrite (buf, count)) < OK)
	{
	    error ("error writing to mailbox");
	    strcpy ("/error writing to mailbox\n", buf);
	    xdlwrite (buf, strlen (buf));
	    return (TRYAGN);
	}
	lastchar = buf[count - 1];
	dbglog ("r2l_msg ()");
    }
    if (retval < OK || count < OK)
	return (HOSTDEAD);
    if (lastchar != '\n')
	fpwrite ("\n", 1);
    fpwrite (0, 0);
    if ((retval = pipwrite ("", 1)) < OK)
    {
	error ("error writing to mailbox");
	strcpy ("/error writing to mailbox\n", buf);
	xdlwrite (buf, strlen (buf));
	return (TRYAGN);
    }
    dbglog ("count = %d, retval = %d\n", count, retval);
    return (OK);
}

l2r_rsp ()
{
    int     c;
    char    linebuf[LINESIZE];

/*  RETURN: ascii -> binary value of first char sent by child           */

    dbglog ("l2r_rsp ()");

    if ((c = pipread (&linebuf, sizeof linebuf, "\000\n\377")) <= 0)
    {
	perror ("Error reading pipe in l2r_rsp");
	return (TRYAGN);
    }
    if (linebuf[c - 1] == '\0')
	linebuf[c - 1] = '\n';
    xdlwrite (linebuf, c);
    linebuf[c] = '\0';
    linebuf[0] =- '0';		  /* make it binary                     */
    if (linebuf[0] < OK)
	strcpy (&linebuf[1], errline);
    return (linebuf[0]);
}

/* *************  PICKUP MAIL FOR FOREIGN HOST  ********************* */
/*                   (SUBMIT TO FOREIGN HOST)                           */

pickup ()
{
    extern char prgmailer[];
    extern char nammailer[];
    extern int  regfdary[];
    struct pipstruct    pkpin,
                        pkpout;
    int     retval;
    int     tmp,
            tmp2;
    int     mailerid;

/*  Cause Deliver to send all POBox mail waiting for the caller.  It is */
/*  send via Deliver's standard i/o fd's and this routine mediates it   */
/*  outbound transmission.                                              */

/*  RETURN: DONE / retval       */

    log ("pickup()\n");

    if (pipe (&pkpin) < OK || pipe (&pkpout) < OK)
	sysabrt (NOTOK, "Unable to pipe() in pickup");

/*  dbglog ("fd[pkpout (prd = %d, pwrt = %d), pkpin (prd = %d, pwrt = %d)]",
	    pkpin.pwrt, pkpin.prd,pkpout.prd, pkpout.prd);      */
    regfdary[0] = pkpout.prd;
    regfdary[1] = pkpin.pwrt;
    regfdary[2] = 2;
    for (tmp = 3; tmp < highfd; regfdary[tmp++] = NOTOK);
/*  tmp = logfd;    */
/*  logfd.fildes = logfd = NOTOK; */
    dbglog ("Forking %s", prgmailer);
    if ((mailerid = newpgml (FRKEXEC, 0, regfdary,
		    prgmailer, nammailer, "-p", 0)) == -1)
	sysabrt (NOTOK, "Unable to newpgml in pickup");
/*  logfd = tmp;  */
    close (pkpout.prd);
    close (pkpin.pwrt);
    zero (&pinbuf, sizeof pinbuf);
    zero (&poutbuf, sizeof pinbuf);
    pinbuf.fildes = pkpin.prd;
    poutbuf.fildes = pkpout.pwrt;
    adrok = FALSE;
    while ((retval = loc2rem ()) == OK &&
				  /* switches & return address          */
	    (retval = l2r_adr ()) == OK &&
				  /* address list                       */
	    (retval = l2r_msg ()) == OK &&
				  /* message text                       */
	    (retval = r2l_rsp ()) == OK);
				  /* submission status                  */

    if (retval != HOSTDEAD)
	xdlwrite (0, 0);

    close (pkpout.pwrt);
    close (pkpin.prd);
    while ((tmp = wait (&tmp2)) != mailerid && tmp != NOTOK);
    dbglog ("return = %d", tmp2 >> 8);
    return ((retval >= OK) ? OK : tmp2 >> 8);
}
/* *****************  PROTOCOL TO FOREIGN SUBMIT  ******************* */

l2r_adr ()
{
    char    linebuf[LINESIZE];
    int     c;

/*  RETURN: OK / TRYAGN / HOSTDEAD      */

    for EVER
    {				  /* address list                       */
	dbglog ("l2r_adr()");
	if ((c = pipread (&linebuf, sizeof linebuf, "\000\n\377")) < 0)
	{
	    perror ("Error reading pipe in l2r_adr");
	    return (TRYAGN);      /* pass address                       */
	}
	if (c == 0)
	    return (DONE);
	if (equal (&linebuf, "!\n", 2))
	{
	    xdlwrite (0, 0);
	    return (OK);
	}
	if (xdlwrite (&linebuf, c) < OK || rem2loc () < OK)
	    return (HOSTDEAD);
				  /* pass response/verification         */
    }
}

l2r_msg ()
{
    register int    c;
    char    linebuf[LINESIZE];
    char    lastchar;

/*  RETURN: OK / TRYAGN / HOSTDEAD      */

    dbglog ("l2r_msg()");
    while ((c = pipread (&linebuf, sizeof linebuf, "\000\n\377")) > 0)
    {				  /* write text of message              */
	if (c == 1 && linebuf[0] == '\0')
	{
	    c = 0;
	    break;
	}
	if (dlbuffo (&linebuf, c) < 0)
	{
	    log ("other side disappeared");
	    return (HOSTDEAD);
	}
	dbglog ("l2r_msg()");
	lastchar = linebuf[c - 1];
    }
    if (c < 0)
    {
	perror ("Error reading pipe in l2r_msg");
	return (TRYAGN);
    }
    if ((lastchar != '\n' && dlbuffo ("\n", 1) < OK) ||
	    dlbuffo (0, 0) < OK)
	return (HOSTDEAD);
    return (OK);
}
/* *********** PACKET COMMUTATION (rem <==> child) *****************  */

rem2loc ()
{
    char    linebuf[LINESIZE];
    int     c;
    int     retval;

/*  RETURN: OK / DONE / TRYAGN / HOSTDEAD      */

    dbglog ("rem2loc()");

    zero (&linebuf, sizeof linebuf);
    switch (retval = dlinrec (&linebuf, &c))
    {
	case NOTOK: 
	    return (HOSTDEAD);    /* return address                     */
	case OK: 
	    return (DONE);
    }
    if (pipwrite (&linebuf, c) < OK)
	return (TRYAGN);
    return (OK);
}

loc2rem ()
{
    char    linebuf[LINESIZE];
    register int    tmp;

/*  RETURN: OK / DONE / TRYAGN / HOSTDEAD      */

    dbglog ("loc2rem()");

    zero (&linebuf, sizeof linebuf);
    if ((tmp = pipread (&linebuf, sizeof linebuf, "\000\n\377")) < 0)
    {
	perror ("Error reading pipe in loc2rem");
	return ((xdlwrite (0, 0) < OK) ? HOSTDEAD : NOTOK);
    }
    if (tmp == 0)
	return (DONE);
    if (xdlwrite (linebuf, tmp) < 0)
	return (HOSTDEAD);
    return (OK);
}
/* *****************  REMOTE RESPONSES ACQUISITION  ***************** */

rem_rsp ()
{
    int     c;
    char    linebuf[LINESIZE];

/*  RETURN: ascii -> binary of first char sent by remote */

    dbglog ("rem_rsp ()");
    zero (&linebuf, LINESIZE);
    if (dlrcv (&linebuf, &c) <= 0)
    {
	perror ("Error from dlrcv in rem_rsp");
	return (HOSTDEAD);
    }
    linebuf[c] = '\0';
    linebuf[0] =- '0';		  /* make it binary                     */
    if (linebuf[0] < OK)
	strcpy (&linebuf[1], errline);
    return (linebuf[0]);
}

r2l_rsp ()
{
    int     c;
    char    linebuf[LINESIZE];

/*  RETURN: ascii -> binary of first char sent by remote */

    dbglog ("r2l_rsp ()");
    zero (&linebuf, sizeof linebuf);
    if (dlrcv (&linebuf, &c) <= 0)
    {
	perror ("Error from dlrcv in r2l_rsp");
	return (HOSTDEAD);
    }
    pipwrite (linebuf, c);
    linebuf[c] = '\0';
    linebuf[0] =- '0';		  /* make it binary                     */
    if (linebuf[0] < OK)
	strcpy (&linebuf[1], errline);
    return (linebuf[0]);
}
/* *************************  CHILD I/O  **************************** */

fpwrite (buf, len)
char   *buf;
int     len;
{
    int     retval;
    char   *strptr;

/*  RETURN: fwrite() return value */

#ifdef VERBOSE
    dbglog ("fpwrite (): (%d)\"%s\"",
	    len, (len == 0) ? "*** EOF *** " : buf);
#endif
    errno = 0;

    if (len == 0)
	fflush (&poutbuf);
    else {
	buf[len] = '\0';
	fwrite (&poutbuf, buf, len);
     }
    retval = (errno == 0) ? OK : NOTOK;
/*  dbglog ("fpwrite returning (%d)", retval);                  */
    return (retval);
}

pipwrite (buf, len)
char   *buf;
int     len;
{
    int     retval;
/*  RETURN: fwrite() return value */

    buf[len] = '\0';
/*  dbglog ("pipwrite (): \"%s\"", buf);        */

    retval = write (poutbuf.fildes, buf, len);
/*  dbglog ("pipwrite returning (%d)", retval);         */
    return (retval);
}

pipread (buf, maxlen, brkset)
char   *buf;
int     maxlen;
char   *brkset;
{
    int     retval;
    register   *p;
/*  RETURN: gcread() return value (count or NOTOK) */

    dbglog ("pipread ()");
/*  dbglog ("pinbuf.fildes = %d, maxlen = %d", pinbuf.fildes, maxlen);  */

    switch (retval = gcread (&pinbuf, buf, maxlen, brkset))
/*  switch (retval = read (pinbuf.fildes, buf, maxlen))         */
    {
	case NOTOK: 
	    perror ("[ error on pipe in ]");
	    break;
	case 0: 
	    dbglog ("[ eof on pipe in ]");
	    break;
	default: 
	    buf[(retval > 0 && retval <= maxlen) ? retval : 0] = '\0';
/*          dbglog ("pipread: (%d) \"%s\"", retval, buf);       */
    }
    return (retval);
}
/* *************************  UTILITIES  **************************** */

sysabrt (retval, a, b, c, d, e, f)
int     retval;
char   *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    extern int  sys_nerr;
    extern char *sys_errlist[];
    extern int  errno;

    dbglog ("closing...");
    if (a != 0)
	perror (a, b, c, d, e, f);
    if ((dlmode == 'm') ? dlclose () : dlstop () < OK)
	perror ("Telephone termination error");
    log ("Remsnd:  Ending [%d]", retval);
    switch (retval)
    {
	case OK: 
	case HOSTDEAD: 
	    exit (retval / 10);
    }
    abort (retval / 10);

}

dlrcv (linebuf, c)
char   *linebuf;
int    *c;
{
    int     retval;

    dbglog ("dlrcv ()");
    alarm (600);
    retval = dlresp (DATA, linebuf, c);
    alarm (0);
    if (retval < 0)
    {
	dbglog ("System error #%d returned", retval);
	return (HOSTDEAD);	  /* return address                     */
    }
    if (retval == 0)
    {
	dbglog ("ERROR message received");
	return (HOSTDEAD);	  /* return address                     */
    }
/*  dbglog ("retval = %d, (%d)\"%s\"", retval, *c, linebuf);    */
    return (1);
}

dlinrec (linebuf, c)		  /* "record" input               */
char   *linebuf;
int    *c;
{
    register int    nbytes,
                    result;
    char    code;

    dbglog ("dlinrec ()");
    alarm (600);
    nbytes = dlread (linebuf, &code);
    alarm (0);
    if (nbytes < 0)
    {
	perror ("Error during dlread");
	return (NOTOK);
    }
    switch (code)
    {
	case DATA: 
	    *c = nbytes;
#ifdef VERBOSE
	    linebuf[nbytes] = '\0';
	    dbglog ("dlinrec got (%d)\"%s\"", nbytes, linebuf);
#endif
	    dbglog ("dlinrec returning (#%d)", nbytes);
	    return (1);
	case EOT: 
	    *c = 0;
	    dbglog ("dlinrec returning (0)");
	    return (OK);
	case ERROR: 
	    log ("Error during receive:");
	    log ("%s", linebuf);
	    dbglog ("dlinrec returning ERROR (-1)");
	    return (NOTOK);
	default: 
	    log ("Unexpected message code '%c'", code);
	    if ((result = dlserr ("unexpected message code")) < 0)
		return (result);
	    dbglog ("dlinrec returning Unknown (-1)");
	    return (NOTOK);
    }
}

xdlwrite (buf, len)
char   *buf;
int     len;
{
    int     retval;

    if(len>0) buf[len] = '\0';
/*  dbglog ("dlwrite: \"%s\"", buf);    */

    alarm (600);
    retval = dlwrite (buf, len);
    alarm (0);
/*  dbglog ("dlwrite returning (%d)", retval);  */
    if (retval == HOSTDEAD)
	log ("HOSTDEAD during dlwrite");
    return (retval);
}
