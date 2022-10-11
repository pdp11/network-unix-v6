#define DELIVERMAIL		/* To use /etc/delivermail to send mail */
/*
 * Eric Shienbrood (BBN) 3 Apr 81 - MTP server, based on BBN FTP server
 */

#include "stdio.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/stat.h"

#define tel_iac 255
/************ F I L E S   U S E D   B Y   M T P ***************/
/******/						/******/
#ifdef DELIVERMAIL
/******/	char *DELIVMAIL= "/etc/delivermail";	/******/
#else
/******/	char *SNDMSG	= "sndmsg";		/******/
/******/	char *pwfile	= "/etc/passwd";	/******/
/******/	char *afile	= "/etc/net/aliases";	/******/
/******/	char *hmdbfile	= "/etc/net/homedb";	/******/
#endif DELIVERMAIL
/******/						/******/
/************ F I L E S   U S E D   B Y   M T P ***************/

char *BUGS = "bug-mtp at bbn-unix";

/*
 * Structure for the components of the MAIL command's TO: argument.
 *	The argument is of the form [<path>]user@host
 *	where "path" is of the form "@host1,@host2,..."
 */

struct path {
	char *p_first;		/* first part of path (should be our name) */
	char *p_hosts;		/* path, after our name has been removed */
	char *p_username;	/* user name of destination mailbox */
	char *p_hostname;	/* host name of destination mailbox */
} rcvpath, sndpath;

char *progname, *us, *them;
char *sender;		/* address of mail sender */
char *errmsg(), *sfind(), *sskip(), *scopy();

extern int errno;	/* Has Unix error codes */
#include "errno.h"

#define ERRFD stderr         /* this is presumably the log file */

int mail(), quit(), help(), mrsq(), mrcp(), accept();

/*
/****************************************************************
 *                                                              *
 *      C O M M A N D   D I S P A T C H   T A B L E             *
 *                                                              *
 ****************************************************************/

struct comarr		/* format of the command table */
{
	char *cmdname;		/* ascii name */
	int (*cmdfunc)();       /* command procedure to call */
} commands[] = {
	"mail", mail,		"mrsq", mrsq,
	"mrcp", mrcp,		"help", help,
	"noop", accept,		"quit", quit,
	"cont", accept,		"abrt", accept,
	NULL, NULL
};

/* communications buffers/counts/pointers */

int  netcount = 0;	/* number of valid characters in netbuf */
char netbuf[512];	/* the place that has the valid characters */
char *netptr  = netbuf;	/* next character to come out of netbuf */

#define BUFL 600	/* length of buf */
char buf[BUFL];		/* general usage */
char pwbuf[512];	/* password entry for current user */
char addressbuf[512];	/* holds <user>:<password> for logging purposes */
char tmpname[] = "/tmp/crmtpXXXXXX";

/* globals used by the mailing stuff */

#define MRSQ_D  0	/* default (no) scheme */
#define MRSQ_T  1	/* Text first scheme */
#define MRSQ_R  2	/* Recipient first scheme (not implemented yet) */

#define MBNAMSIZ 40
char mfname[MBNAMSIZ];	/* storage for mail file name */
int  mrsqsw = MRSQ_D;	/* semaphore for the mrsq/mrcp stuff */


/* character defines */
  
#define CR	'\r'	/* carriage return */
#define LF	'\n'	/* line feed */
#define CNULL	'\0'	/* null */

