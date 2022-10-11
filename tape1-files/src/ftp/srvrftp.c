#include "srv.h"
/*
 *   long hosts jsq BBN 3-15-79; note short host sndmsg is still expected.
 *   long host sndmsg is expected now jsq BBN 4-25-79
 * Modified by Dan Franklin (BBN) August 11, 1979: to call nsndmsg.
 * put all host name-number map calls in ftpsrv jsq BBN 18Aug79.
 * keep both old and new sndmsg calls, add die calls and error reporting
 *   jsq BBN 21Aug79
 * Modified by Dan Franklin (BBN) September 20, 1979: to put error message
 *   in when ***sender closed connection***, etc is put in msg.
 *   mail() changed.
 * Modified by Dan Franklin (BBN) November 1, 1979: increased size of pwbuf
 *   to 512 characters.
 * Do the same for username jsq BBN 27Nov79.
 * set home directory jsq BBN 18Dec79.
 * Modified by dm 5 Jan 80 to permit the person to talk to us on an upper-
 *   case only tty.  This required only a change to the getuser routine.
 * Modified by dm (BBN) Mar 15 80 to implement the multiple store
 *     functionalities of the user ftp, to implement tree-structure storage
 *   changed help() to be more or less self-documenting.  someone industrious
 *     should make help accept an argument, just like the protocol says it does
 *   changed netreply() to be defined as fdprintf, which permits one to use
 *     printf-string formatting.  [used fdprintf (1, ...), rather than printf,
 *     because fdprintf returns -1 on error
 * Modified by dm (BBN) Apr 10, 1980: another pass at the netreply() problem...
 *   made netreply use fdprintf (1,"%r", &args), and generally cleaned up
 *     that approach
 *   flushed numreply in favor of improved netreply
 *   improved almost all of the responces, taking advantage of the improved
 *     netreply
 *   added error-logging to ftplog on failure to find important files, and
 *     on other error conditions that shouldn't happen in the first place,
 *     so people unfamiliar with this program can figure out why it doesn't
 *     work when they install it elsewhere, etc.
 *   put the names of the system files srvrftp accesses to the beginning,
 *     so they are easy to find.
 * dm (bbn) Apr 15, 1980: fixed NLST, so it obeys the protocol (RFC 542).
 * dm (bbn) Apr 25, 1980: added the XRSQ & XRCP protocols (RFC 743)
 *   lifted some code from KLH@ai, but not much, since their mail system
 *     is drastically different from ours.
 *   also, modified getline() to set arg to 0 if the argument is all blanks
 *     it used to give you a pointer to the null, instead, so the if a user
 *     typed "mail ", arg would be non-zero.
 * dm (bbn) May 15, 1980: increased the allocation on data connections to
 *   more efficiently squeeze things through the pli
 * Modified getuser to use tpass when looking up passwords.
 *	pwbuf now holds the virgin password file line as read in by getuser;
 *	loguser changed accordingly, simplified.
 * Also moved "type" and "mode" commands to behind the NOLOGCOM fence so
 *	users can MLFL without having to log in first.
 *  BBN (dan) May 25, 1980
 * dm (bbn) 10 June, 1980: call die through go_die() so the mail_reset()
 *    routine can be called on errors.  also, do a mail_reset() on reading
 *    an EOF on the standard input to flush any xrsq/xrcp text.
 * dm (bbn) 11 August, 1980: fixed dataconnection() & sendsock() to do
 *     server+2, server+3, and user+4, user+5 sockets on dataconnections
 * dm (bbn) 18 October, 1980: moved "stru" and "byte" above the NOLOGCOM
 *     fence, so Tenex users can MLFL without having to log in first.
 *     also moved "rest" (restart) and "allo" there, too, since they 
 *     seem harmless, and i'm sure some system in the world uses them 
 *     in sending mail...
 * dm (bbn) 18 November, 1980:
 *      added the XPWD command
 * dm (bbn) 1 December, 1980:
 *      converted to be either TCP or NCP FTPs...
 *
 *      dataconnection now has two arguments: the offset plus a pointer
 *              to a buffer full of network information
 *              it returns the buffer filled with the information
 *      do_list() now sets up a pipe through which the system-programs
 *              (ls, in some flavor) passes its data, to be written on
 *              the net by the parent fork.
 * rfg (bbn) 21 October, 1981: deleted sendsock per TCP FTP spec
 */

/*name:
	main

function:
	ARPA network server ftp program

	Takes commands from the assumed network connection (file desc 0)
	under the assumption that they follow the ARPA network file
	transfer protocol NIC doc #10596 RFC 354 and appropriate modifications.
	Command responses are returned via file desc 1.

algorithm:
	process structure:

	      There is a small daemon waiting for connections to be
	      satisfied on socket 3 from any host.  As connections
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
	 (any of the procedure in the commands array)

called by:
	small server daemon program


history:
	initial coding 4/12/76 by S. F. Holmgren
	  Changed U4, U5 to be off base instead of telnet
		    Dan Franklin (BBN) August 23, 1978
	  Changed (temporarily) to use crypt2 instead of crypt
	  for big mother password encryption changeover
		    Dan Franklin (BBN) August 23, 1978
	  Changed back to crypt, now that it is the standard (dan:BBN)
	  The listing command now execs the shell, so * works.
	long hosts jsq BBN 3-20-79
	change error handling jsq BBN 20Aug79

*/
char * syntactify();		/* forward declarations. */

main (argc, argv)
char **argv;
{
    register struct comarr *comp;
    char   *i;
    long    atime;
    char   *getline ();

    setbuf (ERRFD, NULL);
    fprintf (ERRFD, "srvrftp started\n");

    signal (SIGQUIT, SIG_IGN);	/* convenient for debugging */

    signal (SIGINT, SIG_IGN);
    inherit_net (&NetParams, 0);
    get_stuff (&NetParams);	/* need connection parameters */
    progname = argv[0];
    if (argc != 3)
    {

	fprintf (ERRFD, "%s:  wrong number of arguments\n", progname);
	netreply
	(
	    "451 %s has been incorrectly invoked by ftpsrv!\r\n",
	    progname
	);
	exit (-1);
    }

    them = argv[1];
    us = argv[2];
    time (&atime);
    fprintf (ERRFD, "%s %s %s", progname, them, ctime (&atime));


 /* set defaults for transfer parameters */

    type = TYPEA;		/* ascii */
    mode = MODES;		/* stream */
    stru = STRUF;		/* file structured */

 /* init NOLOGCOM */

    NOLOGCOMM = &commands[LASTNOLOGCOM];

 /* say we're listening */
    netreply ("220 %s Experimental Server FTP\r\n", argv[2]);
/*
*/
nextcomm: 
    while (i = getline ())
    {
	if ((int) i < 1)	/* handle error */
	    go_die ("net input read error;");

	/* find and call command procedure */
	comp = commands;
	while (comp -> cmdname)	/* while there are names */
	{
	    if (!strcmp (buf, comp -> cmdname))/* a winner */
	    {
		if (comp <= NOLOGCOMM || cmdstate == EXPECTCMD)
		    (*comp -> cmdfunc) ();	/* call comm proc */
		else
		{
		    if (cmdstate == EXPECTPASS)
		    {
			netreply ("530 Give me your password, chucko\r\n");
		    }
		    else
		    {
			netreply ("530 Log in with USER and PASS\r\n");
		    }
		}
		goto nextcomm;	/* back for more */
	    }
	    comp++;		/* bump to next candidate */
	}
	fprintf (ERRFD, "Bad command: %s", buf);
	netreply ("500 I never heard of that command before\r\n");
    }
    mail_reset ();		/* flush any xrcp/xrsq stored text */
}

