#/*
Module Name:
	srvrftp.c

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc srvrftp.c
	exit
: newer
	if ! { newer srvrftp.c /usr/net/etc/srvrftp } exit
	echo ncpp/ftp-s/srvrftp.c:
: finale
	cc -O -s srvrftp.c
	if ! -r a.out exit
	if ! -r /usr/sys/ncpp/ftp-s/srvrftp.c goto same
	if { cmp -s srvrftp.c /usr/sys/ncpp/ftp-s/srvrftp.c } goto same
	su cp srvrftp.c /usr/sys/ncpp/ftp-s/srvrftp.c
	su chmod 444 /usr/sys/ncpp/ftp-s/srvrftp.c
	rm -f srvrftp.c
: same
	su rm -f /usr/net/etc/srvrftp
	su cp a.out /usr/net/etc/srvrftp
	rm -f a.out
	su chmod 544 /usr/net/etc/srvrftp
	su chown root /usr/net/etc/srvrftp
	su chgrp system /usr/net/etc/srvrftp

Function:
	Perform actions as directed by remote FTP user process.

Globals contained:
	none.

Routines contained:
	main

Modules referenced:
	none.

Modules referencing:
	none.

Compile time parameters and effects:

Module History:
	originally written by someone at University of Illinois.
	Later modified by greep at Rand-Unix.
*/
#include "net_open.h"
/*** #include "net_hostname.h" ***/
#define tel_iac 255
#define SIGINR 15

struct openparams openparams;

struct comarr		/* format of the command table */
{
	char *cmdname;		/* ascii name */
	int (*cmdfunc)();       /* command procedure to call */
};

/* command array */
struct comarr commands[]
{
	"user", &user,
	"pass", &pass,
	"acct", &accept,
	"mail", &mail,
	"mlfl", &datamail,
	"sock", &accept,	/* sock command */
	"abor", &abort,
	"bye",  &bye,
	"noop", &accept,	/* no op */
	"stat", &ftpstat,	/* STAT command */
	"help", &help,

/*	This is the fence between log and non-log required commands */
/*      Index 10 is used in the commands array be sure to change     */
/*	NOLOGCOM if commands are added above here		    */

	"retr", &retrieve,
	"stor", &store,
	"appe", &append,
	"dele", &delete,
	"rnfr", &renme_from,
	"rnto",	&renme_to,
	"list", &list,
	"nlst", &nlst,
	"allo", &accept,	/* allocate */
	"rest", &accept,	/* restart */
	"type", &typecomm,
	"mode", &modecomm,
	"stru", &structure,
	"byte", &byte,
	"xcwd", &cwd,           /* change working directory */
	"cwd",  &cwd,		/* change working directory */
	0,      0
};

/* communications buffers/counts/pointers */

char *netptr;		/* next character to come out of netbuf */
int  netcount;		/* number of valid characters in netbuf */
char netbuf[512];	/* the place that has the valid characters */
int  lastpid;           /* pid of last process */

extern int errno;	/* Has Unix error codes */

#define BUFL 80         /* length of buf */
char buf[BUFL];         /* general usage */
char pwbuf[80];		/* password entry for current user */
char username[20];	/* holds <user>:<password> for logging purposes */
char renmebuf[40];	/* holds RNFR argument waiting for RNTO comm */
int  rcvdrnfr;		/* when on says we received RNFR com ok to do RNTO */
char mfname[20];        /* storage for mail file name */

char *pwfile "/etc/passwd";
char *afile  "/usr/net/aliases";
char hex[] "0123456789ABCDEF";  /* Hexadecimal */

/* structure for special network opens */

/*	Stat struct for finding out about file */
struct statb
{
	char	s_minor;	/* minor device */
	char	s_major;	/* major device */
	int 	s_number;	/* i number */
	int	s_mode;		/* mode */
	char	s_links;	/* number of links */
	char	s_uid;		/* user id of owner */
	char	s_gid;		/* group id of owner */
	char	s_size0;	/* high 8 bits of length of file */
	int	s_size1;	/* low 16 bits of len of file */
	int	s_addr[8];	/* block in file */
	int	s_actime[2];	/* time last accessed */
	int	s_modtime[2];	/* time last modified */
} statb;

/* character defines */

#define CEOR	0300	/* end of record for mode = text */
#define CEOF	0301	/* end of file for mode = text */

#define CR	015	/* carriage return */
#define LF	012	/* line feed */
#define NULL	000	/* null */

/* the RESPONSE array */
char *response[]
{
	"200 Last command flushed\r\n",
	"250 File transfer started correctly\r\n",
	"252 File transfer completed ok\r\n",
	"450 I can't find this file\r\n",
	"451 This file is off limits to you\r\n",
	"452 File transfer incomplete: data connection closed\r\n",
	"453 File transfer incomplete: disk is full!\r\n",
	"455 File transfer incomplete: file system error\r\n",
	"455 Mail trouble, please try again later\r\n",
	"504 Action not possible just now\r\n",
	"506 Parameter unknown\r\n"
};

/* defines that map into indexs into the response array */

#define NUM200   0
#define NUM250   1
#define NUM252   2
#define NUM450   3
#define NUM451   4
#define NUM452   5
#define NUM453   6
#define NUM455   7
#define NUM455m  8
#define NUM504   9
#define NUM506  10

/* The TYPE variable	-	modified by type command */

char type;		/* types of transfer */

#define TYPEASCII	'a'	/* type A (default) */
#define TYPEIMAGE	'i'	/* type I */
#define TYPELOCAL	'l'	/* type L */
#define TYPEPRINTER	'p'	/* type p (ascii printer) */
#define TYPEEPRINTER	'e'	/* type e (ebcdic printer) */