int just_con_data;	/* used by getline procedure to limit function */
#ifdef DEBUG
int mypid;
#endif DEBUG
char *arg;		/* zero if no argument - pts to comm param */
char *getline(), *strmove();
/*name:
	main

function:
	network server mtp program

	Takes commands from the assumed network connection (file desc 0)
	under the assumption that they follow the ARPA network mail
	transfer protocol NIC doc ??? RFC 780 and appropriate modifications.
	Command responses are returned via file desc 1.

	

algorithm:
	process structure:

		There is a small daemon waiting for connections to be
		satisfied on socket 57 from any host.  As connections
		are completed by the ncpdaemon, the returned file descriptor
		is setup as the standard input (file desc 0) and standard
		output (file desc 1) and this program is executed to
		deal with each specific connection.  This allows multiple
		server connections, and minimizes the system overhead
		when connections are not in progress.  File descriptors
		zero and one are used to ease production debugging and
		to allow the forking of other relavent Unix programs
		with comparative ease.


	main itself:

		while commands can be gotten
			find command procedure
				call command procedure
				get next command
			command not found

parameters:
	none

returns:
	nothing

globals:
	commands

calls:
	getline
	seq
	netreply
	( any of the procedures in the commands array )

called by:
	small server daemon program

history:
	initial coding 4/12/76 by S. F. Holmgren
	long hosts jsq BBN 3-20-79
	change error handling jsq BBN 20Aug79
	turned into mtp Eric Shienbrood BBN 24Apr81
	put in code to call delivermail, ifdef'd on
	DELIVERMAIL - Eric Shienbrood BBN 12Aug81

*/

main(argc, argv)
char **argv;
{
	register struct comarr *comp;
	char *i;
	long atime;
	char *getuc(), *thisname();

#ifdef SIGINR
	signal(SIGINR,1);       /* ignore INS interrupts */
#endif SIGINR
	progname = argv[0];
	if (argc != 3){
		fprintf(ERRFD, "%s:  wrong number of arguments\n",
			progname);
		netreply("421 %s %s was incorrectly invoked by mtpsrv!\r\n",
			getuc (thisname ()), progname);
		exit(-1);
	}
	them = argv[1];
	us = argv[2];
	time(&atime);
	fprintf(ERRFD, "%s %s %s", progname, them, ctime(&atime));

	/* say we're listening */
#ifdef DEBUG
	mypid = getpid();
#endif DEBUG
	netreply("220 %s Experimental Server MTP\r\n", argv[2]);

nextcomm:
	while (i = getline())
	{
		if (i == (char *)-1)	/* handle error */
			go_die(ERRFD, "%s: %s net input read error; %s\n",
				progname, them, errmsg(0));

		/* find and call command procedure */
		comp = commands;
		while( comp->cmdname != NULL)	/* while there are names */
		{
		    if (seq(buf, comp->cmdname))	/* a winner */
		    {
			(*comp->cmdfunc)();	/* call comm proc */
			goto nextcomm;		/* back for more */
		    }
		    comp++;		/* bump to next candidate */
		}
		netreply("500 I never heard of that command before\r\n" );
	}
	mail_reset();	/* flush any mrcp/mrsq stored text */
}

/*name:
	getline

function:
	get commands from the standard input terminated by <cr><lf>.
	afix a pointer( arg ) to any arguments passed.
	ignore carriage returns
	map UPPER case to lower case.
	manage the netptr and netcount variables

algorithm:
	while we havent received a line feed and buffer not full

		if netcount is zero or less
			get more data from net
				error: return 0
		check for delimiter character
			null terminate first string
			set arg pointer to next character
		check for carriage return
			ignore it
		if looking at command name
			convert upper case to lower case

	if command line (not mail)
		null terminate last token
	manage netptr

parameters:
	none

returns:
	0 for EOF
	-1 when an error occurs on telnet connection
	ptr to last character (null) in command line

globals:
	just_con_data
	netcount=
	netptr =
	buf=

calls:
	read (sys)

called by:
	main

history:
	initial coding 4/12/76 by S. F. Holmgren
	changed no more line condition from "read(..net..) < 0" to
		"read(..net..) <= 0" 5Oct78 S.Y. Chiu
	change error handling jsq BBN 20Aug79
*/

