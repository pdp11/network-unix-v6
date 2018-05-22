#
#include "mapping.h"


/*


/* integer decs */

int nchars 0;			/* number of characters read from tty */
int savetty [3];
int temptty [3];

/* char decs */

char netbuf[128];		/* buffer to fiddle with characters */
char *commarr[]
{
	"end",			/* end this program */
	"bye",
	"help",			/* prints help file */
	"define",			/* enter character mapping */
	"save",			/* save the current char set */
	"load",			/* load a character set */
	"prt",			/* print character set */
	  0
};

/* associated parameter array for the above commands */
char *nextparam 0;
char *netbufp;
char *params[20];
char char_set_name[40];		/* holds current character set name */
char bitmask[]			/* returns two to the power of the index */
{
	01,02,04,010,020,040,0100,0200
};

/* structure decs */


/**/
main(argc,argv)
char *argv[];
{

	register char *charp;
	register char *endp;
	register int  i;
	extern bye();

	/* synthesize home directory and possible char set file */
	endp = colon( charp = gethomedir( netbuf ) );
	*--endp = 0;		/* make a null termed string */
	/* now that we got the home dir bring on the troops */
	strmove( "/character_set",strmove( charp,char_set_name )+1 );
	/* load it if it is there */
	load_char_set();

	/* main loop */
	write (1, "* ", 2);			/* let him know we are here */
	gtty (0,savetty);
	gtty (0,temptty);
	temptty [2] =| RAW;
	stty (0,temptty);
	while (1) {
		netbufp = netbuf;
		nchars = 0;
		if (!buffer_full_line()) continue;
		command_processor();
		write (1, "* ",2);
		}
}

/*
	C O M M A N D _ P R O C E S S O R	--  init nexparam for getoken
						    call indicated command thru
						    decode_command

*/

command_processor()
{

	register index;
	extern (*decode_command[])();

	if( *(nextparam=netbuf) == flagchar) ++nextparam;
	if ((index = getcomm (getoken ())) >= 0)
	(*decode_command [index]) ();

}

/*

	G E T C O M M		-- Scans command array for match between
				   command user typed and available commands.
				   Returns index into commarr if match found
				   otherwise 0
*/

getcomm(strpp)
char *strpp;
{

	register char *comp;	/* pointer to commarr */
	register char *strp;	/* holds strpp for speed */
	register index;		/* keeping track of index into commarr */
	int	length;		/* length of token */
	int	candidate;	/* candidate index */

	/* init some things */
	strp = strpp;
	index = 0;
	candidate = 0;
	length = 0;

	if (strp == 0) return (-1);
	while (*strp++) ++length;		/* length of token */
	strp = strpp;				/* reset it */

	while( comp = commarr[ index++ ] )
		if( compar( comp,strp,length ) == 0 )
			if (candidate) {
				printf ("Command string not unique.\r\n");
				return (-1);
				}
			else candidate = index;
	return (candidate);
}

/*
	G E T O K E N	--  looks at the command string in netbuf, and starting
			    from nextparam( global ) will scan until finds member
			    of terminators, once found, makes the string null 
			    terminated, and returns the beginning of the string
			    if the first char found was a terminator returns 0
*/

char *getoken()
{

	register char *inp;
	register char *startp;

	startp = inp = nextparam;	/* point to the beginning of the string */

	while( *inp == ' ' ) ++inp;	/* scan off leading blanks */
	startp = inp;			/* point at beginning of token */
	while( !setmember( *inp," \r\n") ) ++inp;	/* while not in set bump inp */

	/* if we found something update things and return starting pointer */
	if( inp != startp )
	{
		*inp++ = NULL;		/* make a null string */
		nextparam = inp;	/* update for next time */
		return( startp );	/* return beginning of string */
	}
	else
		return( 0 );		/* nothing found return zero */
}

/*
	E X E C U T E 		--  user typed a command so try to start
				    up a unix file by that name. First look
				    in users directory then in /bin then in
				    /usr/bin.  If not there at all
				    just exit.  The parent waits for the off-
				    spring to exit.

*/

execute()
{

	int status;			/* status of child process */
	register int startid;		/* id of forked process */

	printf("%c\r\n",flagchar);

	startid = fork();

	if( startid < 0 )
	{
		printf("%c\n",flagchar);
		return;
	}

	if( startid == 0 )
	{
		/* here for offspring */
		/* if first char is flag char ignore it */
		if( *(netbufp = netbuf) == flagchar )
			netbufp++;

		/* unparse first command so shell can use glob */
		*--nextparam = ' ';
		*++nextparam = 0;
		execl("/bin/sh","sh","-c",netbufp,0);
		exit(0);
	}
	else	/* here for parent */
	{
		while (wait(&status) != startid);
		printf("%c\n",flagchar);
	}
}


/*
	B Y E	--   Called to exit the program 
		     resets the ttybits 
		     call exit
*/