/* The MODE variable	-	modified by the mode command */

char mode;		/* modes of transfer */

#define MODESTREAM	's'	/* stream mode (default) */
#define MODETEXT	't'	/* text mode (uses CEOF and CEOR) */
#define MODEBLOCK	'b'	/* block mode headers etc (not implemented) */
#define MODEHASP	'h'	/* hasp mode standard hasp( "        "    ) */

/* The STRUCT variable	-	modified by the stru command */

char fstru;		/* structure of transfer */

#define STRUCTFILE	'f'	/* stru f (default) */
#define STRUCTRECORD	'r'	/* stru r (used with mode=TEXT) */

/* the NETSTATE variable	defines generally what commands are expected */

int netstate;		/* state of the nation */

#define EXPECTUSER	0	/* waiting for user command */
#define EXPECTPASS	1	/* got a user now need password */
#define EXPECTCMD	2	/* logged in now transfer commands */

#define MAXLOGTRIES     3       /* number of login tries */
int     logtries;		/* current number of login tries */

/*	Command fence between log and non-log required commands */

struct comarr *NOLOGCOMM;
#define U4	4	/* offset from base telnet socket */
#define U5	5	/* offset from base telnet socket */

/* Current User and Group ids */
int curuid;		/* set by the loguser procedure */
int curgid;		/* in resonse to a user pass sequence */

int just_con_data;	/* used by getline procedure to limit function */
char *arg;		/* zero if no argument - pts to comm param */
/*name:
	main

function:
	ARPA network server telnet program 

	Takes commands from the assumed network connection (file desc 0)
	under the assumption that they follow the ARPA network file
	transfer protocol NIC doc #10596 RFC 354 and appropriate modifications.
	Command responses are returned via file desc 1.

	

algorithm:
	process structure:

		There is a small daemon waiting for connections to be
		satisfied on socket 3 from any host.  As connections
		are completed by the ncpdaemon, the returned file descriptor
		is setup as the standard input( file desc 0 ) and standard
		output( file desc 1 ) and this program is executed to 
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
	strmatch
	netreply
	( any of the procedure in the commands array )

called by:
	small server daemon program 

history:
	initial coding 4/12/76 by S. F. Holmgren

notes and memorabilia:

	This is an initial release and has a few shortcomings
	
		1.  The reply messages should be fancied up
		    a little.

		2.  Several commands are not implemented I
		    cant find documentation on what they are
		    supposed to do known ones are: NLST, QUO
		    NOQUO.

		3.  A HELP command needs to be written.

		4.  There is a companion daemon that starts this
		    up (srvrdaemon.c).  We stick the thing in
		    /etc/srvrdaemon and start it in the rc file.

		5.  The MODE T is untested. Cant get tenex to use it.
		    Its extremely well desk checked, has a good shot.

		6.  The set user id bit must be set on srvrftp it needs
		    to call setgid and setuid.

		7.  Some of the accounting functions need to be put in.
		    The stuff that the shell does with utmp and wtmp
		    needs to be copied and put in 'loguser'.  There is
		    a comment that says where.

		8.  This came up much to quickly, (2 days from compile
		    to bouncing baby) there must be other things I have
		    forgotten.

	Ive been power coding much to much lately.  If anyone has the
	urge to dig in and help with some of these please feel free.


*/