char *
getline()
{

	register char *inp;	/* input pointer in netbuf */
	register char *outp;	/* output pointer in buf */
	register int c;		/* temporary char */
	int cmdflag;		/* looking for telnet command */
	extern char *progname;

	inp = netptr;
	outp = buf;
	arg = 0;
	cmdflag = 0;

	do
	{
		if( --netcount <= 0 )
		{
			netcount = read (0, netbuf, 512);
			if (netcount == 0)	/* EOF */
				return( 0 );
			if (netcount < 0)	/* error */
				return((char *)-1);
			inp = netbuf;
		}
		c = *inp++ & 0377;

#ifdef SIGINR
		if (cmdflag)		/* was last char IAC? */
		{       cmdflag = 0;	/* if so ignore this one too */
			c = 0;		/* make sure c != \n so loop won't */
			continue;	/*  end */
		}

		if (c == tel_iac)	/* is this a telnet IAC? */
		{       cmdflag++;	/* if so note that next char */
			continue;	/*  to be ignored */
		}
#endif SIGINR
		if (c == '\r' ||	/* ignore CR */
		    c >= 0200)		/* or any telnet codes that */
			continue;	/*  try to sneak through */
		if (just_con_data == 0 && arg == NULL)
		{
			/* if char is a delim afix token */
			if (c == ' ' || c == ',')
			{
				c = CNULL;       /* make null term'ed */
				arg = outp + 1; /* set arg ptr */
			}
			else if (c >= 'A' && c <= 'Z')
			/* do case mapping (UPPER -> lower) */
				c += 'a' - 'A';
		}
		*outp++ = c;
	} while( c != '\n' && outp < &buf[BUFL] );

	if( just_con_data == 0 )
		*--outp = 0;		/* null term the last token */

	/* scan off blanks in argument */
	if( arg ){
		while( *arg == ' ' )
			arg++;
		if( *arg == '\0' )
			arg = 0;	/* if all blanks, no argument */
	}
#ifdef DEBUG
	if (just_con_data == 0) {
		fprintf (ERRFD, ":%d: %s", mypid, buf);
		if (arg)
			fprintf (ERRFD, " %s", arg);
		fprintf (ERRFD, "\n");
	}
#endif DEBUG
	/* reset netptr for next trip in */
	netptr = inp;
	/* return success */
	return( outp );
}

/*name:
	mail

function:
	handle the MAIL command over the command connection
	General form:
		MAIL from:<user@host> to:<@h1,@h2,@h3,user@host>

	mail from div6net machines going to the arpanet (BBN ONLY)
	through the forwarder uses the command
		MAIL from:<user@div6host> to:<@FWDR,user@arpahost>

algorithm:
	see if we have a known user

	if mailbox file can't be gotten
		return
	tell him it is ok to go ahead with mail

	while he doesnt type a period
		read and write data
	if path is nonempty, strip our name from the list.
	send the mail to the destination
	say completed

parameters:
	username in arg

returns:
	nothing

globals:
	arg
	username=

calls:
	strmove
	findmbox
	loguser
	getmbox
	write (sys)
	getline
	chown (sys)
	time (sys)
	printf (sys)

called by:
	main thru command array

history:
	initial coding 		Mark Kampe UCLA-ATS
	modified 4/13/76 by S. F. Holmgren for Illinois version
	modified 6/30/76 by S. F. Holmgren to call getmbox
	modified 10/18/76 by J. S. Kravitz to improve net mail header
	modified by greep for Rand mail system
	long host jsq BBN 3-20-79
	error handling jsq BBN 20Aug79
	modified 4/25/80 by dm(BBN) for mrsq/mrcp support
*/