bye()
{

	stty (0,savetty);
	exit();
}

/*

	L O A D _ C H A R _ S E T		load the current character
						set
*/
load_char_set()
{
	int fid;

	if( (fid=open( char_set_name,0 )) >= 0 )
	{
		read( fid,maptab,SVESIZE );
		printf("%s\r\n",char_set_name);
	}
}

/*



	S A V E _ C H A R _ S E T	save the current character set 
*/
save_char_set()
{

	int fid;

	if( (fid=creat( char_set_name,0600 )) >= 0 )
	{
		/* read everything in the character pool see tty section */
		write( fid,maptab,SVESIZE );
		printf("%s\r\n",char_set_name);
	}
}
/* 

	P R T _ C H A R _ S E T			print any mapped characters
*/
prt_char_set()
{
	register char *cp;
	register char *str;
	register char *c;

	printf("Mapped characters are:\r\n");
	cp = maptab;
	while( cp < &maptab[ 128 ] )
	{
		if( *cp < 0 )		/* this mapped */
		{
			str = &stringtab[ definetab[ (*cp&0177) ]];
			c = cp - &maptab[0];
			if( c < ' ' )		/* this control char */
				printf("^%c      ",c | 0100);	/* make it ^<char>*/
			else
				printf(" %c      ",c);	/* normal */
			write( 1,str,strlen( str ));
			printf("\r\n");
		}
		++cp;
	}
	printf("All Others are un-mapped\r\n");
}
/*
	G E T C		--   returns a character from the buffered structure
			     'input'.  
				if doing define expansion(single level for now)
					return define expansion char 
				if no chars buffered then buffer some
					if read error return -1
				decrement num of chars in queue
				return char incrementing pointer
*/

getc()
{
	register int no_slash;
	char c;

	no_slash = 1;
	/* count zero? */
	loop: while (input.cnt == 0)
	{
		if( (input.cnt=read(0,input.nextc = &input.data[0],80)) < 0 )
			return( -1 );		/* done return error */
	}
	--input.cnt;			/* say one less around */
	c = *input.nextc++;
	if (no_slash && c == '\\') {
		no_slash = 0;
		goto loop;
		}

	++nchars;
	*netbufp++ = c;
	return (c);
}

/* 






	B U F F E R _ F U L L _ L I N E		--  keep getting characters
						    until endchar found
*/
buffer_full_line()
{

	register char c;
	while ((c = getc()) != endline)
	{
		if( c == backsp )
		{
				netbufp =- 2;
				nchars =- 2;
		}
		if( c == linedel  | nchars <= 0 )
		{
			netbufp = netbuf;
			nchars = 0;
			if (c == linedel) printf ("\r\n");
			return (0);
		}
	}
	return (nchars);
}

/*

	E N T E R _ D E F I N E _ T A B	--  change the mapping for the char
					    requested to the string requested
*/

enter_define_tab()
{

	register length;	/* length of str */
	register char c;	/* character to be mapped */
	register char *str;	/* pointer to mapped string */
	char echochar;		/* to decide if char is to be echoed */

	/* get the character to be mapped ( dont run it thru transaltion ) */
	printf("Character to be mapped: ");
	if( (c = getc()) < ' ' )
		printf("^%c\r\n",0100|c);
	else
		printf("%c\r\n",c);
	/* should echo bit be set ?? */
	printf ("Should the string be echoed? ");
	echochar = getc();

	/* build the translation string */
	printf("Enter String (end with cntrl c)\r\n");
	restart:
		str = netbuf;		/* use global netbuf to hold string */
		while( (*str = getc()) != 003 )
		{
			if( *str == 010 )	/* backspace */
				str =- 2;
			else
			if( *str == 0100 ) 	/* line delete @ */
				goto restart;
			++str;			/* bump to next char pos */
		}
	printf("\r\n");
	*str++ = 0;				/* make a null termed string */
	str = netbuf;				/* reinit str pointer */

	/* single character case */
	if( (length = strlen( str )) == 2 )	/* strlen includes the null */
	{
		maptab[c] = (*str & 0177);	/* then just store in maptab */
		return;
	}

	/* see if we have room to store the string and define pointer */
	if( nxtdefentry < NDEFINESIZE && curstrp+length <  NSTRINGS  )
	{
		/* yes have room make define entry */
		definetab[ nxtdefentry ] = curstrp;	/* store pointer to first char */
		str = strmove( str,&stringtab[curstrp] ) + 1;	/* move and update current free location */
		*str++ = 0;				/* make null str */
		curstrp = str - stringtab;		/* update to next free index */
		maptab[ c ] = 0200 | nxtdefentry;	/* make maptab knowledgeable */
		++nxtdefentry;				/* update next free entry */
		if (echochar = 'y') set_bit (c,echomask);
	}
	else
		printf(" Out of Define Space \r\n");
}