/*name:
	getline

function:
	get commands from the standard input terminated by <cr><lf>.
	afix a pointer (arg) to any arguments passed.
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
		check for carraige return
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
	changed no more line condition from "read (..net..) < 0" to
		"read (..net..) <= 0" 5Oct78 S.Y. Chiu
	change error handline jsq BBN 20Aug79
*/
char * getline()
{

    register char  *inp;	/* input pointer in netbuf */
    register char  *outp;	/* output pointer in buf */
    register int    c;		/* temporary char */
    int     cmdflag;		/* looking for telnet command */
    extern char *progname;

    inp = netptr;
    outp = buf;
    arg = 0;
    cmdflag = 0;

    do
    {
	if (--netcount <= 0)
	{
	    do {
		if ((netcount = net_read(&NetParams, netbuf, sizeof netbuf)) <= 0) {
			if (errno == ENETSTAT) {
				get_stuff(&NetParams);
				if (NetParams.ns.n_state&(URXTIMO+UURGENT)==0)
					return((char *)netcount);
			} else
				return(-1);
		}   
	    } while (netcount < 0);
	    inp = netbuf;
	}
/*
 */
	c = *inp++ & 0377;
	if (cmdflag)		/* was last char IAC? */
	{
	    cmdflag = 0;	/* if so ignore this one too */
	    c = 0;		/* make sure c != \n so loop won't */
	    continue;		/*  end */
	}

	if (c == TNIAC)		/* is this a telnet IAC? */
	{
	    cmdflag++;		/* if so note that next char */
	    continue;		/*  to be ignored */
	}

	if (c == '\r' ||	/* ignore CR */
		c >= 0200)	/* or any telnet codes that */
	    continue;		/*  to sneak through */

	if (just_con_data == 0 && arg == 0)
	{

	    /* if char is a delim afix token */
	    if (c == ' ' || c == ',')
	    {
		c = NUL;	/* make null termed */
		arg = outp + 1;	/* set arg ptr */
	    }
	    else

		/* do case mapping (UPPER -> lower) */
		if (isupper(c))
		    c = tolower(c);
	}
	*outp++ = c;
    }

    while (c != '\n' && outp < &buf[BUFL]);

    if (just_con_data == 0)
	*--outp = 0;		/* null term the last token */

    /* scan off blanks in argument */
    if (arg)
    {
	while (*arg == ' ')
	    arg++;
	if (*arg == '\0')
	    arg = 0;		/* if all blanks, no argument */
    }

    netptr = inp;		/* reset netptr for next trip in */

    return (outp);		/* return success */
}
/*name:
	retrieve

function:
	implement the retr command for server ftp
	take data from the local file system and ship it
	over the network under the auspicies of the mode/type/struct
	parameters

algorithm:
	fork off a data process
		try and open the local data file
			signal error
			exit
		send sock command
		open the data connection
		send the data on the u+5,u+4 socket pair

parameters:
	none

returns:
	nothing

globals:
	arg

calls:
	spawn
	open (sys)
	dataconnection
	senddata

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	fork changed to fork1 by greep
	and to frog jsq BBN 18Aug79
*/

retrieve()
{

    FILE   *localdata;		/* file id of local data file */
    struct net_stuff    netdata;/* file id of network data file */

    if (chkguest (0) == 0)
	return;

    if ((lastpid = frog ()) == 0)
    {					/* child comes here */

	if ((localdata = fopen (arg, "r")) == NULL)
					/* open file for read */
	{
	    netreply
		("%s can't open \"%s\"; %s\r\n",
		    (errno == EACCES
			? "450"
			: (errno == ENOENT
			    ? "550"
			    : "451"
			)
		    ),
		    arg, errmsg (0)
		);
	    exit (-1);
	}

	/* Open data connection */
	dataconnection (U5, &netdata);

	/* say transfer started ok */
	netreply ("150 Retrieval of \"%s\" started okay\r\n", arg);

	/* send data according to params */
	senddata (localdata, &netdata, netreply);
	net_pclose(&netdata);

	/* say transfer completed ok */
	netreply ("226 File transfer completed okay\r\n");

	exit (0);
    }
}
#ifdef TREE
/*name:
	xmkd

function:
	make a subdirectory of the current working directory

algorithm:
	if the file exists
		if it is a directory,
			change to it via cwd()
		else complain
	else
	do a fork
		fork execl's the standard mkdir
	cwd() to change to the directory
	else complain that you can't do it

parameters:
	local directory name to open (arg)

returns:
	nothing

globals:
	arg

calls:
	execl (sys)
	stat (sys)

called by:
	main thru command array

history:
	Fixed to properly check if dir by Dan Franklin (BBN) May 25, 1980

*/
mkd()
{
    struct stat    statbuf;
    int     ps;
    char    path[QUOTDSIZ];
    register    i;

    ps = 0;
    if (chkguest (1) == 0)
	return;

 /* check if the directory exists */
    if (stat (arg, &statbuf) >= 0)
    {
	if ((statbuf.st_mode & S_IFMT) == S_IFREG)
	{

    /* directory exists */
	    abspath (arg, path, &path[QUOTDSIZ]);
	    netreply
		(
		    "251 \"%s\" directory already exists; taking no action\r\n",
		    syntactify (path)
		);
	    return;
	}
	else
	{
	    netreply
	    ("450 file name \"%s\" exists as non-directory\r\n", arg);
	    return;
	}
    }
/*
 */
 /* create new directory */
    if ((i = fork ()) == 0)
    {
	/* don't let any error messages clobber the user program */
	close (1);
	close (0);
	execlp (MKDIR, "mkdir", arg, 0);
	log_error ("(mkd) can't execl \"%s\"", MKDIR);
	exit (-1);		/* execl failed */
    }
    if (i > 0)
    {
	while (i != wait (&ps));
	if (ps >> 8)
	{
	    netreply ("\r\n451 unable to create directory\r\n", arg);
	    return;
	}
	abspath (arg, path, &path[QUOTDSIZ]);
	netreply ("\r\n251 \"%s\" directory created\r\n",
		syntactify (path));
	return;
    }
    else
    {
	netreply ("\r\n451-unable to fork to make directory");
	netreply ("\"%s\"; %s\r\n451 try again later\r\n", arg, errmsg (0));
    }
}
/*
 */