mail()
{
	register FILE * mboxfid;	/* fid for temporary *mfname file */
	register char *p;	/* general use */
	char *bufptr;
	time_t tyme;		/* for the time */
	int werrflg;		/* keep track of write errors */
	int errflg;		/* And other errors (dan) */
	int keep;		/* flag for rsq stuff */
	FILE *getmbox();
	extern char *errmsg();

	keep = 0;
	errflg = 0;
	werrflg = 0;

	mail_reset();	/* unlink any left over temporary files */

	if (arg == 0) {
		netreply("501 No arguments supplied\r\n");
		return;
	}
	if (!veq_nocase (arg, "from:", 5)) {
		netreply("501 No sender named\r\n");
		return;
	}

	/* First, copy the argument into a non-volatile area */
	scopy (arg, addressbuf, 0);

	/* Scan FROM: and TO: parts of arg */
	sender = sfind (addressbuf, ":") + 1;
	p = sskip (sfind (sender, " "), " ");
	if (dstparse (sender, &sndpath) != 0)
		goto badsyn;

	/*
	 * p now points to the beginning of
	 * the TO: argument, if any
	 */
	if (*p == CNULL) {	/* make sure there is a user name */
		keep++;
		if (mrsqsw == MRSQ_D) {	/* unless one is not necessary */
			netreply("501 No recipient named.\r\n");
			return;
		}
	} else {
		/* parse destination arg */
		if (!veq_nocase (p, "to:", 3)) {
			netreply("501 No recipient named.\r\n");
			return;
		}
		p = sfind (p, ":") + 1;
		if (dstparse (p, &rcvpath) != 0)
			goto badsyn;
		/*
		 * If destination is on this machine,
		 * then user must be known.
		 */
		if (rcvpath.p_first == NULL && rcvpath.p_hosts == NULL) {
			if (findmbox() == 0) {
				netreply("553 User \"%s\" Unknown\r\n",
					rcvpath.p_username);
				return;
			}
		}
	}
	/* get to open mailbox file descriptor */
	if ((mboxfid = getmbox()) == NULL)
		return;

	/* say its ok to continue */
	netreply ("354 Enter Mail, end by a line with only '.'\r\n");

	just_con_data = 1;      /* tell getline only to drop cr */
#ifdef DEBUG
	fprintf (ERRFD, ":%d: ... body of message ...\n", mypid);
#endif DEBUG

	while (1) {             /* forever */
		if ((p = getline()) == 0) {
		    fprintf(mboxfid,"\n***Sender closed connection***\n");
		    errflg++;
		    break;
		}
		if (p == (char *)-1) {
		    fprintf(mboxfid, "\n***Error on net connection:  %s***\n",
			    errmsg(0));
		    if (!errflg++)
			log_error("(mail) errflg++");
		    break;
		}
		/* are we done? */
		if (buf[0] == '.')
			if (buf[1] == '\n')
				break;		/* yep */
			else
				bufptr = &buf[1];	/* skip leading . */
		else
			bufptr = &buf[0];
		/* If write error occurs, stop writing but keep reading. */
		if (!werrflg) {
		    if (fwrite( bufptr, 1, (p-bufptr),mboxfid ) < 0) {
			werrflg++;
			log_error("(mail) werrflg++");
		    }
		}
	}
	just_con_data = 0;	/* set getline to normal operation */
#ifdef DEBUG
	fprintf (ERRFD, ":%d: Finished receiving text.\n", mypid);
#endif DEBUG

	if (errflg) {
	    time (&tyme);
	    fprintf(mboxfid,"\n=== Network Mail from host %s on %20.20s ===\n",
			them, ctime (&tyme) );
	}

	fclose (mboxfid);
	if (werrflg) {
		netreply("451-Mail trouble (write error to temporary file)\r\n");
		netreply("451 (%s); try again later\r\n", errmsg(0));
		mail_reset();	/* delete temporary file */
		return;
	}
	if (mrsqsw == MRSQ_T && keep) {
		netreply("250 Mail stored\r\n");
		return;
	}
	if (sndmsg(mfname) == -1)	/* call sndmsg to deliver mail */
		netreply("451 Mail trouble (sndmsg balks), try later\r\n",
				errmsg(0));
	else
		netreply("250 Mail Delivered \r\n");
	mail_reset();     /* clean up (delete temporary file, *mfname) */
	return;

badsyn:
	netreply("501 Bad argument syntax\r\n");
	return;
}