/*
	C O M P A R		Compares strings s1 to s2 for either nchars or
				either string contains a null.  If s1 != s2
				returns pointer to char in s1 at which point
				compare failed.  If s1 == s2 returns 0

*/


compar( s1,s2,n )
char *s1;
char *s2;
int n;
{

	register char c1,c2;

	while( (c1 = *s1++) == (c2 = *s2++) && (n-- > 0) )
		if( n == 0 || c1 == 0 || c2 == 0 )
			return( 0 );
	return( --s1 );
}
/*

	S T R M O V E		Copies str1 to str2 until str1 contains a null

*/

char *strmove( str1,str2 )
char *str1;
char *str2;
{

	register char *src;
	register char *dest;

	src = str1;
	dest = str2;

	while( *dest++ = *src++ );

	return( dest-2 );
}

/*





	S T R L E N	--   return the number of characters in the null
			     terminated string passed.  The count returned
			     includes the null character at the end of the
			     string
*/

strlen( str )
char *str;
{

	register char *strp;	/* for speed */
	register  int cnt;	/* for the number of chars */

	strp = str;
	cnt = 0;

	while( *strp++ ) ++cnt;
	return( ++cnt );
}
/*
	P R T F I L E		Prints the file whose name is passed

*/
prtfile()
{
	register char *charp;
	register int  cnt;
	char buf[80];		/* somewhere to put the data */
	int fid;		/* file id of opened file */

	printf("\r\n\r\n");	/* space out a bit */

	if( (fid = open("/etc/mapper_help",0 )) >= 0 )
	{
		while(cnt=read(fid,buf,80))
			write (1,buf,cnt);
		close(fid);
	}
	else
		printf ("Can not open help file.\n");

	printf("\r\n\r\n");
}

/*
	S E T M E M B E R	--  looks in the set passed for the member
				    if found will return the member else
				    zero

*/

char setmember( member,set )
char member;
char *set;
{

	register char mem;	/* holds member for speed */
	register char *setp;	/* holds set for speed */

	setp = set;
	mem = member;

	/* while there are members of set - null cant be a member */
	while( *setp )
		/* member this one? */
		if( mem == *setp++ )
			return( mem );

	/* no matches return 0 */
	return( 0 );
}

/*

	B I T _ O N			takes care of checkin whether a bit is on
					bit bitnum in map bitstr
*/

bit_on( bitnum,bitstr )
int bitnum;
char *bitstr;
{
	return( bitmask[ bitnum&07 ] & bitstr[ bitnum>>3 ] );
}

/*


	S E T _ B I T			sets bit bitnum in map bitstr

*/

set_bit( bitnum,bitstr )
int bitnum;
char *bitstr;
{

	bitstr[ bitnum>>3 ] =| bitmask[ bitnum&07 ];
}



/*


	R E S E T _ B I T		reset bit bitnum in map bitstr

*/

reset_bit( bitnum,bitstr )
int  bitnum;
char *bitstr;
{
	bitstr[ bitnum>>3 ] =& ~bitmask[ bitnum&07 ];
}

/*

	C O L O N			scans from ptr passed to a colon
					returns next char passed colon

*/
char *colon( buf )
char *buf;
{

	register char *bufp;

	bufp = buf;
	while( *bufp++ != ':' );
	return( bufp );
}
/*

	G E T H O M E D I R		based on the current UID, searches the
					password file for a match, returns a
					pointer into the buf passed which
					is the start of the home directory
					for the user.  Used to build a path-
					name for the character set file
*/

char *gethomedir( buf )
char *buf;
{

	extern fin;
	register char *bufp;
	register int  uid;
	register char c;
	int     curuid;

	/* open the password file */
	if( (fin = open( "/etc/passwd",0 )) < 0 )
		return( 0 );

	curuid = (getuid())&0377;	/* get hard user id */
	while( 1 )
	{
		bufp = buf;		/* point to beginning of area */
		/* get a full line */
		while( (c=getchar()) != '\n' )
		{
			/* get EOF ? */
			if( c <= 0 )
				return( 0 );	/* return failure */
			*bufp++ = c;
		}
		/* space over to uid part of password entry */
		bufp = colon( colon( buf ) );
		/* turn ascii uid into binary */
		uid = 0;
		while( *bufp != ':' )
			uid = uid*10 + *bufp++ - '0';
		/* match current uid ? */
		if( uid == curuid )
			/* a winnah */
			return( colon( colon( ++bufp )));
	}
}
/*
* command array for procedure addresses to be called in response to the
   above command strings
*/
int (*decode_command[]) ()
{
	&execute,			/* the default - exec unix command */
	&bye,				/* exit this program restoring tty */
	&bye,
	&prtfile,			/* print the help file */
	&enter_define_tab,		/* make a define mapping */
	&save_char_set,			/* save the current char set */
	&load_char_set,			/* load the character set */
	&prt_char_set,			/* print character set */
	     0
};