char *
syntactify (str) /* if str has a " in it, double the " */
char * str;
{				/* NOTE: returns pointer to a static
				   character array */
    static char quoted[QUOTDSIZ];
    register char  *p,
                   *d;
    p = str;
    d = quoted;

    do
    {
	if (*p == '"')
	    *d++ = '"';
	*d++ = *p++;
    } while (*p && d < &(quoted[QUOTDSIZ]));

    *d = '\0';
    return (quoted);
}
/*name:
	rmdir

function:
	removes a directory

algorithm
	fork a process which execls the systems rmdir

parameter
	just arg

returns:
	nothing

globals:
	arg

calls:
	nothing

called by
	main

history:
	initial coding by dm@bbn-unix Mar 27, 1980
*/
rmdir()
{
    int     child,
            ps;

    ps = 0;
    if ((child = fork ()) < 0)
    {
	netreply
	(
	    "451 can't fork process to remove \"%s\"; %s\r\n",
	    arg, errmsg (0)
	);
	log_error ("(rmdir) fork error, removing \"%s\"", arg);
    }
    else
	if (child == 0)
	{
	    /* don't let any error messages clobber the user program */
	    close (0);
	    close (1);
	    execlp (RMDIR, "rmdir", arg, 0);
	    netreply
	    (
		"\r\n451 can't execl \"%s %s\"; %s\r\n",
		RMDIR, arg, errmsg (0)
	    );
	    log_error ("(rmdir) can't execl \"%s %s\"", RMDIR, arg);
	    exit (-1);
	}
	else
	{
	    while (child != wait (&ps));
	    if (ps >> 8)
	    {
		netreply ("\r\n451 could not remove \"%s\"\r\n", arg);
	    }
	    else
		netreply ("\r\n250 directory removed\r\n");
	}
}
/*name:
	xpwd

function:
	print working directory

algorithm:
	do an abspath of "."

parameters:
	none

returns:
	none

globals:
	none

calls:
	abspath         syntactify
	netreply

called by:
	main through command array

history:
	initial coding by dm (bbn) 18 November, 1980
*/
xpwd()
{

    char path[QUOTDSIZ];
    abspath (".", path, &path[QUOTDSIZ]);

    netreply ("251 \"%s\" is the current working directory\r\n",
	syntactify (path));
}
/*name:
	xcup

function:
	change to parent of current working directory

algorithm:
	point 'arg' to the string ".." and call cwd()

parameters:
	none

returns:
	nothing

globals:
	arg

calls:
	cwd

called by:
	main thru command array

history:
	initial coding by dm (BBN/sandy ego) 19 Oct. 1979
*/

xcup()
{
    arg = "..";
    cwd();
}

/* tha's all, folks! */

#endif TREE
/*name:
	store

function:
	receive data from the network and store upon the local file
	system

algorithm:
	fork a data process
		try and create a local file
			signal error
			exit
		send sock command
		open data connection
		receive data

parameters:
	local file name to open (arg)

returns:
	nothing

globals:
	arg

calls:
	spawn
	creat (sys)
	dataconnection
	rcvdata

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	fork changed to fork1 by greep
	changed by greep to check success from rcvdata
	default file modes corrected jsq BBN 7-19-79
	fork1 to frog jsq BBN 18Aug79
*/

store()
{

    FILE     *localdata;		/* file id for local data file */
    struct net_stuff    netdata;/* file id for network data file */

    if (chkguest (1) == 0)
	return;

    if ((lastpid = frog()) == 0)
    {

	/* create new file */
	if ((localdata = fopen (arg, "w")) == NULL)
	{
	    netreply ("451 can't create \"%s\"; %s\r\n", arg, errmsg (0));
	    exit (-1);
	}

	/* open data connection */
	dataconnection (U4, &netdata);

	/* say transfer started ok */
	netreply ("125 Storing \"%s\" started okay\r\n", arg);

	/* receive and translate data according to params */
	if (rcvdata (&netdata, localdata, netreply) >= 0)
	    netreply ("226 File transfer completed okay\r\n");

	exit (0);
    }
}

/*name:
	append

function:
	append data from the net to an existing file (file is created if
	it doesnt exist.

algorithm:
	fork a data process
		try and open the file
			try and creat the file
				signal error
				exit
		seek to the end of the file
		send a sock command
		open the data connection
		receive the data

parameters:
	file name to append/create

returns:
	nothing

globals:
	arg

calls:
	spawn
	open (sys)
	creat (sys)
	seek (sys)
	dataconnection
	rcvdata

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	fork changed to fork1 by greep
	changed by greep to check success of rcvdata
	use fmodes for default creation mode jsq BBN 7-19-79
	fork1 to frog jsq BBN 18Aug79
*/

append()
{

    FILE   *localdata;		/* file id of local data file */
    struct net_stuff    netdata;/* file id of network data file */

    if ((lastpid = frog ()) == 0)
	return;

    if ((lastpid = frog ()) == 0)
    {

	/* try and open file -- it may exist */
	if ((localdata = fopen (arg, "a")) == NULL)
	{
	    netreply
		(
		    "451 can't create \"%s\"; %s\r\n",
		    arg, errmsg (0)
		);
	    exit (-1);
	}

	/* open data connection */
	dataconnection (U4, &netdata);

	netreply ("125 Append to \"%s\" started correctly\r\n", arg);

	/* rcv and translate according to params */
	if (rcvdata (&netdata, localdata, netreply) >= 0)
	    netreply ("250 Append completed okay\r\n");

	exit (0);

    }
}

/*name:
	user

function:
	handle USER command

algorithm:
	max login tries exceeded
		signal error
		exit

	expecting user command
		signal error
		reset state
		return

	save user name
	request password

parameters:
	none

returns:
	nothing

globals:
	cmdstate=
	logtries=
	username=
	arg

calls:
	netreply
	strcpy

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

user()
{

    if (cmdstate != EXPECTUSER)
    {
	netreply ("504 User command unnecessary\r\n");
	return;
    }

    if (logtries++ > MAXLOGTRIES)
    {
	netreply ("430 Login attempts exceeded, go away\r\n");
	exit (-1);		/* error exit */
    }

    /* set state to expecting a PASS command */
    cmdstate = EXPECTPASS;

    /* save the username in username buf */
    strcpy (username, arg);

    /* ask him for password */
    netreply ("331 Enter PASS Command\r\n");
}

/*name:
	pass

function:
	handle the PASS command

algorithm:
	do we know this user
		signal error
		set state to EXPECTUSER
		return
	do appropriate logging
	say he is logged in
	check for first 3 chars of user name "ftp" - if so, guest user

parameters:
	none

returns:
	nothing

globals:
	cmdstate=
	guest

calls:
	getuser
	netreply
	loguser

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	modified by greep to check for guest user

*/

pass()
{
    int     guestflg;

    if (cmdstate != EXPECTPASS)
    {
	netreply ("503 I am not expecting a password\r\n");
	return;
    }

    guestflg = (username[0] == 'f' && username[1] == 't' &&
	    username[2] == 'p');

    /* get the users password entry */
    if (getuser (1, pwfile) == 0)
    {
	/* unknown user */
	netreply ("530 I don't know you, try again\r\n");
	cmdstate = EXPECTUSER;	/* waiting for user command */
	return;
    }

    /* got user */
    cmdstate = EXPECTCMD;

    /* do appropriate accounting and system info work */
    loguser ();
    netreply ("230 User Logged in\r\n");

    if (guest = guestflg)
	netreply ("211 FTP guest user - restricted access\r\n");
}

/*
name:
	getuser

function:
	getuser: find a match for the user in username and password in arg
	  in /etc/passwd

parameters:
	user name in username password in arg
	name of file to look in

returns:
	1 if user found
	0 failure

	pwbuf containing the line from the passwd or alias file which matched

history:
	initial coding 4/12/76 by S. F. Holmgren
	altered 5 Jan 80 by Dave Mankins to permit UPPERCASED arguments to
	  the PASS command to work
	modified 25 May 80 by Dan Franklin to use new tpass routine,
	    clean up
 */
