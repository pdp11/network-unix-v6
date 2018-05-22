#				  /* Mailer for MS */
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
#include "mailer.h"
#include "ffio.h"
int     timout1;
int     timout2;
#define NUMFFBUFS   10
char ffbufs[NUMFFBUFS][FF_BUF];
char    homeque[];
char    aquedir[];
char    mquedir[];
char   *logindir;
char   *respon[];

#define SIGHUP 1
#define SIGINT 2
#define SIGQIT 3

#define NAMESIZE 100
#define ERRLINSIZ 300

struct dirent			  /* Structure for directory entries */
{
    int     d_inum;
    char    d_filenm[14];
    char    d_fill;
};

struct filelist			  /* Structure for sorting files */
{
    long int    f_time;		  /* Creation time of message */
    char    f_filenm[14];	  /* Filename */
    char    f_fill;		  /* Null for padding */
};


/* The following routines return something other than an int */

long int    getnum ();
long int    ff_pos (),
	    ff_size ();

/* Global variable declarations */

char    sender[NAMESIZE + 1];     /* Name of message sender */
char    username[10];
int     errno;
char    errline[ERRLINSIZ + 1];   /* Last FTP response */
char   *txtfile;		  /* Name of text file */
char   *homeaque;
struct iobuf    namebuf,
		resbuf;
int     dirfd;			  /* File descriptor for mail queue directory
				  */
int     msginfd;                  /* Handle on message text             */
int     adrinfd;                  /* Handle on address list for message */
int     child;                    /* Process id of locsnd               */
long int    curtime;		  /* Current time in hours */
long int    msgtime;              /* Creation of current message, in hours */
long int    optnoff;		  /* Offset to options */
long int    adroff;               /* Offset to adrlist, for tellusr()   */
char    sndrdlm;		  /* Delimiter by sender name */
char    lastdlm;		  /* Last delimiter found */
int     options;
int     userid;
int     ownerid;

char    skipped;		  /* Number of addresses skipped */
char    more;			  /* Number of addresses not completed */
char    domsg;			  /* True if writing messages */
char    pickup;			  /* Wants to retrieve POBOX mail */
char    donamed;		  /* only do named files */
char    bakgrnd;                  /* True if being run from ms */
char    msgflgs;
char    havt_ok;                  /* adr ok; wait til text sent         */
/**/

main (argc, argv)
int     argc;
char   *argv[];
{
    char dochdir;
    register int    i;

    for (i = 0; i < NUMFFBUFS; i++)
	ff_use (ffbufs[i]);
    dochdir = TRUE;
    if (argc > 1)
	arginit (argc, argv, &dochdir);
    varinit ();                 /* initialize variables             */
    if (bakgrnd)
	signal (SIGQIT, 1);     /* disabled only when necessary ?   */
    signal (SIGINT, 1);         /* also disable keyboard int        */
    close (2);                  /* Close error fd                   */
    if (!pickup)
    {
	signal (SIGHUP, 1);     /* Ignore hangups                   */
	close (0);              /* Close so ALS port doesn't hang   */
	open ("/dev/null", 0) != 0;    /* make sure 0 is open            */
    }
    if (dochdir && chdir (homeaque) < OK)
	sysabrt ("deliver: can't chdir to %s", homeaque);
    if (setuid (userid) < OK)
	sysabrt ("deliver: can't setuid to caller (%d)", userid);
    if (!pickup && donamed)
    {
	for (i = 1; i < argc; i++)/* Do each file named by user */
	    if (*argv[i] != '-')
		single (argv[i]);
	clslog ();
    }
    else
	multip ();		  /* Otherwise do entire directory */
    exit (OK);
}
/* *****************  INITIALIZATION ROUTINES  ********************** */
arginit (argc, argv, dochdir)
int     argc;
char   *argv[];
char   *dochdir;
{
    register int    i;

    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	    switch (argv[i][1])
	    {
		case 'b':
		    bakgrnd = TRUE;
		    break;
		case 'd':
		    dochdir = FALSE;
		    break;
		case 'p':
		    pickup = TRUE;
		    break;	  /* POBOX for caller only */
		case 'w':
		    domsg = TRUE;
		    break;        /* caller wants to watch  */
	    }
	else
	    donamed = TRUE;

    }
}