/*
 * dstparse - parse the a path of the form
 *
 *		<@h1,@h2,...,user@host>
 *
 *	into its constituent parts.
 *	returns 0 for successful parse, -1 for failure
 */

dstparse (str, pp)
register char *str;
register struct path *pp;
{
	if (str == NULL)
		return (-1);

	while (*str == ' ')
		str++;
	if (*str == CNULL || *str++ != '<' || *str == CNULL)
		return (-1);

	if (*str != '@') {	/* simple address: user@host */
		pp->p_hosts = NULL;
		pp->p_first = NULL;
	}
	else {	/* compound: @host1,@host2,...,user@host */
		pp->p_first = str;
		str = sfind (str, ",");
		if (*str == CNULL)
			return (-1);
		*str++ = CNULL;
		if (*str != '@')
			pp->p_hosts = NULL;
		else {
			pp->p_hosts = str;
			/*
			 * Find the first comma that is not
			 * followed by a @.  That is the beginning
			 * of the user@host part.
			 */
			do {
				str = sfind (str, ",");
				if (*str++ == CNULL)
					return (-1);
			} while (*str == '@');
			str[-1] = CNULL;
		}
	}

	/* Now get the user@host part */

	pp->p_username = str;
	str = sfind (str, "@");
	if (*str == CNULL)
		return (-1);
	*str++ = CNULL;
	pp->p_hostname = str;
	if (*str == '>')
		return (-1);
	for (; *str != '>'; str++)
		if (*str == ',' || *str == CNULL)
			return (-1);
	*str = CNULL;		/* Remove trailing '>' */
#ifdef PDEBUG
printf ("dstparse: p_first: %s\np_hosts: %s\np_username: %s\np_hostname: %s\n",
  pp->p_first, pp->p_hosts, pp->p_username, pp->p_hostname);
#endif PDEBUG
	return (0);
}

/*
name:
	getuser

function:
	getuser: find a match for the user in username
	  in /etc/passwd or /etc/net/aliases

parameters:
	user name in username
	name of file to look in

returns:
	1 if user found
	0 failure

	pwbuf containing the line from the passwd or alias file which matched

history:
	initial coding 4/12/76 by S. F. Holmgren
*/

#ifndef DELIVERMAIL
getuser(filename, must_exist)
    char *filename;
    int must_exist;
{

	register char *p;	/* general use */
	char temp[sizeof(pwbuf)];
	extern char *sfind();
	FILE *iobuf;

	/* Open the "password" file */

	if ((iobuf = fopen(filename, "r")) == NULL) {
	    if (must_exist) {
		netreply("421 can't open \"%s\" (%s); try again later\r\n",
		    filename, errmsg(0));
		log_error("(getuser) can't open \"%s\"", filename);
		exit(-1);		/* fail exit */
	    }
	    else
		return (0);
	}

	/* Look thru the entries in the password or alias file for a match. */

	while (gets(iobuf, pwbuf))	/* Get entry from pwfile in pwbuf. */
	{
	    scopy(pwbuf, temp, NULL);	/* Copy into work area */
	    p = sfind(temp, ":");
	    *p = '\0';	/* Turn colon into null */
	    if (seq_nocase(rcvpath.p_username, temp)) {	/* Found username in pwbuf */
		fclose(iobuf);		/* return success */
		return(1);
	    }
	}
	fclose(iobuf);	        /* close password file */
	return(0);		/* Failure */
}
#endif DELIVERMAIL