getuser (user_pass, filename)
    char *filename;
{

	register char *p;	/* general use */
	register int  retval;	/* return value: 1 success, 0 failure */
	char temp[sizeof (pwbuf)];
	char * lowercase();
	char * colon();
	FILE *iobuf;

/* Open the "password" file--if the pointer is to the alias-file, */
/* and you can't get at it, try opening the old one... */

	if (((iobuf = fopen (filename, "r")) == NULL)
	     && (filename == afile)
	     && (iobuf = fopen (old_afile, "r")) == NULL)
	{
	    netreply ("451 can't open \"%s\" (%s); try again later\r\n",
		filename, errmsg (0));
	    log_error ("(getuser) can't open \"%s\"", filename);
	    exit (-1);		    /* fail exit */
	}

/* Look thru the entries in the password or alias file for a match. */

	retval = 0;			/* Assume failure. */

	/* Get entry from pwfile in pwbuf. */
	while (fgets (pwbuf, sizeof pwbuf, iobuf) != NULL)
	{
	    strncpy (temp, pwbuf, sizeof temp);	/* Copy into work area */
	    *((p = colon (temp))-1) = '\0';	/* Turn colon into null */
	    if (seq_nocase (username, temp))    /* Found username in pwbuf */
	    {
		if (!user_pass) /* Is that all? */
		    retval++;			    /* return success */
		else
		{
		    *(colon (p)-1) = '\0';
		    		/* Terminate password field with null */
		    if (tpass (arg, p, NUL) 
		        || tpass (lowercase (arg, arg), p, NULL)
		      )
		      retval++;
		}
		break;
	    }
	}
	fclose (iobuf);		      /* close password file */
	return (retval);
}

/*
  name:
    lowercase

  function:
    convert a string to lower-case

  parameters:
    input and output arrays

  returns:
    pointer to output array
*/

char *
lowercase (in, out)
    register char * in;
    register char * out;
{
    char * retval;
    register char c;

    retval = out;

    while (c = *in++)
	*out++ = isupper(c) ? tolower(c) : c;

    return retval;
}

/*
  name:
	loguser

function:
	perform installation/system specific login/accounting functions

parameters:
	user information in pwbuf

returns:
	nothing

history:
	initial coding 4/12/76 by S. F. Holmgren
	order of setuid and setgid switched by greep to make them work
	set homedir jsq BBN 18Dec79
	use pwbuf format as set by new getuser, eliminate argument dan BBN 25 May 80
*/

loguser()
{
	register char *p;		/* general usage */
	int *(oldsig());
	char * colon();

/* If there is nothing in pwbuf, forget it. */

	if (pwbuf[0] == '\0')
	    return;

	p = colon (colon (pwbuf));    /* Set passentp to userid field. */
	curuid = atoi (p);	    /* Set user id. */
	p = colon (p);		    /* Set passentp to groupid field. */
	curgid = atoi (p);	    /* Set group id. */
	p = colon (colon (p));	    /* Set passentp to home directory. */
	*(colon (p)-1) = '\0';	    /* Null terminate. */
	chdir (p);		    /* Set working dir to it. */

/* Also set login directory, if that system call is available. */

/*	oldsig = signal (SIGSYS, SIG_IGN);
	chlogdir (p);
	signal (12, oldsig); */

	setgid (curgid); 	    /* Set group id */
	setuid (curuid); 	    /* Set user id */
}

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
	creat (sys)
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
FILE *getmbox()
{
	register FILE *mboxfid;

	crname (mfname);       /* create mailbox file name */

	mboxfid = fopen (mfname,"w");

	if (mboxfid == NULL)
	{
		netreply
		    ("450 Can't create \"%s\"; %s\r\n",mfname,errmsg (0));
		log_error ("(getmbox) can't create \"%s\"", mfname);
		return (NULL);
	}
	return (mboxfid);
}
/*name:
	crname

function:
	create a unique file name

algorithm:
	increment count
	if count is 256
		sleep one second to make time different
	get date-time, process number, and count
	convert to hexadecimal

parameters:
	address of string where result is to be put

returns:
	nothing

globals:
	hex

calls:
	getpid (sys)
	sleep (sys)
	time (sys)

called by:
	getmbox

history:
	written by greep for Rand mail system
*/
crname (ptr)
char *ptr;
{
    int     i;
    int     tvec[4];
    static int  filecnt;
    register char  *p,
                   *q;

    p = "/tmp/";
    q = ptr;
    while (*q++ = *p++);
    --q;
    p = (char *) & tvec[0];
    tvec[2] = getpid ();
    tvec[3] = filecnt++;
    if (filecnt == 256)
    {
	filecnt = 0;
	sleep (1);
    }
    time (tvec);
    for (i = 7; i; --i)
    {
	*q++ = hex[(*p >> 4) & 017];
	*q++ = hex[*p++ & 017];
    }
    *q = '\0';
}

/*name:
	xsen and xsem

function:
	handle XSEN and XSEM commands
	xsen: display message on user's tty (s)
	xsem: same as xsen but if unsuccessful then mail instead

algorithm:
	call display
	[xsem only] if not successful, call mail

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	display
	netreply
	mail

called by:
	main through command array

history:
	written by greep to support ITS hackery
*/
xsen()
{
   if (display())
	netreply ("450 User \"%s\" not logged in\r\n", arg);
}


xsem()
{
   if (display())
	mail();
}
/*name:
	display

function:
	reads data from telnet stream and displays on all terminals
	on which user is logged in and has terminal writeable

algorithm:
	see whether user is logged in and has tty writeable
	if so
		for each such tty
			copy message to tty

parameters:
	none

returns:
	0 if successful
	1 otherwise

globals:
	arg
	username
	ttyname

calls:
	open (sys)
	close (sys)
	read (sys)
	write (sys)
	stat (sys)
	getline
	netreply

called by:
	xsen
	xsem

history:
	written by greep
	change error handling jsq BBN 20Aug79
*/
char ttyname[] = "/dev/ttyx";