main()
{
	register struct comarr *comp;

	signal(SIGINR,1);       /* ignore INS interrupts */
	/* get connection info here */
	fstat( 0,&openparam );  /* get frn host, read local and frn sockets */

	/* set defaults for transfer parameters */
	type = TYPEASCII;	/* ascii */
	mode = MODESTREAM;	/* stream */
	fstru = STRUCTFILE;	/* file structured */
	/* init NOLOGCOM */
	NOLOGCOM = &commands[10];
	/* say were listening */
	netreply( "300 NOSC-SDL Unix Server FTP -- Hello!\r\n" );

	nextcomm:
	while( getline() )
	{
		/* find and call command procedure */
		comp = commands;
		while( comp->cmdname )		/* while there are names */
		{
		    if( strmatch( buf,comp->cmdname ) )     /* a winner */
		    {
			if( comp <= NOLOGCOM || netstate == EXPECTCMD )
				(*comp->cmdfunc)();     /* call comm proc */
			else
			{       if( netstate == EXPECTPASS )
				    netreply("504 Give me your password chucko\r\n");
				else
				    netreply("504 Log in with USER and PASS\r\n" );
			}
			goto nextcomm;                          /* back for more */
		    }
		    comp++;         /* bump to next candidate */
		}
		netreply( "500 I never heard of that command before\r\n" );
	}
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
	0 when an error occurs on telnet connection
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

*/

getline()
{

	register char *inp;     /* input pointer in netbuf */
	register char *outp;    /* output pointer in buf */
	register int c;         /* temporary char */
	int cmdflag;            /* looking for telnet command */

	inp = netptr;
	outp = buf;
	arg = 0;
	cmdflag = 0;

	do
	{
		if( --netcount <= 0 )
		{
			if( (netcount = read( 0,netbuf,512 )) <= 0 )
				return( 0 );
			inp = netbuf;
		}
		c = *inp++ & 0377;

		if ( cmdflag )          /* was last char IAC? */
		{       cmdflag = 0;    /* if so ignore this one too */
			c = 0;          /* make sure c != \n so loop won't */
			continue;       /*  end */
		}

		if ( c == tel_iac )     /* is this a telnet IAC? */
		{       cmdflag++;      /* if so note that next char */
			continue;       /*  to be ignored */
		}

		if ( c == '\r' ||       /* ignore CR */
		     c >= 0200 )        /* or any telnet codes that */
			continue;       /*  to sneak through */

		if( just_con_data == 0 && arg == 0 )
		{
			/* if char is a delim afix token */
			if( c == ' ' || c == ',' )
			{
				c = NULL;       /* make null termed */
				arg = outp + 1; /* set arg ptr */
			}
			else
			/* do case mapping (UPPER -> lower) */
			if( c >= 'A' && c <= 'Z' )
				c =+ 'a' - 'A';
		}
		*outp++ = c;
	}
	while( c != '\n' && outp < &buf[BUFL] );

	/* scan off blanks in argument */
	if( arg )
		while( *arg == ' ' ) arg++;

	if( just_con_data == 0 )
		*--outp = 0;            /* null term the last token */
	/* reset netptr for next trip in */
	netptr = inp;
	/* return success */
	return( outp );
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
	fork1
	open (sys)
	numreply
	sendsock
	dataconnection
	sendata

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	fork changed to fork1 by greep

*/

retrieve()
{

	int localdata;		/* file id of local data file */
	int netdata;		/* file id of network data file */

	if( ( lastpid = fork1() ) == 0 )
	{
		/* child comes here */
		if( (localdata = open( arg,0 )) < 0 )	/* open file for read */
		{
			switch( errno )
			{       case 2:                 /* ENOENT */
					numreply( NUM450 );
					break;
				case 13:                /* EACCESS */
					numreply( NUM451 );
					break;
				default:                /* other */
					numreply( NUM455 );
			}
			exit( 1 );
		}

		/* send SOCK command */
		sendsock( U5 );

		/* Open data connection */
		netdata = dataconnection ( U5 );

		/* say transfer started ok */
		numreply( NUM250 );

		/* send data according to params */
		sendata( localdata,netdata );

		/* say transfer completed ok */
		numreply( NUM252 );

		exit( 0 );
	}
}

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
	fork1
	creat (sys)
	numreply
	sendsock
	dataconnection
	rcvdata

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	fork changed to fork1 by greep

*/

store()
{

	int localdata;		/* file id for local data file */
	int netdata;		/* file id for network data file */

	if( ( lastpid = fork1() ) == 0 )
	{

		/* create new file */
		if( (localdata = creat( arg,0644 )) < 0 )
		{
			numreply( NUM451 );	/* access denied */
			exit( 1 );
		}

		/* send socket command */
		sendsock( U4 );

		/* open data connection */
		netdata = dataconnection( U4 );

		/* say transfer started ok */
		numreply( NUM250 );

		/* receive and translate data according to params */
		rcvdata( netdata,localdata );

		/* say completed ok */
		numreply( NUM252 );

		exit( 0 );
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
	fork1
	open (sys)
	creat (sys)
	numreply
	seek (sys)
	sendsock
	dataconnection
	rcvdata

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren
	fork changed to fork1 by greep

*/

append()
{

	int localdata;		/* file id of local data file */
	int netdata;		/* file id of network data file */

	if( ( lastpid = fork1() ) == 0 )
	{

		/* try and open file -- it may exist */
		if( (localdata = open( arg,1 )) < 0 )
		{
			/* doesnt exist try and creat */
			if( (localdata = creat( arg,0644 )) < 0 )
			{
				numreply( NUM451 );	/* denied access */
				exit( 1 );
			}
		}
		else	/* it exists seek to end */
			seek( localdata,0,2 );

		/* send socket command */
		sendsock( U4 );

		/* open data connection */
		netdata = dataconnection( U4 );

		/* say transfer started ok */
		numreply( NUM250 );

		/* rcv and translate according to params */
		rcvdata( netdata,localdata );

		/* say transfer completed ok */
		numreply( NUM252 );

		exit( 0 );

	}
}

/*name:
	dataconnection

function:
	connect with either U4,U5 or U+5,U+4 from the base
	currently available thru file descriptor 0.

algorithm:
	set up appropriate open parameters
	try to open the file
		signal error
		exit
	return opened file

parameters:
	offset - offset from local socket

returns:
	file descriptor of opened data connection

globals:
	openparam=

calls:
	open (sys)
	netreply

called by:
	retrieve
	store
	append

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

dataconnection( offset )
{

	register netfid;	/* id of network data connection */

	openparam.o_lskt = offset;      /* set local from telnet base */
	openparam.o_fskt[0] = 0;        /* clear any high bits */
	openparam.o_fskt[1] = (offset+1)&05;   /* make (u+4,u+5) or (u+5,u+4) */
	openparam.o_relid = 1; /* specify local telnet base */
	/* specify a relative, direct open that we initiate */
	openparam.o_type = (RELATIVE | DIRECT | INIT );
	openparams.o_nmal = 512;        /* set allocation level */

	/* try and open network data connection */
	if( (netfid = open( "/dev/net/anyhost",&openparam )) < 0 )
	{
		netreply( "454 Can't Establish Data Connection.\r\n" );
		exit( 1 );
	}
	return( netfid );	/* give file id back */
}

/*name:
	sendata

function:
	transfer data from the local disk file system to the network

algorithm:
	while data can be buffered
		thru the current buffer
			if type is ascii
				if mode is text
					translate lf->CEOR
				else
					translate lf->crlf
				translate cr -> cr null

		write the translated contents
			signal error
			exit

	if mode is text
		send eof

parameters:
	infile - file to get data from
	outfile - file to write data to

returns:
	nothing

globals:
	netbuf=
	typecomm
	mode
	fstru

calls:
	read (sys)
	write (sys)
	numreply

called by:
	retrieve

history:
	initial coding 4/12/76 by S. F. Holmgren
	check for image transfer and do a full buf write 8/22/76 S. Holmgren

*/

sendata( infile,outfile )
{

	char tranbuf[1024];		/* buffer to do expansion */
	register char *inp;		/* input data */
	register char *outp;		/* output data */
	register int  incnt;		/* input count */
	char *transtart;		/* start addr of tranbuf */

	transtart = &tranbuf[0];

	while( (incnt = read( infile,netbuf,512 )) > 0 )
	{

		if( type == TYPEIMAGE )		/* if image just do the write */
		{
			write( outfile,netbuf,incnt );
			continue;
		}

		inp = netbuf; outp = tranbuf;	/* attach pointers */
		do
		{
			if( type == TYPEASCII )	/* binary or ascii? */
			{
				/* ascii ( play crlf games ) */
				if( *inp == LF )	/* find a linefeed */
					if( mode == MODETEXT && fstru == STRUCTFILE )
						*inp = CEOR;	/* make end of record */
					else
						*outp++ = CR;	/* make crlf */
				else
				if( *inp == CR )	/* find a cr */
				{
					*outp++ = CR;	/* tran to crnull */
					*inp = NULL;
				}
			}
			*outp++ = *inp++;	/* copy input to output */
		}
		while( --incnt );		/* thru num of characters */

		if( write( outfile,transtart,( outp-transtart )) <= 0 )
		{
			numreply( NUM452 );             /* data error */
			exit( 1 );
		}
	}

	/* if mode is text send an end of file */
	if( mode == MODETEXT )
	{
		tranbuf[0] = CEOF;
		write( outfile,transtart,1 );
	}
}

/*name:
	rcvdata

function:
	receive data from the network and translate contents
	according to global params mode/type/fstru

algorithm:
	while there are buffers to be gotten
		if we need a line feed handle the problem
		thru the data in the buffer
			copy input to output
			if type is ascii
				if char is CR
					if we have another char
						if next char LF
							ignore CR
						if next char NULL
							ignore NULL
						if needed LF
							output needed CR
						reset needlf
					else
						not enough chars say needlf
				else
				mode is TEXT
					char is end of record
						make char line feed
					char is end of file
						flush present buffer
						return success

			increment output pointer

		flush present buffer

	if mode is TEXT
		signal error
		return failure

	return success

parameters:
	infile - file to get data from
	outfile - file to write data to

returns:
	-1 for failure
	0  for success

globals:
	type
	mode
	netbuf=

calls:
	read (sys)
	write (sys)
	netreply

called by:
	store
	append

history:
	initial coding 4/12/76 by S. F. Holmgren
	check for image transfer and do a full buf write 8/22/76 S. Holmgren

*/

rcvdata( infile,outfile )
{

	register char *inp;		/* input data ptr */
	register char *outp;		/* output data ptr */
	register int  incnt;		/* input data count */
	int  crflag;			/* just received cr do something */
	char tranbuf[1024];		/* buffer to stick output in */
	char *transtart;		/* contains &tranbuf[0] */

	crflag = 0;
	transtart = &tranbuf[0];	/* yes it really contains start addr */

	while( (incnt = read( infile,netbuf,512 )) > 0 )
	{

		if( type == TYPEIMAGE )
		{
			write( outfile,netbuf,incnt );	/* just write data */
			continue;
		}

		inp = netbuf;		/* setup input ptr */
		outp = tranbuf;		/* setup output ptr */
		do
		{
			if( type == TYPEASCII )		/* ascii translation */
			{
				if( *inp == CR )	/* found a cr */
				{
					if( crflag++ == 0 )	/* ignore first one */
					{
						inp++;		/* ignore cr */
						continue;
					}
				}
				else
				if( *inp == LF )	/* cr lf seq */
					crflag = 0;	/* yes ignore cr */
				else
				if( *inp == NULL && crflag )	/* cr null seq */
				{
					crflag = 0;	/* yes ignore null */
					*inp = CR;	/* substitute cr */
				}
				else
				if( mode == MODETEXT )	/* ascii & text mode */
				{
					if( *inp == CEOR )	/* end of rec */
						*inp = LF;	/* sub a line feed */
					else
					if( *inp == CEOF )	/* end of file */
					{
						write( outfile,transtart,(outp-transtart));
						return(0);	/* return success */
					}
				}
				else	/* default */
				if( crflag )	/* cr <anything> seq */
				{
					crflag = 0;
					*outp++ = CR;	/* put out a CR */
				}
			}
			*outp++ = *inp++;	/* copy input to output */
		}
		while( --incnt );	/* while there are characters */
		/* write contents of buffer */
		write( outfile,transtart,( outp-transtart ) );
	}

	/* if text mode and we got here say data error */
	if( mode == MODETEXT )
	{
		numreply( NUM452 );		/* data error */
		return( -1 );			/* return error */
	}

	return( 0 );				/* return success */
}

/*name:
	sendsock

function:
	send ftp sock command

algorithm:
	send ascii sock string
	send ascii local socket number
	send cr lf

parameters:
	offset from base socket to send

returns:
	nothing

globals:
	openparam.o_lskt

calls:
	netreply
	locv (sys)

called by:
	store
	retrieve
	append

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

sendsock( offset )
{

	netreply( "255 SOCK " );
	netreply( locv( 0,openparam.o_lskt+offset ) );
	netreply( "\r\n" );
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
		signale error
		reset state
		return

	save user name
	request password

parameters:
	none

returns:
	nothing

globals:
	netstate=
	logtries=
	username=
	arg

calls:
	netreply
	strmove

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

user()
{

	if( netstate != EXPECTUSER )
	{       numreply( NUM504 );
		return;
	}

	if( logtries++ > MAXLOGTRIES )
	{
		netreply( "430 Login attempts exceeded, go away\r\n" );
		exit( 1 );	/* error exit */
	}

	/* set state to expecting a PASS command */
	netstate = EXPECTPASS;

	/* save the username in username buf */
	strmove( arg,username );

	/* ask him for password */
	netreply( "330 Enter PASS Command\r\n" );
}

/*name:
	pass

function:
	handle the PASS command

algorithm:
	do we know this user
		signale error 
		set state to EXPECTUSER
		return
	do appropriate logging
	say he is logged in

parameters:
	none

returns:
	nothing

globals:
	netstate=

calls:
	getuser
	netreply
	loguser

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

pass()
{

	if( netstate != EXPECTPASS )
	{       numreply( NUM504 );
		return;
	}

	/* get the users password entry */
	if( getuser( 1,pwfile ) == 0 )
	{
		/* unknown user */
		netreply( "431 I don't know you, try again\r\n");
		netstate = EXPECTUSER;	/* waiting for user command */
		return;
	}

	/* got user */
	netstate = EXPECTCMD;
	/* do appropriate accounting and system info work */
	loguser( 1 );

	/* say he is logged in */
	netreply( "230 User Logged in\r\n" );
}