/*
name:
	getmbox

function:
	return a file descriptor for a temporary mailbox

algorithm:
	create unique file name
	create file
	if can't
		signal error
	return mailbox file descriptor

parameters:
	none

returns:
	file descriptor of open mailbox file

globals:
	mfname

calls:
	loguser
	stat (sys)
	creat(sys)
	open (sys)
	seek (sys)

called by:
	mail
	datamail

history:
	initial coding 6/30/76 by S. F. Holmgren
	rewritten by greep for Rand mail system
	use fmodes for default creation mode jsq BBN 7-19-79

*/
FILE *
getmbox()
{
	register FILE * mboxfid;

	crname (mfname);       /* create mailbox file name */

	mboxfid = fopen(mfname,"w");
	if (mboxfid == NULL)
	{       netreply("450 Can't create \"%s\"; %s\r\n",mfname,errmsg(0));
		log_error("(getmbox) can't create \"%s\"", mfname);
	}
	return (mboxfid);
}
/*name:
	crname

function:
	create a unique file name

algorithm:
	use mktemp library routine

parameters:
	address of string where result is to be put

returns:
	nothing

globals:
	none

calls:
	mktemp (lib)

called by:
	getmbox

history:
	written by greep for Rand mail system
*/
crname(ptr)
register char *ptr;
{
	register char *p;
	register char *s = "/tmp/crmtpXXXXXX";

	p = tmpname;
	while (*p++ = *s++)
		;
	p = tmpname;
	mktemp(p);
	while (*ptr++ = *p++)
		;
}

/*name:
	mrsq, mrcp, mail_reset

algorithm:
	obvious

function:
	handle the MRSQ & MRCP protocols

history:
	initial coding by dm(bbn) 25 april, 1980 to handle these protocols
*/
mrsq()
{
	register int c;

	mail_reset();             /* Always reset stored stuff */
	if(arg == 0) {
		mrsqsw = MRSQ_D;        /* Back to default */
		netreply("200 OK, using default scheme (none).\r\n");
	}
	else switch (c = arg[0]) {
		case '?':
			netreply("215 T Text-first, please.\r\n");
			break;
		case 't':
		case 'T':
			mrsqsw = MRSQ_T;
			netreply("200 Win!\r\n");
			break;
		case '\0':
		case ' ':
			mrsqsw = MRSQ_D;	/* Back to default */
			netreply("200 OK, using default scheme (none).\r\n");
			break;
		default:
			mrsqsw = MRSQ_D;
			netreply("501 Scheme not implemented.\r\n");
			break;
	}
}

mrcp()
{
	register char *p;
	char rcpmbox[MBNAMSIZ];

	if (mrsqsw == MRSQ_D) {
		netreply("503 No scheme specified yet; use MRSQ.\r\n");
		return;
	}
	if (*mfname == '\0') {
		netreply("503 No stored text.\r\n");
		return;
	}
	/* parse destination arg */
	if (!veq_nocase (arg, "to:", 3)) {
		netreply("501 No recipient named.\r\n");
		return;
	}
	p = sfind (arg, ":") + 1;
	if (dstparse (p, &rcvpath) != 0) {
		netreply("501 Bad argument syntax.\r\n");
		return;
	}
	/*
	 * If destination is on this machine,
	 * then user must be known.
	 */
	if (rcvpath.p_first == NULL &&
	    rcvpath.p_hosts == NULL && findmbox() == 0) {
		netreply("553 User \"%s\" Unknown.\r\n", rcvpath.p_username);
		return;
	}
	crname(rcpmbox);/* create a temporary name for sndmsg to mung */
	if (link(mfname, rcpmbox) < 0) {
		netreply("451 can't link \"%s\" to \"%s\"; %s\r\n",
			mfname, rcpmbox, errmsg(0));
		log_error("(mrcp) can't link \"%s\" to \"%s\"",
			mfname, rcpmbox);
		return;
	}
	if (sndmsg(rcpmbox) == -1) {
		netreply("451 mail trouble, please try later.\r\n");
		unlink(rcpmbox);
		return;
	}
	else
		netreply("250 Mail delivered.\r\n");
}

/* if "mrcp r" ever gets implemented, this will scrub out */