display()
{
    struct
    {
	char    logname[8];
	char    logtty;
	long int    logtime;
	int     dummy;
    }       wholine;
    register char  *p;
    register int    i;
    int     ttyfds[100];	/* there can't possibly be this many */
    int     ttycnt;
    int     whofd;

/*
*/
    strcpy (username, arg);	/* stick user arg in username */
    for (p = username; *p; p++)
    {
	if (*p >= 'A' && *p <= 'Z')
	    *p |= ' ';		/* convert to lower case if req */
    }

    ttycnt = 0;
    if ((whofd = open (UTMP, 0)) < 0)
    {
	log_error ("(display) can't open \"%s\"", UTMP);
	return (-1);
    }

    while (read (whofd, &wholine, sizeof wholine) == sizeof wholine)
    {
	if (wholine.logname[0] == '\0')
	    continue;		/* this entry is empty */
	for (i = 0; i < 8; i++)
	    if (wholine.logname[i] != username[i])
		break;
	if (username[i] == '\0' &&
		(i == 8 || wholine.logname[i] == ' '))
	{

	    ttyname[8] = wholine.logtty;

	    if (ttycnt < 100 && stat (ttyname, &statb) >= 0 &&
		    (statb.st_mode & S_WRLD (S_IWRITE)) &&
		    (ttyfds[ttycnt] = open (ttyname, 1)) >= 0)
		ttycnt++;	/* It it all works, use it */
	}
    }
/*
*/
    close (whofd);
    if (ttycnt <= 0)
	return (1);		/* Indicate failure */

    netreply ("354 Enter text, end by a line with only '.'\r\n");

    for (i = 0; i < ttycnt; i++)
	write (ttyfds[i], "\007Message from Network...\n", 25);
    just_con_data = 1;		/* tell getline only to drop cr */
    while (p = getline ())
    {
	if (p <= 0)
	    return (1);

	 /* are we done */
	if (buf[0] == '.' && buf[1] == '\n')
	    break;		/* yep */

	for (i = 0; i < ttycnt; i++)
	    write (ttyfds[i], buf, (p - buf));
    }

    just_con_data = 0;		/* set getline to normal operation */
    for (i = 0; i < ttycnt; i++)
    {
	write (ttyfds[i], "<EOT>\n", 6);
	close (ttyfds[i]);
    }

    netreply ("250 Message displayed successfully\r\n");
    return (0);			/* Indicate success */
}
/*name:
	mail

function:
	handle the MAIL <user> command over the command connection

algorithm:
	see if we have a known user

	if mailbox file cant be gotten
		return
	tell him it is ok to go ahead with mail

	while he doesnt type a period
		read and write data
	say completed

parameters:
	username in arg

returns:
	nothing

globals:
	arg
	username=

calls:
	strcpy
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
	modified 6/30/76 by S. F. Holmgren to call get~mbox
	modified 10/18/76 by J. S. Kravitz to improve net mail header
	modified by greep for Rand mail system
	long host jsq BBN 3-20-79
	error handling jsq BBN 20Aug79
	modified 4/25/80 by dm (BBN) for xrsq/xrcp support
*/
mail()
{
    register FILE  *mboxfid;	/* fid for temporary *mfname file */
    register char  *p;		/* general use */
    int     tyme[2];		/* for the time */
    int     werrflg;		/* keep track of write errors */
    int     errflg;		/* And other errors (dan) */
    int     keep;		/* flag for rsq stuff */
    extern char *errmsg ();

    keep = 0;
    errflg = 0;
    werrflg = 0;

    mail_reset ();		/* unlink any left over temporary files */

    if (arg == 0)
    {				/* make sure there is a user name */
	keep++;
	if (xrsqsw == XRSQ_D)
	{			/* unless one is not necessary */
	    netreply ("502 no recipient named\r\n");
	    return;
	}
    }
    else
    {
	strcpy (username, arg);	/* stick user arg in username for getuser 
				*/
	if (findmbox () == 0)
	{
	    netreply ("553 User \"%s\" Unknown\r\n", arg);
	    return;
	}
    }
 /* get to open mailbox file descriptor */
    if ((mboxfid = getmbox ()) < 0)
	return;

 /* say its ok to continue */
    netreply ("354 Enter Mail, end by a line with only '.'\r\n");

    just_con_data = 1;		/* tell getline only to drop cr */
/*
*/
    for (;;)
    {				/* forever */
	if ((p = getline ()) < 0)
	{
	    fprintf (mboxfid, "\n***Sender closed connection***\n");
	    errflg++;
	    break;
	}
	if (p == NULL)
	{
	    fprintf
		(mboxfid,
		    "\n***Error on net connection:  %s***\n",
		    errmsg (0)
		);
	    if (!errflg++)
		log_error ("(mail) errflg++");
	    break;
	}

 /* are we done */
	if (buf[0] == '.' && buf[1] == '\n')
	    break;		/* yep */

 /* If write error occurs, stop writing but keep reading. */
	if (!werrflg)
	{
	    if (fwrite (buf, (p - buf), 1, mboxfid) < 0)
	    {
		werrflg++;
		log_error ("(mail) werrflg++");
	    }
	}
    }
    just_con_data = 0;		/* set getline to normal operation */
/*
 */
    if (errflg)
    {
	time (tyme);
	fprintf
	    (mboxfid,
		"\n=== Network Mail from host %s on %20.20s ===\n",
		them, ctime (tyme)
	    );
    }

#ifdef ADDITIONAL_STUFF
    time (tyme);
    fprintf
	(mboxfid, "\n--- Network mail ended on %20.20s ---\n",
	    ctime (tyme)
	);
#endif ADDITIONAL-STUFF

    fclose (mboxfid);
    if (werrflg)
    {
	netreply ("452-Mail trouble (write error to temporary file)\r\n");
	netreply ("452 (%s); try again later\r\n", errmsg (0));
	mail_reset ();		/* delete temporary file */
	return;
    }
    if (xrsqsw == XRSQ_T && keep)
    {
	netreply ("250 Mail stored\r\n");
	return;
    }
    if (sndmsg (mfname) == -1)	/* call sndmsg to deliver mail */
	netreply ("451 Mail trouble (sndmsg balks), try later\r\n", errmsg (0));
    else
	netreply ("250 Mail Delivered \r\n");
    mail_reset ();		/* clean up (delete temporary file,
				   *mfname) */
}
/*name:
	datamail

function:
	handle the MLFL command

algorithm:
	fork
		make sure we have a valid user
			say bad user and exit
		send sock command
		open data connection
		get open mailbox file descriptor
		call rcvdata to receive mail

parameters:
	username in arg

returns:
	nothing

globals:
	arg

calls:
	spawn
	strcpy
	findmbox
	netreply
	dataconnection
	getmbox
	rcvdata
	printf (sys)
	time (sys)

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren
	modified 10/18/76 by J. S. Kravitz to put net mail header
	modified by greep for Rand mail system
	changed by greep to check success of rcvdata
	modified 4/25/80 by dm (BBN) for xrsq/xrcp support
*/
datamail()
{

    register FILE  *mboxfid;
    int     keep;
    struct net_stuff    netdata;

    keep = 0;
    if (arg == 0)
    {
	keep++;
	if (xrsqsw == XRSQ_D)
	{
	    netreply ("501 no recipient specified\r\n");
	    return;
	}
    }
    else
    {
	strcpy (username, arg);	/* move user to username */
	if (findmbox () == 0)	/* valid user */
	{
	    netreply ("553 User \"%s\" Unknown\r\n", arg);
	    exit (-1);
	}
    }
/*
*/
    if ((lastpid = frog ()) == 0)
    {
	dataconnection (U4, &netdata);
 
 /* try and get an open file descriptor for mailbox file */
	if ((mboxfid = getmbox ()) == NULL)
	{
	    exit (-1);
	}
	netreply ("150 Mail transfer started okay\r\n");

 /* get data from net connection and copy to mail file */
	if (rcvdata (&netdata, mboxfid, netreply) < 0)
	{
	    fclose (mboxfid);
	    unlink (mfname);
	    exit (-1);
	}
	fclose (mboxfid);
	if (xrsqsw == XRSQ_T && keep)
	{
	    netreply ("250 Mail stored\r\n");
	    exit (0);
	}
	if (sndmsg (mfname) == -1)/* call sndmsg to deliver */
	    netreply ("451 Mail system trouble, try again later\r\n");
	else
	    netreply ("250 Mail delivered\r\n");

	mail_reset ();		/* delete temporary files */
	exit (0);
    }
    else
	if (!(xrsqsw == XRSQ_T && keep))
	    *mfname = '\0';
 /* parent process does NOT want to know about the temporary file    */
 /* if its not gonna be rcp'd--but don't unlink it--sndmsg does that */
}
/*name:
	xrsq, xrcp, mail_reset

algorithm:
	obvious

function:
	handle the XRSQ & XRCP protocols

history:
	initial coding by dm (bbn) 25 april, 1980 to handle these protocols
*/
xrsq()
{
    mail_reset ();		/* Always reset stored stuff */

    if (arg == 0)
    {
	xrsqsw = XRSQ_D;	/* Back to default */
	netreply ("200 OK, using default scheme (none).\r\n");
    }
    else
	switch (arg[0])
	{
	    case '?': 
		netreply ("215 T Text-first, please.\r\n");
		break;
	    case 't': 
	    case 'T': 
		xrsqsw = XRSQ_T;
		netreply ("200 Win!\r\n");
		break;
	    case '\0': 
	    case ' ': 
		xrsqsw = XRSQ_D;/* Back to default */
		netreply ("200 OK, using default scheme (none).\r\n");
		break;
	    default: 
		xrsqsw = XRSQ_D;
		netreply ("501 Scheme not implemented.\r\n");
		break;
	}
}
/*
*/
xrcp()
{
    char    rcpmbox[MBNAMSIZ];
    if (xrsqsw == XRSQ_D)
    {
	netreply ("503 No scheme specified yet; use XRSQ.\r\n");
	return;
    }
    if (*mfname == '\0')
    {
	netreply ("503 No stored text.\r\n");
	return;
    }
    strcpy (username, arg);
    if (findmbox () == 0)
    {
	netreply ("553 User \"%s\" Unknown\r\n", arg);
	return;
    }
    crname (rcpmbox);		/* create a temporary name for sndmsg to
				   mung */
    if (link (mfname, rcpmbox) < 0)
    {
	netreply
	(
	    "451 can't link \"%s\" to \"%s\"; %s\r\n",
	    mfname, rcpmbox, errmsg (0)
	);
	log_error
	(
	    "(xrcp) can't link \"%s\" to \"%s\"",
	    mfname, rcpmbox
	);
	return;
    }
    if (sndmsg (rcpmbox) == -1)
    {
	netreply ("451 mail trouble, please try later\r\n");
	unlink (rcpmbox);
	return;
    }
    else
	netreply ("250 Mail delivered\r\n");
}
/*
*/
mail_reset()	/* if "xrcp r" ever gets implimented, this will scrub out */
{		/* the array of names, when finished, i guess */

 /* if there is a temporary file left over from previous xrcp's */
    if (*mfname)
	unlink (mfname);	/* remove it-- */

    *mfname = '\0';
}