/*name:
	getuser

function:
	find a match for the user in username and password in arg
	in /etc/passwd

algorithm:
	open the password file
		signal no wanted users
		exit
	build <username>:<password>\0
	search thru password file for match

parameters:
	user name in username password in arg
	name of file to look in

returns:
	1 if user found
	0 failure

globals:
	username=
	pwbuf=

calls:
	open (sys)
	strmove
	gets
	strmatch
	colon
	close (sys)

called by:
	pass

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

getuser( user_pass,filename )
char *filename;
{

	register char *p;		/* general use */
	register int  retval;		/* return value  = 1 success
					                 = 0 failure
					 */
	struct
	{
		int fildes;
		int nleft;
		char *nextp;
		char buf[512];
	} iobuf;

	retval = 0;			/* default to failure */
	iobuf.nleft = 0;		/* make sure inited */
	iobuf.nextp = 0;		/* make sure inited */

	/* open the password file */
	if ( (iobuf.fildes = open( filename,0 )) < 0 )
	{
		netreply( "401 Users not accepted now, try later\r\n" );
		exit( 1 );		/* fail exit */
	}


	if( user_pass )		/* password required also? */
		strmove( crypt( arg ),strmove( ":",strmove(username,username)));

	/* look thru the entries in the password file for a match */
	while( gets( &iobuf,pwbuf ) )		/* get entry from pwfile in pwbuf */
	{
		p = colon( pwbuf );		/* skip to colon after user name */
		if( user_pass )			/* need pass also */
			p = colon( p );		/* get to colon after passwd */
		*--p = 0;	/* terminate <user>:<passwd>0 */
		if( strmatch( username,pwbuf ))		/* we got a winner */
		{
			retval++;			/* return success */
			break;
		}

	}
	close( iobuf.fildes );			/* close password file */
	return( retval );
}