varinit ()
{
    char    pwbuf[150];
    register char  *ptr,
		   *start;

    homeaque = nmultcat (homeque, "/", aquedir, 0);

    time (&curtime);
    curtime =/ 3600;		  /* Get current time (in hours) */

    userid = getuid ();         /* real user id                     */
    ownerid = userid >> 8;
    userid =& 0377;
    bakgrnd = bakgrnd && (userid == 0 || userid == ownerid);
				/* asked for && run by me or root   */

    getpw (userid, &pwbuf);
    for (ptr = &pwbuf; *ptr != ':'; ptr++);
    *ptr = '\0';
    strcpy (&pwbuf, username);  /* name of user                     */
    while (*ptr++ != ':');
    while (*ptr++ != ':');
    for (start = ptr; *ptr != ':'; ptr++);
    *ptr = '\0';
    logindir = strdup (start);  /* user's login directory           */
}
/* *************  FILE PROCESSING MANAGEMENT  ******************  */

single (filename)
char    filename[];
{
    if (opnfile (filename) < OK)
	return;

    dofile (filename);
    clsfile ();
    makedest (NOTOK);
    if (!more)
	return;

    log ("[ spawning for background completion ]");
    if (fork () != 0 || fork () != 0)
	return;     /* parent and intermediate will return => exit      */
    domsg = FALSE;
    if (opnfile (filename) >= OK)
	dofile (filename);
}

/* Multip handles the case where the mailer should process all messages in
   the queue.  In order to make sure that messages are in the same order in
   which they were created, the creation date of each message (recorded in
   the file) is read and they are sorted.  If storage for the sort cannot
   be allocated, then the messages are processed sequentially.  */

multip ()
{
    extern int  mailsleep;
    struct inode    inode;
    int     count;

    for EVER
    {
	if ((dirfd = open (".", 0)) <= OK)
	    sysabrt ("deliver: can't open %s", homeaquedir);
	seek (dirfd, 32, 0);      /* Skip . and .. */

	if (fstat (dirfd, &inode) < OK ||
		(inode.gid = 0, (count = inode.fsize / 16 - 2) < 2) ||
		sortfil (count) < OK)
	    seqfil ();
	if (count <= 0)
	    printx ("No mail queued for sending.\n");
	close (dirfd);
	makedest (NOTOK);
	clslog ();
	if (!bakgrnd)           /* not running as daemon        */
	    break;
	sleep (mailsleep);
	time (&curtime);
	curtime =/ 3600;
    }
}
/* *****************  FILE-NAME ORDERING  *********************** */

sortfil (count)
int     count;
{
    register int    index;
    int     temp;
    struct dirent   dirent;
    struct filelist *filelist;
    int     compar1 ();

    if (count == 0)
    {
	printx ("No queued mail.\n");
	return (OK);
    }
    if ((temp = alloc (count * sizeof (*filelist))) == NOTOK)
	return (NOTOK);
    for (filelist = temp, dirent.d_fill = '\0', index = 0;
	--count >= 0 && read (dirfd, &dirent, 16) > 0; )
    {                       /* Openable file > put name/date into table */
	if (dirent.d_inum != 0 && dirent.d_filenm[0] != '.' &&
		(adrinfd = ff_open (dirent.d_filenm, 0)) != NOTOK)
	{
	    filelist[index].f_time = getnum ();
	    strcpy (dirent.d_filenm,
		    filelist[index].f_filenm);
	    ff_close (adrinfd);
	    index++;
	}
    }
    qsort (filelist, (count = index), sizeof *filelist, &compar1);
				/* Sort the entries by creation date. */
    for (index = 0; index < count; index++)
				/* Now go through the sorted table. */
	procfile (filelist[index].f_filenm);
    free (filelist);
    return (OK);
}
seqfil ()
{
    struct dirent   dirent;

    while (read (dirfd, &dirent, 16) == 16)
    {
	if (dirent.d_inum == 0)
	    continue;
	procfile (dirent.d_filenm);
    }
}
/* *****************  HANDLE A SINGLE FILE  ************************* */