/*
name:
	findmbox

function:
	determine whether a mailbox exists

algorithm:
	change mailbox name to lower case
	if getuser doesn't find name in password file
		try in alias file

parameters:
	none

returns:
	1 if successful, 0 otherwise

history:
	initial coding 12/15/76 by greep for Rand mail system
	Modified to use lowercase() by Dan Franklin (BBN) May 26, 1980

*/

findmbox()
{
    char   *lowercase ();

    lowercase (username, username);
    if (getuser (0, pwfile) == 0)
	return (getuser (0, afile));
    else
	return (1);
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
	datamail

history:
	initial coding 12/15/76 by greep for Rand mail system
	long hosts, with kludge for short host sndmsg program jsq BBN 3-25-79
	sndmsg kludge removed jsq BBN 4-25-79
	dm (bbn) 4-25-80 take filename to send as an argument
*/
sndmsg (file)
char *file;
{
	int n;
	int status;
	char sndmsgflg[3];

	while ((n=fork()) == -1) sleep (5);
	if (n == 0)
	{
	    if (DOSNDMSG == 0)
	    {
		execlp (SNDMSG, "sndmsg", "-netmsg", username, file, 0);
		log_error
		(
		    "(sndmsg) DOSNDMSG == 0, can't execl \"%s\";",
		    SNDMSG
		);
	    }
	    else
	    {
		if (DOSNDMSG == 1)
		{			 /* during conversion */
		    execlp (NSNDMSG, "nsndmsg", "-netmsg", username, file, 0);
		    log_error
		    (
		        "(sndmsg) DOSNDMSG == 1, can't execl \"%s\";",
			NSNDMSG
		    );
		}

		sndmsgflg[0]='\001';    /* special flag for old sndmsg */
		sndmsgflg[1]='\0';
		execlp (SNDMSG,sndmsgflg,file,username,0);
		log_error
		(
		    "(sndmsg) DOSNDMSG irrelevant, can't execl \"%s\";",
		    SNDMSG
		);
	    }
	    exit (-1);
	}
	wait (&status);
	return (status>>8);
}

/*name:
	do_list, list, nlst

function:
	handle the LIST and NLST commands

algorithm:
	fork
		send sock
		open data connection
		dup net data connection into zero and one
		let the standard unix 'ls' do its thing

parameters:
	possible directory from arg

returns:
	nothing

globals:
	arg

calls:
	fork
	dataconnection
	close (sys)
	dup (sys)
	execl (sys)

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren
	combined NLST & LIST commands via do_list() 5/15/80 dm (BBN)
*/
list()
{
	do_list (~NLSTFLAG);
}
nlst()
{
	do_list (NLSTFLAG);
}


char lstargbf[LSTARGSIZ] = "exec ls ";
/*
*/
do_list (lstflag)
int lstflag;
{
    struct net_stuff    netdata;
    register    pid,
                i;
    int     waitwrd;
    int     pype[2];
    char    pipeline[PIPELEN + 1],	/* room for null at pipeline[512] */
            crlfied[2 * PIPELEN];	/* room for two pipefulls */
    char   *ap,
           *cp;

    if (chkguest (0) == 0)
	return;

    if (pipe (pype) < 0)
    {
	netreply ("452 can't make pipe for output; %s\r\n", errmsg (0));
	log_error ("(do_list) can't make pipe for shell output");
	return;
    }

    /* open the data connection */
    dataconnection (U5, &netdata);

    /* say transfer started ok */
    netreply ("125 List started ok\r\n");
/*
*/
    if ((pid = fork ()) == 0)

    {				/* child */

	/* set up file descriptors for the shell */
	dup2 (pype[1],1);
	if (pype[1] != 1) close(pype[1]);
	close (0);
	close (pype[0]);

	/* Set up shell command line */
	for (cp = lstargbf; *cp; cp++)/* Find null */
	    ;
	if (lstflag != NLSTFLAG)
	    for (
		    ap = "-l ";
		    (*cp++ = *ap++) && cp < &lstargbf[LSTARGSIZ];
		);
	if (arg)
	    for
		(
		    ap = arg;
		    (*cp++ = *ap++) && cp < &lstargbf[LSTARGSIZ];
		);

	 /* start up the standard unix 'ls' command */
	execlp (SHELL, "sh", "-c", lstargbf, 0);
	netreply ("451 Can't find shell program!; %s\r\n", errmsg (0)); 
	log_error ("(list) can't find \"%s\"", SHELL);
	exit (-1);
    }
    else
    {				/* parent */
	close (pype[1]);
	while ((i = read (pype[0], pipeline, PIPELEN)) > 0 )
	{
	    pipeline[i] = '\0';
	    i = crlfy (pipeline, crlfied);
	    dat_write (&netdata, crlfied, i);/* this may be a hack */
	}

	while (pid != wait (&waitwrd));
	/* say transfer completed ok */
	if ((waitwrd >> 8) != -1)
	netreply ("250 List completed\r\n");
    }
    net_pclose (&netdata);
}
/*
*/
dont_list (lstflag)		/* doesn't work, unfortunately */
int lstflag;
{
    struct net_stuff    netdata;
    FILE   *pipfil;
    FILE   *popen ();
    int pid, waitwrd;
    char   *ap,
           *cp;

    if (chkguest (0) == 0)
	return;

    if ((pid = fork()) != 0)
    {
	/* parent, wait for child */
	while (pid != wait (&waitwrd));
	if ((waitwrd >> 8) != -1)
	netreply ("250 List completed\r\n");
	return;
    }

 /* Set up shell command line */
    for (cp = lstargbf; *cp; cp++)/* Find null */
	;
    if (lstflag != NLSTFLAG)
	for (
		ap = "-l ";
		(*cp++ = *ap++) && cp < &lstargbf[LSTARGSIZ];
	    );
    if (arg)
	for
	    (
		ap = arg;
		(*cp++ = *ap++) && cp < &lstargbf[LSTARGSIZ];
	    );

 /* start up the standard unix 'ls' command */
    if ((pipfil = popen (lstargbf, "r")) == NULL)
    {
	netreply ("451 Can't start list program!; %s\r\n", errmsg (0));
	log_error ("(list) popen failed \"%s\"", lstargbf);
	exit(0);
    }

 /* open the data connection */
    dataconnection (U5, &netdata);

 /* say transfer started ok */
    netreply ("125 List started ok\r\n");

 /* do the transfer */
    senddata (pipfil, &netdata, netreply);

 /* finish up */

    pclose (pipfil);
    net_pclose (&netdata);
    exit (-1);
}
/*name:
	crlfy()

function:
	to turn newlines into return-newlines, so a shell executing
	an ls command will obey the FTP protocol

algorith:
	copy the buffer into a static area
	replacing newlines with crlf-newlines as you do

parameters:
	from - the buffer with text to be crlfied
	to - where to put the crlfied text

returns:
	count of how many characters in the crlfied text

globals:
	none

calls:
	nothing

called by:
	do_list()

history:
	initial coding by dm (BBN) 29 November, 1980
*/

crlfy (from, to)
char *from, *to;
{
    register char  *fp = from,
                   *tp = to;

    do
    {
	if (*fp == '\n' && *tp != '\r')
	    *tp++ = '\r';
    } while (*tp++ = *fp++);

    return (strlen (to));
}

/*name:
	ftpstat

function:
	handle the STAT command -- for now just say if anything happening
	later can add other stuff (current mode, type, &c)

algorithm:
	send a signal to lastpid
	if it worked
		say something is happening
	else say it's not

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	kill (sys)

called by:
	main thru command array

history:
	greep

*/
ftpstat()
{

    if ((lastpid == 0) || (kill (lastpid, SIG_NETINT) == -1))
	netreply ("211 No transfer in progress\r\n");
    else
	netreply ("211 Transfer in progress\r\n");
}


/*name:
	modecomm

function:
	handle the MODE command

algorithm:
	set the mode variable according to the param
	if its not stream or text then
		say unknown mode
	else
		say mode ok

parameters:
	global param in arg

returns:
	nothing

globals:
	arg
	mode=

calls:
	netreply

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/
modecomm()
{
    char fmode = *arg;			/* assign mode */

    if (isupper(fmode))	/* convert to lower case */
	fmode = tolower(fmode);

    netreply
	(
	  ( (fmode != 's' && fmode != 't' && fmode != 'c')
	      ? "504 Unknown mode (%c)\r\n"
	      :(fmode == 's'
		 ? "200 Stream (%c) mode okay\r\n"
		 : "504 Mode %c not implimented"
	       )
	  ), fmode
	);
}