mail_reset()
{               /* the array of names, when finished, i guess */
	/* if there is a temporary file left over from previous mrcp's */
	if(*mfname)
		unlink(mfname);     /* remove it-- */
	*mfname = '\0';
}

/*
name:
	findmbox

function:
	determine whether a mailbox exists

algorithm:
	if destination host div6net and this is outgoing mail,
		always indicate success, otherwise
	look first in user-to-home data base,
	if not found there, try password file
	if getuser doesn't find name in password file
		try in alias file

parameters:
	none

returns:
	1 if successful, 0 otherwise

history:
	initial coding 12/15/76 by greep for Rand mail system
	Modified by Eric Shienbrood(BBN): July 1981 to accept a
		user name terminated by a * as the name of a file into
		which mail should be deposited.  The file must be publicly
		writeable.
	Modified by Eric Shienbrood(BBN): August 1981 to always return
		success if DELIVERMAIL is #defined.

*/

findmbox()
{
#ifndef DELIVERMAIL
	register char *sp;
	char *uname;
	int i;
	char * lowercase();
	struct stat statb;

	sp = uname = rcvpath.p_username;
	while (*sp++ != '\0')
		;
	if (sp > &uname[1] && sp[-2] == '*') {  /* The "user" is a file */
		sp[-2] = '\0';
		/*
		 * To allow mail to be sent to a file,
		 * it must exist, it must be a regular
		 * file, and it must be publicly writeable.
		 */
		i = stat (uname, &statb);
		sp[-2] = '*';	/* Put back the * for sndmsg */
		if (i < 0)
			return (0);
		if ((statb.st_mode & S_IFMT) != S_IFREG)
			return (0);
		return (statb.st_mode & 02);
	}
	return (getuser(hmdbfile, 0) || getuser(pwfile, 1) || getuser(afile, 0));
#else
	return (1);
#endif DELIVERMAIL
}

/*
name:
	sndmsg

function:
	call sndmsg to deliver mail

algorithm:
	fork
	execute sndmsg with special argument

parameters:
	none

returns:
	status of sndmsg, -1 if couldn't execute

globals:
	username
	mfname

calls:
	fork (sys)
	exec (sys)

called by:
	mail

history:
	initial coding 12/15/76 by greep for Rand mail system
	long hosts, with kludge for short host sndmsg program jsq BBN 3-25-79
	sndmsg kludge removed jsq BBN 4-25-79
	dm(bbn) 4-25-80 take filename to send as an argument
*/
sndmsg(file)
char *file;
{
	int n;
	int status;
	char address[40];
	char returnto[40];
	char *uname, *flag;

	flag = "-netmsg";
	while((n = fork()) == -1)
		sleep(5);
	if (n == 0) {
		sprintf (returnto, "%s@%s", sndpath.p_username, sndpath.p_hostname);
		if (rcvpath.p_first == NULL && rcvpath.p_hosts == NULL)
			uname = rcvpath.p_username;
		else if (rcvpath.p_hosts == NULL) {
			sprintf(address, "%s@%s", rcvpath.p_username, rcvpath.p_hostname);
			uname = address;
		}
		else 
			exit(-1);	/* Can't handle this yet */
#ifdef DELIVERMAIL
		/*
		 * Make the mail file be
		 * delivermail's standard input.
		 */
		close(0);
		open(file, 0);
		execl(DELIVMAIL, "delivermail", "-a", uname, 0);
		log_error("(delivermail) can't execl \"%s\";", DELIVMAIL);
#else
		execlp(SNDMSG, SNDMSG, flag, uname, file, returnto, 0);
		log_error("(sndmsg) can't execl \"%s\";", SNDMSG);
#endif DELIVERMAIL
		exit(-1);
	}
	wait(&status);
	return (status>>8);
}