procfile (filename)
    char filename[];
{
    printx ("\nFile %s\n", filename);
    if (opnfile (filename) == OK)
    {
	dofile (filename);
	clsfile ();
    }
}

opnfile (filename)
char    filename[];
{
    register char  *p,
		   *q;
    if ((adrinfd = ff_open (filename, 6)) == NOTOK)
    {
	printx ("deliver: cannot open control file %s\n", filename);
	return (NOTOK);
    }
    if (txtfile != 0)
	free (txtfile);         /* get rid of old txt file name */
    txtfile = nmultcat ("../", mquedir, filename, 0);
    if ((msginfd = open (txtfile, 0)) <= 0)
    {
	printx ("deliver: cannot open text file %s; errno = %d\n",
		txtfile, errno);
	ff_close (adrinfd);
	return (NOTOK);
    }

    msgtime = getnum () / 3600;	  /* Read create time time of message */
    sndrdlm = lastdlm;		  /* Whether warning already given */
    optnoff = ff_pos (adrinfd) - 1;  /* Offset to option bits */
    msgflgs = getnum ();	  /* Options: only RETCITE, now */
    sender[ff_read (adrinfd, &sender, LINESIZE, "\n\377") - 1] = '\0';
    adroff = ff_pos (adrinfd);  /* for tellusr() return mailer      */
    log ("[ %s / %s ]", filename, sender);
    return (OK);
}

clsfile ()
{
    ff_flush (adrinfd);
    ff_close (adrinfd);
    close (msginfd);
}

dofile (filename)
char    filename[];
{
    long int    sndroff;
    long int    elaptim;
    char    name[NAMESIZE + 1];

    more =
	skipped = FALSE;

    seqadr ();                  /* go to it; sequence thru addrs    */
    if (skipped)
	return;
    if (!more && !havt_ok)        /* Any that couldn't be handled? */
    {
	if (unlink (filename) < OK || unlink (txtfile) < OK)
	    error ("Unable to remove message files.");
	return;
    }

    elaptim = (msgtime) ? curtime - msgtime : 0;
    if (elaptim < timout1)         /* Prior to warning time?  */
	return;
    if (elaptim < timout2)         /* Prior to return time?   */
    {
	if (sndrdlm == DELNONE || tellusr (1, 0, 0) < OK)
	    return;
	ff_seek (adrinfd, optnoff);
	ff_putc (DELNONE, adrinfd);        /* flag as already warned   */
	log ("[ Warning issued ]");
    }
    else			  /* Warn only once */
    {
	if (tellusr (0, 0, 0) < OK)
	    return;
	unlink (filename);
	unlink (txtfile);
	log ("[ *** Returned to: %s *** ]", sender);
    }
}

seqadr ()
{
    register int    tmp;
    register char  *p;
    long int    ladroff;
    struct adr  adr;

    for EVER			  /* Go through each adr */
    {
	ladroff = ff_pos (adrinfd); /* used in doadr()               */
	if ((tmp = ff_read (adrinfd, p = &adr, sizeof adr, "\n\377")) <= 0)
	{
	    makedest (OK);      /* end of list                      */
	    return;
	}
	p[tmp - 1] = '\0';
	if (domsg && adr.adr_hand == 's')
	    adr.adr_hand = 'w';   /* Caller's hot to watch          */
	if (adr.adr_delv != DELNONE)
	{
	    if (adr.adr_delv & DELT_OK)
		adr.adr_delv =& ~DELT_OK;
	    doadr (&adr, ladroff); /* This one hasn't been done yet  */
	}
    }
}
/* *************  MANAGEMENT FOR INDIVIDUAL DELIVERY  *************** */