/*name:
	sturcture

function:
	handle the STRU command

algorithm:
	set stru to param
	if not f or r then
		say unknown mode
	else
		say ok

parameters:
	indirectly thru arg

returns:
	nothing

globals:
	arg
	stru=

calls:
	nothing

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/
structure()
{
    char fstru = *arg;		/* assign struct type */

    /* see if it is ok */

    if (isupper(fstru))		/* convert to lower case */
	fstru = tolower(fstru);

    netreply
	(
	  (fstru != 'f' && fstru != 'r')
	    ? "504 Unimplimented structure type\r\n"
	    : (
		(fstru == 'f')
		? ((stru=STRUF),"200 File Structure okay\r\n")
		: ((stru=STRUR),"200 Record Structure okay\r\n")
	    )
	);
}

/*name:
	type

function:
	handle the TYPE command

algorithm:
	assign the param to the type variable
	if it isnt a or i then
		say error
	else
		say ok

parameters:
	indirectly thru arg

returns:
	nothing

globals:
	arg
	type=

calls:
	nothing

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	modified so that it will take 'i' or 'l' of either case
		10/12/78 S.Y. Chiu

*/
typecomm()
{
    register char xtype = *arg;

    if (isupper(xtype))			/* convert to lower case */
	xtype = tolower(xtype);

    if (xtype == 'l' && (*++arg != '8'))
        netreply ("504 Only Type L8 implemented\r\n");
    else if (*++arg)
	netreply ("501 Syntax error.\r\n");
    else
    {
	netreply
	 ((xtype != 'a' && xtype != 'i' && xtype != 'l')
	    ? "504 Unimplemented Type\r\n"
	    : ((xtype == 'a')
		? "200 Ascii transfers okay\r\n"
		: "200 Image and Local transfers okay\r\n"
	      )
	 );
	type =
	    (xtype == 'a'
		? TYPEA
		: (xtype == 'l' || xtype == 'i'
		    ? TYPEI
		    : type
		  )
	    );
    }
}
/*name:
	delete

function:
	delete a file from disk

algorithm:
	unlink the file
	if error
		say either access or un existant
		return
	say ok

parameters:
	indirectly thru arg

returns:
	nothing

globals:
	arg
	errno (sys)

calls:
	unlink (sys)
	netreply

called by:
	main thru command array

history:
	initial coding <virtual programmer> at Rand Corp
	modified by S. F. Holmgren for Illinois server 4/12/76

*/
delete()
{

    if (chkguest (1) == 0)
	return;

    if (unlink (arg) == -1)
    {
	netreply
	(	(errno == 2
		    ? "550 Can't find \"%s\"\r\n"
		    : "550 Can't unlink \"%s\"\r\n"
		 ), arg
	);
	return;
    }
    netreply ("250 \"%s\" deleted\r\n", arg);
}

/*name:
	cwd - change working directory

function:
	change the current working directory

algorithm:
	use the system chdir entry

parameters:
	new working directory in 'arg'

returns:
	nothing

globals:
	arg

calls:
	chdir (sys)

called by:
	main thru command array

history:
	initial coding 6/30/76 by S. F. Holmgren

*/
cwd()
{
 /* see if we can do the change dir */

    if (chkguest (0) == 0)
	return;

    if (chdir (arg) < 0)
    {
	netreply
	    (
		"%s Can't change to \"%s\"; %s\r\n",
		(errno == ENOENT
		    ? "550"
		    : (errno == EACCES
			? "551"
			: "551"
		    )
		), arg, errmsg (0)
	    );
    }
    else
	netreply ("250 Working directory changed to %s\r\n", arg);
}