/*name:
	loguser

function:
	perform installation/system specific login/accounting functions

algorithm:
	build user id

	build group id

	change to working directory

	set user id
	set group id

parameters:
	user information in pwbuf

returns:
	nothing

globals:
	pwbuf=
	curuid=
	curgid=

calls:
	strmove
	setgid (sys)
	setuid (sys)
	colon
	chdir (sys)

called by:
	pass

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

loguser( fullog )
{

	register char *passentp;	/* pointer to password entry */
	register char *p;		/* general usage */
	register int  num;		/* constructing uid and gid */

	passentp = strmove( pwbuf,pwbuf );	/* get to end of <user>:<pw>0 */

	/* if there is nothing in pwbuf forget it */
	if( passentp == &pwbuf[0] ) return;

	if( fullog == 0 )
		passentp = colon( passentp );	/* if not full log need another */
	else
		passentp++;			/* a little adjustment music */

	num = 0;
	/* build user id */
	while( *passentp != ':' )
		num = num*10 + *passentp++ - '0';
	curuid = num;			/* save user id globally */

	num = 0;
	/* build group id */
	while( *++passentp != ':' )
		num = num*10 + *passentp - '0';
	curgid = num;			/* save group id globally */

	/* skip over GCOS mailbox to beginning of working dir */
	passentp = colon( ++passentp );

	/* null terminate working directory */
	p = colon( passentp );
	*--p = NULL;
	chdir( passentp );

	if( fullog )
	{
		/* do accounting here */

		setuid( curuid );		/* set user id */
		setgid( curgid );		/* set group id */
	}
}