doadr (adr, ladroff)
struct adr *adr;		  /* Mailbox name */
long int    ladroff;               /* Offset to this address in file */
{
    long int    offset;
    char *char1, *char2;


 /* If the message was successfully delivered, or if a permanent failure
    resulted, indicate that this address has been handled by changing the
    delimiter between the host and mailbox name. */

    offset = ff_pos (adrinfd);          /* for OK & T_OK            */
    char1 = &(adr -> adr_delv);
    char2 = adr;                        /*      and SKIP            */
    ff_seek (adrinfd, ladroff + (char1 - char2));
    switch (doadr1 (adr))
    {
	case T_OK:                  /* name ok; text later          */
	    ff_putc (adr -> adr_delv | DELT_OK, adrinfd);
	    havt_ok = TRUE;
	    break;
	case OK: 		  /* Actually, means "done"       */
	    ff_putc (DELNONE, adrinfd);
	    break;
	case SKIP:
	    skipped = TRUE;
	    break;
	case NOTOK:
	default:
	    printx ("queued for retry");
	    more = TRUE;
    }
    printx ("\n");        /* Msg was generated by doadr2  */
    ff_seek (adrinfd, offset);  /* keep it clean & isolated     */
}
/*/* Doadr1 handles one address for this message.  It returns OK if
   processing of the message is finished (either successfully or
   unsuccessfully), NOTOK if it is to be tried again, and SKIP if
   it was skipped because the user does not want to wait for it now.
*/

doadr1 (adr)
struct adr *adr;
{
    switch (send (adr))
    {
	case T_OK:                /* Address only accepted  */
	    printx ("address ok");
	    return (T_OK);
	case OK: 		  /* Delivered successfully */
	    printx ("sent");
	    return (OK);
	case NODEL: 		  /* Permanent failure */
	    printx ("cannot deliver");
	    tellusr (0, adr, errline);
	    return (OK);
	case TRYAGN: 		  /* Temporary failure */
	    printx ("not deliverable now, ");
	    return (NOTOK);
	case TIMEOUT: 		  /* Host timed out */
	    printx ("destination took too long, ");
	    return (NOTOK);
	case BADNET: 		  /* Sockets closed or something */
	    printx ("transmission error, ");
	    return (NOTOK);
	case HOSTDEAD: 		  /* Couldn't open */
	    printx ("destination not available, ");
	    return (NOTOK);
	case HOSTERR:             /* Can't handle host behavior     */
	    printx ("error at destination, cannot deliver");
	    tellusr (0, adr, errline);
	    return (OK);
	case SKIP: 		  /* Skip over this one */
	    return (SKIP);
    }
}
/* ********  MANAGE DATA TRANSFER / SELECT DELIVERY PROGRAM ********* */

send (adr)
struct adr *adr;
{
    int     result;
    register int    c;
    register char  *p;

    if (!bakgrnd && (donamed && adr -> adr_hand == 'q'))
	return (SKIP);
    if (pickup)
    {				  /* check rcvr hostname && username  */
	for (p = adr -> adr_name; *p && *p != ' '; p++);
	c = *p;
	*p = '\0';
	if (
#ifdef POBOX
		adr -> adr_dest != POBOX ||
#endif
		!strequ (adr -> adr_name, username))
/*	    result = SKIP  / DN: give all hosts to pickup */ ;
	*p = c;
	if (result == SKIP)
	    return (SKIP);
    }
    if ((result = makedest (adr -> adr_dest)) < OK)
	return (result);

    adr -> adr_hand = (domsg) ? 'w' : 's';

    return (telldest (adr));
}

telldest (adr)
    struct adr *adr;
{
    char result;
    char    charstr[6];
    register int    c;
    register char  *p;