/*name:
	quit

function:
	handle the QUIT command

algorithm:
	say goodbye
	exit

parameters:
	none

returns:
	never

globals:
	none

calls:
	netreply
	exit (sys)

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
quit()
{
	mail_reset();   /* flush mail temporary files */
	netreply("221 Toodles, call again\r\n" );
	exit( 0 );
}

/*name:
	accept

function:
	to signal the current command has been logged and noted

algorithm:
	say command has been logged and noted

parameters:
	none

returns:
	nothing

globals:
	none

called by:
	called by main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
accept()
{
	netreply( "200 OK\r\n");
}
/*name:
	help

function:
	give a list of valid commands

algorithm:
	send list

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	netreply

called by:
	called by main thru the help command

history:
	greep
	altered by dm (12/3/79) to permit automatic addition of commands
	   (someone industrious should someday make help accept an argument,
		like the protocol says it does...)
*/
help()
{
	register i;
	register struct comarr *p;

	netreply("214-The following commands are accepted:\r\n" );
	for(p = commands, i = 1; p->cmdname; p++, i++)
		netreply("%s%s", p->cmdname, ((i%10)?" ":"\r\n") );

	netreply("\r\n214 Please send complaints/bugs to %s\r\n", BUGS);
}
/*name:
	netreply

function:
	send appropriate ascii responses

algorithm:
	get the length of the string

	send it to the standard output file

parameters:
	str to be sent to net

returns:
	nothing

globals:
	none

calls:
	write (sys)

called by:
	the world

history:
	initial coding 4/13/76 by S. F. Holmgren
	replaced by fdprintf facility to permit greater
		versatility in replies (e.g.,
		including errmsg, etc.) dm/bbn Apr 10, '80
*/
netreply(printargs)
char *printargs;
{
	char replyline[256];
	int len;

	sprintf(replyline, "%r", &printargs);

	if(write(1,replyline, len = slength(replyline)) < 0){
		fprintf(ERRFD,"(netreply) error in writing [");
		fprintf(ERRFD, "%r", &printargs);
		fprintf(ERRFD,"]; %s\n", errmsg(0));
		go_die(ERRFD, "\n");
	}
#ifdef DEBUG
	if (len >= 2 && replyline[len - 2] == '\r') {
		replyline[len - 2] = '\n';
		replyline[len - 1] = '\0';
	}
	fprintf (ERRFD, ":%d: %s", mypid, replyline);
#endif DEBUG
}

/* write error messages out to the log file */
log_error(printargs)
char *printargs;
{
	fprintf(ERRFD, "%r", &printargs);
	fprintf(ERRFD, "; %s\n", errmsg(0));
}

/* clean up mail stuff and call die */
go_die(n, args)
int n; char *args;
{
	mail_reset();
	die(n, &args);
}

/*name:
	gets

function:
	get characters from iobuffer until a newline is found

algorithm:
	while get a character
		if char is new line
			stuff in a null
			return success

	return failure

parameters:
	iobuffer - address of an iobuf see getc unix prog manual
	buf      - addres of character array to put data into

returns:
	null terminated line

globals:
	none

calls:
	getc (sys)

called by:
	getuser

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

#ifndef DELIVERMAIL
gets( iobuffer,destbuf )
FILE *iobuffer;
char *destbuf;
{

	register int c;
	register char *outp;
	register FILE *iobuf;

	iobuf = iobuffer;
	outp = destbuf;

	while( (c = getc( iobuf )) > 0 )
		if( c == '\n' )
		{
			*outp = CNULL;	 /* null terminate */
			return( 1 );	/* return success */
		}
		else
			*outp++ = c;	/* just bump to next spot */

	/* return failure */
	return( 0 );
}
#endif DELIVERMAIL

/*
 * Uppercase a string in place. Return pointer to
 * null at end.
 */
char *
getuc(s)
    char *s;
{
    register char *p,
		  c;
    for (p = s; c = *p; p++)
    {
	if (c <= 'z' && c >= 'a')
	    *p -= ('a' - 'A');
    }
    return(s);
}