/*name:
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
	numreply
	open (sys)
	seek (sys)

called by:
	mail
	datamail

history:
	initial coding 6/30/76 by S. F. Holmgren
	rewritten by greep for Rand mail system

*/
getmbox()
{
	register int mboxfid;

	crname( mfname );       /* create mailbox file name */

	mboxfid = creat(mfname,0644);
	if (mboxfid < 0)
	{       numreply( NUM451 );
		return( -1 );
	}
	return( mboxfid );
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
crname(ptr)
char *ptr;
{
	int i;
	int tvec[4];
	static int filecnt;
	register char *p, *q;

	p = "/tmp/";
	q = ptr;
	while (*q++ = *p++);
	--q;
	p = &tvec[0];
	tvec[2] = getpid();
	tvec[3] = filecnt++;
	if (filecnt==256)
	{       filecnt = 0;
		sleep(1);
	}
	time(tvec);
	for (i=7; i; --i)
	{       *q++ = hex[(*p>>4)&017];
		*q++ = hex[ *p++  &017];
	}
	*q = '\0';
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

*/
mail()
{
	register mboxfid;	/* mailbox file desc */
	register char *p;	/* general use */
	int tyme [2];		/* for the time */

	/* make sure there is a user name */
	if( arg == 0 )
		arg = "mother";         /* default to mother */
	strmove( arg,username );	/* stick user arg in username for getuser */
	if( findmbox() == 0 )
	{
		netreply( "450 User Unknown\r\n" );
		return;
	}

	/* get to open mailbox file descriptor */
	if( (mboxfid = getmbox()) < 0 )
		return;

	/* say its ok to continue */
	netreply( "350 Enter Mail, end by a line with only '.'\r\n" );

/***    time (tyme);
	printf (mboxfid, "\n--- Network Mail from host %s on %20.20s ---\n"
		,hostnames[openparam.o_host & 0377]
		,ctime (tyme)
		);                      ***/

	just_con_data = 1;			/* tell getline only to drop cr */

	while( 1 )				/* forever */
	{
		if( (p = getline()) == 0 )
		{
			write(mboxfid,"\n***Sender closed connection***\n",32 );
			break;
		}
		/* are we done */
		if( buf[0] == '.' && buf[1] == '\n' )
			break;                  /* yep */
		write( mboxfid,buf,(p-buf) );	/* stick data in mailbox */
	}
	just_con_data = 0;	/* set getline to normal operation */
/***    time (tyme);
	printf (mboxfid, "\n--- Network mail ended on %20.20s ---\n"
		,ctime (tyme)
		);              ***/
	close( mboxfid );
	if( sndmsg() == -1 )    /* call sndmsg to deliver mail */
	{       numreply( NUM455m );    /* something wrong! */
		unlink( mfname );       /* delete temporary file */
	}
	else
	/* say mail delivered */
		netreply( "256 Mail Delivered \r\n");
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
	fork1
	strmove
	findmbox
	netreply
	sendsock
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

*/
datamail()
{
	register netdata;
	register mboxfid;
	int tyme [2];

	if( ( lastpid = fork1() ) == 0 )
	{
		if( arg == 0 )	/* no user speced default to root */
			arg = "root";
		else
		{
			strmove( arg,username );	/* move user to username */
			if( findmbox() == 0 )           /* valid user */
			{
				netreply( "451 User Not Recognized\r\n" );
				exit( 1 );
			}
		}

		/* send sock command */
		sendsock( U4 );

		/* open data connection */
		netdata = dataconnection( U4 );

		/* try and get an open file descriptor for mailbox file */
		if( (mboxfid = getmbox()) < 0 )
			exit( 1 );

/***            time (tyme);
		printf (mboxfid, "\n--- Network Mlfl from host %s on %20.20s ---\n"
			,hostnames[openparam.o_host & 0377]
			,ctime (tyme)
			);              ***/
		/* say its ok to proceed */
		numreply( NUM250 );

		/* get data from net connection and copy to mail file */
		rcvdata( netdata,mboxfid );


/***            time (tyme);
		printf (mboxfid, "\n--- Network MLFL transmission ended on %20.20s ---\n"
			,ctime (tyme)
			);      ***/
		/* make the current guy owner of the mail box file */
		close( mboxfid );
		if( sndmsg() == -1 )    /* call sndmsg to deliver mail */
		{       numreply( NUM455m );    /* something wrong! */
			unlink( mfname );       /* delete temporary file */
		}
		else
		/* say it went off ok */
			numreply( NUM252 );

		exit( 0 );
	}
}

/*name:
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
	nothing

globals:
	username

calls:
	getuser

called by:
	mail
	datamail

history:
	initial coding 12/15/76 by greep for Rand mail system

*/
findmbox()
{
	register char *p;

	for (p = username; *p; p++)
	{       if (*p >= 'A' && *p <= 'Z')
			*p =| ' ';      /* convert to lower case if req */
	}
	if ( getuser( 0,pwfile ) == 0)
		return( getuser( 0,afile) );
	else
		return( 1 );
}
/*name:
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
	openparam.o_host

calls:
	fork (sys)
	exec (sys)

called by:
	mail
	datamail

history:
	initial coding 12/15/76 by greep for Rand mail system

*/
sndmsg()
{
	char sndmsgflg[3];
	int n;
	int status;

	sndmsgflg[0]='\001';    /* special flag for sndmsg */
	sndmsgflg[1]=openparam.o_host;
	sndmsgflg[2]='\0';
	while((n=fork()) == -1) sleep(5);
	if (n == 0)
	{       execl("/bin/sndmsg",sndmsgflg,mfname,username,0);
		exit(-1);
	}
	wait(&status);
	return( status>>8 );
}

/*name:
	list

function:
	handle the LIST command 

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
	sendsock
	dataconnection
	close (sys)
	dup (sys)
	execl (sys)

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
list()
{
	register netdata;
	register pid;
	int      waitwrd;

	if( (pid = fork()) == 0 )
	{
		/* send sock command */
		sendsock( U5 );

		/* open the data connection */
		netdata = dataconnection( U5 );

		/* say transfer started ok */
		netreply( "250 List started ok\r\n");

		close( 0 );		/* close zero */
		dup( netdata );		/* move in netdata */
		close( 1 );		/* close one */
		dup( netdata );		/* move in netdata */
		close( netdata );	/* close netdata itself */

		/* start up the standard unix 'ls' command */
		execl( "/bin/dir","dir",arg,0 );
		netreply( "507 Can't find listing program!\r\n" );
		exit( -1 );
	}
	else
	{       while( pid != wait( &waitwrd ) );
		/* say transfer completed ok */
		if ( ( waitwrd>>8 ) != -1 )
			netreply( "252 List completed\r\n");
	}
}

/*name:
	nlst

function:
	handle the NLST command --  give directory listing

algorithm:
	fork
		exec the standard unix ls command

parameters:
	possible file name in arg

returns:
	nothing

globals:
	arg

calls:
	netreply
	fork (sys)
	execl (sys)

called by:
	main thru command array

history:
	greep

*/
nlst()
{

	register pid;
	int waitwrd;

	netreply( "151-Listing starting, duck!\r\n");
	if( (pid = fork()) == 0 )	/* child */
	{
		execl( "/bin/dir","dir",arg,0 );
		netreply( "Can't find listing program!\r\n" );
		exit( 1 );
	}
	while( pid != wait( &waitwrd ) );
	netreply( "151 Listing ended (that's all, folks)\r\n");
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

	if( ( lastpid == 0 ) || ( kill( lastpid,SIGINR ) == -1) )
		netreply( "100 No transfer in progress\r\n" );
	else
		netreply( "100 Transfer in progress\r\n" );
}


/*name:
	byte

function:
	handle the BYTE command

algorithm:
	convert the param to binary
	if it is out of range <= 0 >= 256
		signal error
	if it is not 8
		signial we dont handle this

parameters:
	global param in arg

returns:
	nothing

globals:
	arg

calls:
	netreply
	numreply

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/
byte()
{

	register int num;
	register char *p;

	p = arg;	/* a little speed */
	num = 0;	/* a little initialization */
	while( *p )	/* convert the number to binary */
		num = num * 10 + (*p++ - '0');
	if(∑∫6üô>>∑∫6ûêÖÑ∑2:π28∂<ëòê$6∂≤≥06±<∫2êπ4Ω2π≤∏∫≤9∫22.9.7êîÖÑ2∂π2ÖÑ∑∫6π28∂<∑∫6êêúêß™¶ß™&êîÖ>Ö∑∞∂2ÖÑ∂7≤≤±∑∂6≥:∑1∫¥77Ö¥072∂2:¥2ê¶'¢"ê±∑∂∂072Ö0∂≥7π4:¥6ÖÑπ2::¥2ê∂7≤2ª0π¥01∂2ê∞±±79≤4∑3∫7:¥2∏0π∞6ÖÑ43ê4∫9 not stream or text then 
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
	mode = *arg;		/* assign mode */
	if( mode >= 'A' && mode <= 'Z' )        /* convert to lower case */
		mode =+ 'a' - 'A';
	numreply( (mode != 's' && mode != 't') ? NUM506 : NUM200 );
}

/*name:
	sturcture

function:
	handle the STRU command

algorithm:
	set fstru to param
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
	fstru=

calls:
	nothing

called by:
	main thru command array

history:
	initial coding 4/12/76 by S. F. Holmgren

*/
structure()
{
	fstru = *arg;	/* assign struct type */
	/* see if it is ok */
	if( fstru >= 'A' && fstru <= 'Z' )      /* convert to lower case */
		fstru =+ 'a' - 'A';
	numreply( (fstru != 'f' && fstru != 'r') ? NUM506 : NUM200 );
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

*/
typecomm()
{
	type = (*arg == 'l') ? 'i' : *arg;	/* map type l to type i */
	if( type >= 'A' && type <= 'Z' )        /* convert to lower case */
		type =+ 'a' - 'A';
	numreply( (type != 'a' && type != 'i') ? NUM506 : NUM200 );
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

	if( unlink( arg ) == -1 )
	{
		numreply( (errno == 2) ? NUM450 : NUM451 );
		return;
	}
	netreply( "254 File flushed\r\n" );
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
	numreply

called by:
	main thru command array

history:
	initial coding 6/30/76 by S. F. Holmgren

*/
cwd()
{
	/* see if we can do the change dir */

	if( chdir( arg ) < 0 )
		netreply( "450 Dir unknown or access denied\r\n" );
	else
		netreply( "200 Working directory changed\r\n" );
}

/*name:
	renme_from

function:
	Handle the RNFR command

algorithm:
	save the 'source' file name in buf
	set netstate to expecting a RFTO command
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
	strmove
	netreply

called by:
	main thru command array

history:
	initial coding 4/13/76 by S. F. Holmgren

*/
renme_from()
{

	strmove( arg,renmebuf );	/* save the file name in buf */
	rcvdrnfr++;		/* say we got a RNFR */
	netreply( "200 RNFR accepted, please send RNTO next\r\n" );
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
	numreply
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
	register destfid;
	register srcfid;
	register cnt;
	char cpybuf[512];

	if( rcvdrnfr == 0 )
	{
		numreply( NUM504 );
		return;
	}
	rcvdrnfr = 0;	/* clear rename to flag */

	/* if cant do stat or file is not standard data file */
	if( (stat( renmebuf,&statb ) == -1) || ((statb.s_mode&060000) != 0) )
	{
		numreply( NUM450 );	/* file not found */
		return;
	}

	/* try and create the new file */
	if( (destfid=creat( arg,statb.s_mode&0777 )) == -1 )
	{
		/* cant create say error and return */
		numreply( NUM451 );
		return;
	}

	/* try and open the source file */
	if( (srcfid=open( renmebuf,0 )) == -1 )
	{
		close( destfid );
		numreply( NUM451 );	/* access denied */
		return;
	}

	/* while there is data in source copy to dest */
	while( (cnt=read( srcfid,cpybuf,512 )) > 0 )
		if( write( destfid,cpybuf,cnt ) < cnt )
		{
			netreply( "453 Warning: Unix write error - aborting\r\n" );
			goto bad;
		}

	/* remove link to source file */
	unlink( renmebuf );
	/* say all went well */
	netreply( "253 Rename Completed\r\n" );

bad:	

	close( srcfid );	/* close source file */
	close( destfid );	/* close dest file */
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

	netreply( "231 Toodles, call again\r\n" );
	exit( 0 );
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
abort()
{
	if( ( lastpid == 0 ) || ( kill( lastpid,9 ) == -1) )
		netreply( "202 Nothing to abort\r\n" );
	else
		netreply( "201 Operation aborted\r\n" );
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

calls:
	numreply

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
	numreply( NUM200 );
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
	netstate

calls:
	netreply

called by:
	called by main thru the help command

history:
	greep

*/
help()
{       netreply( "050-The following commands are accepted:\r\n" );
	netreply( "USER PASS ACCT MAIL MLFL SOCK ABOR NOOP HELP BYE\r\n" );
	if( netstate != EXPECTCMD )     /* is he logged in? */
		netreply("The following require that you be logged in:\r\n");
	netreply( "RETR STOR APPE DELE RNFR RNTO LIST NLST STAT ALLO\r\n" );
	netreply( "REST TYPE MODE STRU BYTE XCWD\r\n");
	netreply( "050 Please send complaints/bugs to greep@Rand-Unix\r\n");
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

*/
netreply( str )
char *str;
{
	register char *netstr;		/* string to be sent */
	register char *p;		/* general usage */

	netstr = str;
	/* get length of str */
	for( p=netstr; *p; p++ );

	/* send string */
	write( 1,netstr,(p-netstr) );
}

/*name:
	numreply

function:
	to send ascii strings to net

algorithm:
	take param and index in repsonse array to get char string
	get length of charstring
	send string to net

parameters:
	index into response array 

returns:
	nothing

globals:
	resonse array

calls:
	write (sys)

called by:
	everyone

history:
	initial coding 4/14/76 by S. F. Holmgren

*/
numreply( index )
int index;
{

	register char *netstr;
	register char *p;

	netstr = responses[ index ];	/* get char string */
	for( p=netstr; *p ; p++ );	/* get length of str */
	write( 1,netstr,( p-netstr ) );	/* send to net */
}


/*name:
	fork1

function:
	spawn an orphan and return its pid

algorithm:
	set up a pipe
	fork

	old process:                    new process:
		wait for new                    fork
		  process                       write process number of
		read 2 bytes                      newest process on pipe
		  from pipe

parameters:
	none

returns:
	pid of new process

globals:
	none

calls:
	fork (sys)

called by:
	retrieve
	store
	append
	datamail

history:
	initial coding by greep (spawn stolen from jsz)

*/
fork1()
{       int k, l;
	int pstat;
	int pip[2];
	int pid;

	if (pipe(pip) == -1)    /* this shouldn't happen (I hope) */
		return(-1);
	while ((k = fork()) == -1) sleep(10);
	if (k)
	{       wait(&pstat);           /* Wait for k below to exit */
		read(pip[0],&pid,2);    /* read pid */
		close(pip[0]);          /* close pipe */
		close(pip[1]);
		return (pid);   /* Returns non-zero in parent */
	}
	else
	{	while ((l = fork()) == -1) sleep(10);
		if (l)
		{       write(pip[1],&l,2);     /* write pid of child */
			exit();         /* After creating l, exit. */
		}
		close(pip[0]);          /* close pipe */
		close(pip[1]);
		return(0);
	}
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

colon( p )
char *p;
{

	register char *cp;

	cp = p;

	while( *cp != ':' && *cp != '\n' ) cp++;
	return( ++cp );		/* return next char after delim */
}

/*name:
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

gets( iobuffer,destbuf )
char *destbuf;
{

	register int  c;
	register char *outp;
	register int  iobuf;

	iobuf = iobuffer;
	outp = destbuf;

	while( (c = getc( iobuf )) > 0 )
		if( c == '\n' )
		{
			*outp = NULL;	/* null terminate */
			return( 1 );	/* return success */
		}
		else
			*outp++ = c;	/* just bump to next spot */

	/* return failure */
	return( 0 );
}

/*name:
	strmove

function:
	move data from one address to another

algorithm:
	copy data from one address to another

parameters:
	src - place to get data from
	dest - place to put data

returns:
	pointer to the null at the end of the concationed string

globals:
	none

calls:
	nothing

called by:
	user
	getuser

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

strmove( from,to )
char *from;
char *to;
{

	register char *src;	/* same as from for speed */
	register char *dest;	/* same as to for speed */

	src = from;  dest = to;

	while( *dest++ = *src++ );

	return( --dest );	/* return ptr to null at end of string */
}

/*name:
	strmatch

function:
	to compare two null terminated strings for equality.

algorithm:
	Do until one of them contains a null;
		if unequal return false
	if both are not null, return false
	else
	return true

parameters:
	char ptrs to the strings to be matched

returns:
	0: if strings differ
	1: if string are identical

globals:
	none

calls:
	nothing

called by:

history:
	designed and coded by Mark Kampe Ucla-ats

*/
strmatch( s1,s2 )
char *s1;
char *s2;
{

	register char *p1;
	register char *p2;

	p1 = s1;	p2 = s2;

	while( *p1 && *p2 )
		if( *p1++ != *p2++ )	return( 0 );
	return( *p1 == *p2 );
}