/*name:
	renme_from

function:
	Handle the RNFR command

algorithm:
	save the 'source' file name in buf
	set cmdstate to expecting a RFTO command
	say command completed

parameters:
	indirectly thru arg

returns:
	nothing

globals:
	arg
	renmebuf=
	rcvdrnfr=

calls:
	strcpy
	netreply

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
renme_from()
{

    if (chkguest (1) == 0)
	return;

    strcpy (renmebuf, arg);	/* save the file name in buf */
    rcvdrnfr++;			/* say we got a RNFR */
    netreply ("350 RNFR accepted, please send RNTO next\r\n");
}

/*name:
	renme_to

function:
	Handle the RNTO command

algorithm:
	make sure a RNFR command has been received
		signal error
		return
	unlink destination file name
	link source file name to destination
	if error
		if error is access
			signal error and return
		if error is file not found
			signal error and return
		otherwise link failed because of cross device
			copy the file.

parameters:
	indirectly thru arg
	and contents of renamebuf

returns:
	nothing

globals:
	errno (extern to unix sys)
	statb=
	renamebuf
	arg

calls:
	unlink (sys)
	link   (sys)
	stat  (sys)
	creat (sys)
	open  (sys)
	read  (sys)
	write (sys)

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
renme_to()
{
    register FILE  *destfid;
    register FILE  *srcfid;
    register    cnt;
    char    cpybuf[512];

    if (chkguest (1) == 0)
	return;

    if (rcvdrnfr == 0)
    {
	netreply ("503 Haven't received a RNFR\r\n");
	return;
    }
    rcvdrnfr = 0;		/* clear rename to flag */

 /* if cant do stat or file is not standard data file */
    if ((stat (renmebuf, &statb) == -1) || ((statb.st_mode & S_IFBLK) != 0))
    {
	netreply ("550 Can't find \"%s\"; %s\r\n", renmebuf, errmsg (0));
	return;
    }

 /* try and create the new file */
    if ((destfid = fopen (arg,"w")) == NULL)
    {

	/* can't create say error and return */
	netreply
	(
	    "%s Can't create \"%s\"; %s\r\n",
	    (errno == ENOENT
		? "550"
		: (errno == EACCES
		    ? "550"
		    : "551"
		  )
	    ), arg, errmsg (0)
	);
	return;
    }
/*
*/
 /* try and open the source file */
    if ((srcfid = fopen (renmebuf, "r")) == NULL)
    {
	fclose (destfid);
	netreply
	    (
		"%s Can't open \"%s\"; %s\r\n",
		(errno == ENOENT
		    ? "550"
		    : (errno == EACCES
			? "550"
			: "551"
		    )
		),
		renmebuf, errmsg (0)
	    );
	return;
    }

 /* while there is data in source copy to dest */
    while ((cnt = fread (cpybuf, 1, sizeof cpybuf, srcfid)) > 0)
	if (fwrite (cpybuf, 1, cnt, destfid) < cnt)
	{
	    netreply
		(
		    "%s Warning: Unix write error (%s)- aborting\r\n",
		    (errno == ENOENT
			? "550"
			: (errno == EACCES
			    ? "550"
			    : (errno == ENOSPC
				? "552"
				: "551"
			    )
			)
		    ),
		    errmsg (0)
		);
	    log_error
	        (
		    "(renme_to) error copying \"%s\" to \"%s\"",
		    renmebuf, arg
		);
	    goto bad;
	}
/* 
 */
 /* remove link to source file */
    unlink (renmebuf);

 /* say all went well */
    netreply ("250 Rename of \"%s\" to \"%s\" completed\r\n", renmebuf, arg);

bad: 

    close (srcfid);		/* close source file */
    fclose (destfid);		/* close dest file */
}
/*name:
	bye

function:
	handle the BYE command

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
bye()
{
    mail_reset ();		/* flush mail temporary files */
    netreply ("221 Toodles, call again\r\n");
    exit (0);
}

/*name:
	abort

function:
	handles the ABORT command

algorithm:
	if no transfer process has been started or if kill of last one fails
		give error message
	else give success message

parameters:
	none

returns:
	nothing

globals:
	lastpid

calls:
	kill (sys)
	netreply

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren
	modified by greep to make tolerable

*/
cmd_abort()
{
    netreply
	( ((lastpid == 0) || (kill (lastpid, 9) == -1))
	    ? "225 Nothing to abort\r\n"
	    : "226 Operation aborted\r\n"
	);
}

/*name:
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
	called by main thru command array in response to
		ALLO
		REST
		SOCK
		ACCT
	commands

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
accept()
{
    netreply ("200 OK\r\n");
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
	cmdstate

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
    register    i;
    register struct comarr *p;

    netreply ("214-The following commands are accepted:\r\n");
    for (p = commands, i = 1; p <= NOLOGCOMM; p++, i++)
	netreply ("%s%s", p -> cmdname, ((i % 10) ? " " : "\r\n"));
    if (cmdstate != EXPECTCMD)
    {				/* is he logged in? */
	netreply ("\r\nThe following require that you be logged in:\r\n");
	i = 1;			/* for the for statement, following... */
    }
    for (; p -> cmdname; p++, i++)
	netreply ("%s%s", p -> cmdname, ((i % 10) ? " " : "\r\n"));

    netreply ("\r\n214 Please send complaints/bugs to %s\r\n", BUGS);
}
/* write error messages out to the log file */
/* VARARGS */
log_error (printargs)
char *printargs;
{
	fprintf (ERRFD, "%r", &printargs);
	fprintf (ERRFD, "; %s\n", errmsg (0));
}

/* clean up mail stuff (i.e., delete any temporary files) and call die */
go_die (arg)
char *arg;
{
    mail_reset ();
    die (-1, "%s %s %s %s", arg, progname, them, errmsg (0));
}



/*name:
	chkguest

function:
	check whether operation is valid for guest user

algorithm:
	for the time being: if optype is 1, no go
	   if optype is 0, ok if path name does not begin with / or ..

parameters:
	optype - 0 if retr or [x]cwd, 1 if stor, appe, dele, rnfr, rnto

returns:
	1 if ok, else 0

globals:
	filename in arg

calls:
	write (sys)

called by:
	retr, cwd, stor, appe, dele, rnfr, rnto routines

history:
	initial coding by greep

*/
chkguest (optype)
int optype;
{
    if (guest && (optype != 0 || arg[0] == '/' || chkacc ()))
    {
	netreply ("530 Access not allowed for guest users\r\n");
	return (0);
    }
    return (1);
}


chkacc()
{
    register char  *p;
    register int    sep;	/* Indicates new component */

    sep = 1;			/* Initially indicate it is */
    for (p = arg; *p; p++)
    {
	if (sep && p[0] == '.' && p[1] == '.')
	    return (1);
	sep = (p[0] == '/');
    }
    return (0);
}

/*name:
	colon

function:
	skip to next colon or new line

algorithm:
	scan for colon or linefeed

parameters:
	p - pointer to a character string (usually a password entry)

returns:
	pointer to first character after a delimiter

globals:
	none

calls:
	nothing

called by:
	getuser
	loguser

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

char *
colon (p)
char *p;
{
	extern char * sfind();
	return sfind (p,":\n") + 1;
}

frog()
{
    register    i;
    extern int  par_uid;

    i = spawn ();
    if (i)
	dieinit (i, ERRFD);
    else
	dieinit (par_uid, ERRFD);
    return (i);
}