    if (namebuf.fildes == 0)
	return (HOSTDEAD);
    if (adr)
	puts (adr, &namebuf);
    if (putc ('\0', &namebuf) < OK ||
	    fflush (&namebuf) < OK ||
	    read (resbuf.fildes, &result, 1) <= 0)
    {
	makedest (NOTOK);       /* close it */
	result = HOSTDEAD;
    }
    else
    {				  /* non-null resbuf => response message  */
	zero (&errline, sizeof errline);
	switch (gcread (&resbuf, &errline, sizeof errline, "\n\000\377"))
	{
	 case NOTOK:
	 case OK:
	    makedest (NOTOK);     /* close it */
	    result = HOSTDEAD;
	    break;
	 case 1:
	    strcpy ("(No reason given)", errline);
	}
    }
    if (adr || havt_ok || result != OK)
	log ("%s\t[ %s (%d) ]", /* only do "[ end ]" if was problem     */
	   (adr) ? &(adr -> adr_dest) : "[ end ]",
	   (result >= MINSTAT && result <= MAXSTAT) ?
		respon[result - MINSTAT] : "Illegal response value", result);
				/* "[ END ]" wastes space in log, so    */
				/* do it only when needed               */
    return (result);
}
/* *************  DELIVERY CHANNEL OPEN & CLOSE  ***************  */

char curdest;

makedest (dest)
char    dest;
{                       /* NOTOK => close; 0 => end of addr list       */

    extern struct netstruct *nets[];
    static char listend;
    char    oldest;
    int     temp,
	    status;
    register struct netstruct *p;

    status = OK;
    if (curdest != OK)
    {                               /* already have a channel open   */
	oldest = curdest;
	if (dest != OK && curdest != dest)
	{
	    status = closedest (OK);
	    curdest = OK;
	}
	if (dest <= OK)             /* an eom or close request.     */
	{                           /* eom the dest.                */
	    if (listend)            /* been here before             */
		return (OK);
	    listend = TRUE;
	    status = telldest (OK); /* send the eom                 */
	    if (havt_ok)            /* have some old business       */
	    {                       /* only were addrs ack'd before */
		if (status < OK)
		{                   /* any problem with the eom?    */
		    printx ("unable to send message text\n");
		    more = TRUE;
		}
		else                /* convert the T_OKs to OKs     */
		{
		    printx ("sent\n");
		    destt2ok (oldest); /* change temp-OKs to OK       */
		}
		havt_ok = FALSE;
	    }
	}
	if (curdest == dest && listend) /* new msg on channel           */
	{
	    listend = FALSE;
	    if ((status = telldest (txtfile)) == OK)
		status = telldest (sender);
	}
    }
    if (curdest == dest || dest <= OK)
	return (status);        /* just wanted to close it out          */
    listend = FALSE;

    for (temp = 0; ; temp++)
    {
	if (nets[temp] == 0)
	    sysabrt ("Unknown delivery progam ('%c')\n", dest);
	if ((p = nets[temp]) -> net_code == dest)
	    break;                  /* which "channel" do we want?  */
    }
    if ((p -> net_access == BAKGRND && !bakgrnd)
#ifdef POBOX
	 || (p -> net_code == POBOX && !pickup)
#endif
	 )
	return (SKIP);          /* No background or POBOX run   */

/*   printx ("[ %s ]\n", p -> net_ref);*/
    log ("[ %s ]", p -> net_ref);

    if ((temp = execdest (p)) == OK)
	curdest = dest;
    return (temp);
}

execdest (p)
    struct netstruct *p;
{
    extern int regfdary[];
    int thelogfd;
    int nampipe[2];               /* to xmit process for addr name      */
    int respipe[2];               /* from xmit process, for responses   */
    char    arg1[10],
	    arg2[10],
	    arg3[10],
	    arg4[10];

    if (pipe (nampipe))
	return (BADNET);
    if (pipe (respipe) < OK)
    {
	close (nampipe[0]);
	close (nampipe[1]);
	return (BADNET);
    }
    itoa (msginfd, &arg1);
    itoa (nampipe[0], &arg2);
    itoa (respipe[1], &arg3);
    thelogfd = getlogfd ();
    itoa (thelogfd, &arg4);

    regfdary[0] = 0;
    regfdary[1] = 1;
    regfdary[2] = 2;
    regfdary[msginfd] = msginfd;
    regfdary[nampipe[0]] = nampipe[0];
    regfdary[respipe[1]] = respipe[1];
    if (thelogfd > 0)
	regfdary[thelogfd] = thelogfd;

    child = newpgml (FRKEXEC, 0, regfdary, p -> net_name, p -> net_ref,
	    arg1, arg2, arg3, arg4, txtfile, sender, 0);

    close (nampipe[0]);		  /* Close other end of pipes */
    close (respipe[1]);

    if (child == NOTOK)
    {
	child = 0;
	close (nampipe[1]);       /* Close other end of pipes */
	close (respipe[0]);
	return (BADNET);
    }
    namebuf.fildes = nampipe[1];
    resbuf.fildes = respipe[0];
    return (OK);
}

closedest (type)
    int type;
{
    int status;
    register int temp;

    if (child == 0)
	return (OK);            /* don't have one       */

    switch (type)
    {case NOTOK:                /* we are aborting      */
	kill (child, 9);
	break;
     case OK:                   /* normal end           */
	close (namebuf.fildes); /* Close other end of pipes */
	close (resbuf.fildes);
	break;
     default:
	sysabrt ("Illegal call to closedest with type = %d", type);
    }
    namebuf.fildes = 0;

    while ((temp = wait (&status)) != child && temp != NOTOK);
    if (temp == NOTOK)
	log ("Deliver: wait on child returned NOTOK");
    if ((temp = (status >> 8)) != 0)
	log ("Deliver: child returned: %d", temp);
    child = 0;  /* don't have one any more      */
    return (temp);
}

destt2ok (oldest)
    char oldest;
{                             /* convert temp_ok's to OKs       */
    register struct adrstruct *tadr;
    register int dlvoff;
    struct adrstruct mtadr;
    long int strtadr, endadr;

    dbglog ("destt2ok ()");
    tadr = &mtadr;
    dlvoff = &(tadr -> adr_delv) - tadr;
    seek (adrinfd, adroff);    /* start of address list    */
    for (strtadr = adroff;
	ff_read (adrinfd, tadr, sizeof tadr, "\n\377") > 0; strtadr = endadr)
    {
	endadr = ff_pos (adrinfd);
	if (tadr -> adr_dest != oldest || !(tadr -> adr_delv & DELT_OK))
	    continue;          /* same pgm && marked with temp_ok       */
	seek (adrinfd, strtadr + dlvoff);
	ff_putc (DELNONE, adrinfd);
	ff_seek (adrinfd, endadr);
    }
}

/* *****************  MAIL MESSAGE TO SENDER  *********************** */

tellusr (warn, adrptr, reason)
int     warn;
struct adr *adrptr;
char    reason[];
{
    extern char prgsmail[];
    extern char namsmail[];
    extern char locname[];
/*  register */ int    c;
    register char  *p;
    int     pipdes[2];
    struct iobuf    pipebuf;
    int     temp;
    int     f;
    int     lines;
    int     status;
    char    days[10];
    char    linebuf[LINESIZE];
    struct adr  adr;
    struct iobuf    textbuf;

    if (sender[0] == '\0')      /* no return address                */
	return (OK);
    if (pipe (pipdes))
	return (NOTOK);
    strcpy (locname, strcpy ("MMDF Memo Distributor @ ", linebuf));
    f = fork ();
    switch (f)
    {case NOTOK:                /* bad day all around               */
	close (pipdes[0]);
	close (pipdes[1]);
	return (NOTOK);
     case 0:                  /* this is the child                  */
	close (0);
	dup (pipdes[0]);
	close (pipdes[0]);
	close (pipdes[1]);
	execl (prgsmail, namsmail, sender, "-f", linebuf, "-s",
		warn ? "Mail not yet sent" : "Undeliverable mail", 0);
	exit (NOTOK);
    }
    close (pipdes[0]);
    zero (&pipebuf, sizeof pipebuf);    /* since it's on the stack  */
    pipebuf.fildes = pipdes[1];
    if (adrptr)
    {
	if (puts ("   Your message could not be delivered to \"", &pipebuf)
		< OK ||
		puts (adrptr -> adr_name, &pipebuf) < OK ||
		puts ("\" for\nthe following reason: ", &pipebuf) < OK ||
		puts (reason[0] ? reason : "(Reason not known)", &pipebuf)
		    < OK)
	    sysabrt ("Error 1 writing to %s", namsmail);
    }
    else
    {
	temp = (curtime - msgtime) / 24;
	itoa (temp, &days);
	if (puts (warn ? "Your message has not yet been" :
		"   Your message could not be", &pipebuf) < OK ||
		puts (" delivered to the following addressee(s)\nafter ",
			&pipebuf) < OK ||
		puts (days, &pipebuf) < OK ||
		puts ((temp > 1) ? " days:\n\n" : " day:\n\n", &pipebuf)
		    < OK)
	    sysabrt ("Error 6 writing to %s", namsmail);
	for (ff_seek (adrinfd, adroff), p = &adr;
		(c = ff_read (adrinfd, &adr, sizeof adr - 1, "\n\377")) > 0;)
	{
	    p[c] = '\0';   /* hack. hack. hack.     */
	    if (adr.adr_delv != DELNONE &&
		(puts ("\t", &pipebuf) < OK  ||
		 puts (adr.adr_name, &pipebuf) < OK))
	    sysabrt ("Error 7 writing to %s", namsmail);
	}
    }
    puts ("\n   Your message ", &pipebuf);
    if (warn || msgflgs == RETCITE)
	puts ("begins as ", &pipebuf);
    puts ("follows:\n\n", &pipebuf);

/* If this is a warning or RETCITE, then include the header and the first
   three lines of the body (not counting blank lines).  Otherwise include
   the entire message.  */

    for (seek (msginfd, 0, 0), textbuf.fildes = msginfd,
	    textbuf.nleft = textbuf.nextp = 0;
	    (c = gcread (&textbuf, &linebuf, sizeof linebuf, "\n\377")) > 0;)
    {                                   /* do headers               */
	if (fwrite (&pipebuf, &linebuf, c) < OK)
	    sysabrt ("Error 1 writing to %s", namsmail);
	if (c == 1)     /* blank line -> end of headers             */
	    break;
    }
    if (c < 0)
	sysabrt ("Problem reading message for return message to sender.");
    if (c > 0)
    {
	if (warn || msgflgs == RETCITE)
	    for (lines = 0; lines++ < 3 &&
		(c = gcread (&textbuf, &linebuf, sizeof linebuf, "\n\377"))
		    > 0;)
	    {
		if (c == 1)     /* blank lines don't count      */
		    lines--;
		if (fwrite (&pipebuf, &linebuf, c) < OK)
		    sysabrt ("Error 2 writing to %s", namsmail);
	    }
	else
	    while ((c = gcread (&textbuf, &linebuf, sizeof linebuf, 0)) > 0)
		if (fwrite (&pipebuf, &linebuf, c) < OK)
		    sysabrt ("Error 7 writing to %s", namsmail);
    }
    if (fflush (&pipebuf) < OK)
	sysabrt ("Error 3 writing to %s process", namsmail);
    for (close (pipdes[1]); (temp = wait (&status)) != f && temp != NOTOK;);
    return (temp == NOTOK ? NOTOK : OK);
}
/* ********************  UTILITIES  ***************************** */

long int    getnum ()
{
    register char   c;
    long int    result;

    for (result = 0; (c = ff_getc (adrinfd)) >= '0' && c <= '9';
	    result = result * 10 + (c - '0'));
    lastdlm = c;
    return (result);
}


inperr ()			  /* i can do better than this! */
{
    sysabrt ("Input error\n");
}

/* Compar1 is called by qsort to compare times so that files will
   be handled in the same order in which they were created.
*/

compar1 (a, b)
struct filelist *a,
	       *b;
{
    long int    i;

    i = a -> f_time - b -> f_time;
    return (i < 0 ? NOTOK : (i > 0 ? 1 : 0));
}

sysabrt (a, b, c, d, e, f)
char    a[],
	b[],
	c[],
	d[];
char    e[],
	f[];
{
    printx (a, b, c, d,e,f);
    log (a, b, c, d,e,f);
    log ("[*** Abnormal termination ***]");
    flush ();
    makedest (NOTOK);
/*  abort ();    DBG */
    exit (NOTOK);
}
