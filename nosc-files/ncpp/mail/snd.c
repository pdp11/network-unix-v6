#/*
Module Name:
	snd.c

Installation:
	if $1e = finale goto finale
	cc -c snd.c
	if ! -r snd.c exit
	cc snd.o /usr/net/hnconv.c
	rm -f snd.o hnconv.o
	exit
: finale
	cc -O -c -s snd.c
	if ! -r snd.o exit
	cc -O -s snd.o /usr/net/hnconv.c
	rm -f snd.o hnconv.o
	if ! -r a.out exit
	su cp a.out /bin/snd
	rm a.out

Module History:
(((Most recent changes first)))
   change NOSC-SDL to be the HOSTNAME value in siteinfo.h; also to get the
      local IMP number from NETNUMBER in siteinfo.h
   change RAND-Unix to be NOSC-SDL
   take '\n' or EOF to be same as 'queue'
   change ISD to UNIX in 'from'
   don't include final line of dashes in unsent.mail
   delete files before leaving
   omit ' at RAND-ISD' in 'From' field for local mailboxes
   fixed write-length in unsent.mail
   restore @ as line-delete before asking for subject, not after
   added 'copy of mail for xxx' to messages for snoop copy
   restore @ as line-delete before exiting if no 'To' list given
   changed SNOOP to MAILCOPY
   check for null file name
   added message separaters
   took out ridiculous editing stuff and special \ handling, added
      ability to call re, ed, nroff
*/
#include "/usr/sys/h/siteinfo.h"
#define MSGSEP "\001\001\001\001\n"
#define MSGSEPL 5
#define loop for(;;)
/* #define SNOOP "MAILCOPY@RCC\n"  /* 0 or snoop mailbox name ended with \n */
#define SNOOP 0  /* */
#define LOCIMP (NETNUMBER&077)        /* local imp number */
#define true 1
#define false 0
#define SMALLBUFLEN 256
#define controla 001
#define controlb 002
#define controlc 003
#define controld 004
#define controlf 006
#define controlh 010
#define controlq 021
#define controlr 022
#define controls 023
#define controlw 027
#define controlx 030
#define controlz 032
#define backspace 036
#define del 0177
#define abortchar 034

int goaway1(), goaway2();

struct status
	{
	char	s_minor;
	char	s_major;
	int	s_number;
	int	s_mode;
	char	s_nlinks;
	char	s_uid;
	char	s_gid;
	char	s_size0;
	int	s_size1;
	int	s_addr[8];
	int	s_actime[2];
	int	s_modtime[2];
	} statb;

struct  {
	char    ispeed;
	char    ospeed;
	char    erase;
	char    kill;
	int     mode;
	} *sttyptr &ttys;
struct iobuf {
	int fildes;
	int nleft;
	char *nextp;
	char buff[512];
	} afilebuf, ifilebuf;
extern struct iobuf *fin;
int savekill;
int pwfd;
int afile;
int tmpfd;
int fd;
int namefd;
int mbfd;
char bigbuf[512];
char pwbuf[512];
char *pwptr;
int pwcount;
int filecnt;
int netmsgs;
char cc[SMALLBUFLEN],to[SMALLBUFLEN],subject[SMALLBUFLEN];
char answer[10];
char user[40];
char dbuf[32];
char mailbox[64],netfile[64],from[64],returnto[64];
char hostname[64];
char realname[80];
char fn1[25],fn2[25],*fnx,*fny;
char *letter,*date;
int ttys[3];
int oldmode,uid,gid,myuid;
int unsent false;
int snoopcopy false;    /* true if called by ftp for snoop's copy */
int ftp false;          /* true if called by ftp */
int snoop false;        /* becomes true if any non-Rand recipients */
int file false;
int oldfile false;
int orighost;
char hex[] "0123456789ABCDEF";

/* name:
	send

function:
	A message sending program modeled after the tenex program
	sndmsg.

parameters:
	None have been defined as of yet.

returns:
	It delivers local mail by appending it to a file called mailbox
	in the user's root.
	If mail is undeliverable, a copy of the letter is left in
	a file called unsent.mail, in the senders current directory.


 */
main(argc,argv)
char *argv[];
int argc;
{
char *lp;
register char *p1, *p2;
int register i;
int vector[2];
if ((pwfd = open("/etc/passwd",0)) < 0)
	{
	printf("Couldn't open password file!\n");
	exit();
	}
afile=fopen("/usr/net/aliases",&afilebuf);

if (argc==3 && *argv[0]=='\001')          /* is being called by FTP */
{       if ((tmpfd=open(argv[1],0)) < 0)  /* second argument is file name */
	    exit();
	orighost=argv[0][1];            /* originating host */
	returnto[0] = '\0';             /* can't return this */
	p1 = "mailer";
	p2 = from;
	while(*p1++ = *p1++);
	ftp++;                          /* don't try to add header! */
	send(argv[2],0);                /* third argument is user name */
	if ((orighost&077) != LOCIMP)
	    snoop++;
	snoopcopy = argv[2];
	if (snoop && SNOOP)
	    send(SNOOP,0);
	close(tmpfd);
	unlink(argv[1]);
	exit();
}

getname(from);
findlocal(from);
p1 = mailbox;
p2 = returnto;
while(*p2++ = *p1++);   /* save user's directory for returning undeliverable*/
time(vector);           /*  mail                                            */
chkill();       /* remove @ as line delete character */
date = arpadate(vector);
printf("To: "); gather(to);
if (*to == '\000')
{       printf("Can't send mail to nobody!\n");
	rstkill();
	exit();
}
printf("cc: "); gather(cc);
rstkill();      /* restore line delete */
printf("Subject: "); gather(subject);
input();
send(to,1);
send(cc,1);
if (snoop && SNOOP)
	{returnto[0] = '\0';    /* don't want to return this copy */
	 send(SNOOP,0);
	}
if (unsent)
{       fd = creat("unsent.mail",0600);
	if (fd < 0)
	{       printf("unable to create unsent.mail\n");
		exit(1);
	}
	seek(tmpfd,0,0);
	while(i=read(tmpfd,bigbuf,512)) write(fd,bigbuf,i);
	close(fd);
	printf("A copy of this letter has been left in 'unsent.mail'\n");
}

close(tmpfd);
unlink(fnx);
exit(0);
}
/* name:
	gather

function:
	read a single line of console input into the user specified
	buffer.

algorithm:
	Read a character:
	If it is an unescaped new line, null terminate the buffer and return.
	Otherwise, stash the character into the buffer.
	If the length of the buffer has been reached, echo a new line and
	return.

parameters:
	*char	pointer to user buffer of length SMALLBUFLEN

returns:
	nothing

globals:
	SMALLBUFLEN

calls:
	getchar

called by:
	main
	send


 */
gather(sp)
char *sp;
{
register char c;
register char *p;

p = sp;
while( ((c = getchar()) != '\n') && (c > 0) )
{       *p++ = c;
	if ((p - sp) >= SMALLBUFLEN) break;
}
*p = '\0';
}


/* name:
	writef

function:
	Write a null terminated string

algorithm:
	Determine the length of the string.
	Write it out.
 CAVEAT	This call is not buffered, so use it carefully or pay the cost
	of high I/O expense.  It was writted to decrease subroutine linkage
	 global variable referencing and module size.  But those wins can
	easily be lost if writef is consistantly called with small strings.

parameters:
	int	file descriptor of output file.
	*char	pointer to null terminated string.

returns:
	int	value returned by write.

globals:

calls:
	write	(system)

called by:
	lotsa people


 */
writef(afd,str)
int afd;
char *str;
{	register int count;
	register char *s;

	s = str;
	for(count=0; *s++; count++);

	return(write(afd,str,count));
}

writef1(afd,str)        /* same as writef except convert '@' to ' at ' */
int afd;
char *str;
{       char c;
	while (c = *str++)
	{       if (c == '@') write(afd," at ",4);
		else write(afd,&c,1);
	}
}
/* name:
	input

function:
	To accept the text of the message and allow re, ed, and nroff
	to be called.

algorithm:
	Read until end-of-file, then ask for one of the following:
	mail to be sent, more input to be typed, a file to be included,
	re or ed to be called, or nroff to be called to justify the
	message.

parameters:
	none

returns:
	With a null terminated letter in the string pointed to by
	the global variable letter.

globals:
	letter	address of the buffer.

calls:
	writef
	write	system
	printf
	getchar

called by:
	main


 */
input()
{
char *tp;
register char *p,c;
register int i;
char *fnt;
char *p1;
int infile;
int stat;
int npipe[2];
int pid;

p1 = bigbuf;
pid = getpid();
for (i=0; i<5; i++)
{       *p1++ = (pid&07)+'0';
	pid =>> 3;
}
*p1 = 0;
stcopy("/tmp/send",fn1);
stcat(bigbuf,fn1);
stcopy(fn1,fn2);
stcat("a",fn1);
stcat("b",fn2);
fnx = fn1;
fny = fn2;
signal(2,&goaway2);      /* make sure file is deleted if user hits del */
if ((tmpfd=creat(fnx,0666)) < 0)
{       printf("Can't create temporary file!\n");
	exit();
}
close(tmpfd);
tmpfd=open(fnx,2);      /* creat opens for output only! */
printf("Type message, end with ^D\n");

more:
while(i=read(0,bigbuf,512)) write(tmpfd,bigbuf,i); /* collect message */


loop
{       printf("\nqueue, send, re, ed, input, file, justify, ");
	printf("display, or ?: ");
	gather(bigbuf);
	if ((c=bigbuf[0]) == 's' || c == 'q' || c == '\n' || c <= 0) break;
	switch(c)
	{
	case 'r':
		if (tmpfd > 0)
		{       close(tmpfd);
			tmpfd = 0;
		}
		if (fork() == 0)
		{	setuid(getuid()&0377);
			execl("/bin/re","re",fnx,0);
			printf("can't find re\n!");
			exit();
		}
		signal(2, 1);	/* ignore dels while in re */
		wait(&stat);
		signal(2, &goaway2);	/* restore del handling */
		stcopy(fnx,bigbuf);
		stcat(".bak",bigbuf);
		unlink(bigbuf);         /* delete .bak file */
		break;
	case 'e':
		if (tmpfd > 0)
		{       close(tmpfd);
			tmpfd = 0;
		}
		if (fork() == 0)
		{	setuid(getuid()&0377);
			execl("/bin/ed","ed",fnx,0);
			printf("can't find ed!\n");
			exit();
		}
		signal(2, 1);	/* ignore dels while in ed */
		wait(&stat);
		signal(2, &goaway2);	/* restore del handling */
		break;
	case 'j':
		if (tmpfd > 0)
		{       close(tmpfd);
			tmpfd = 0;
		}
		if (fork() == 0)
		{       pipe(npipe);
			close(0);
			dup(npipe[0]);  /* make the pipe the standard input */
			writef(npipe[1],".c2 ~\n.cc ~\n~ec ~\n~~so ");
			write(npipe[1],fnx,15);
			writef(npipe[1],"\n~~pl 0\n");
			close(1);
			close(npipe[1]);
			creat(fny,0600);
			execl("/usr/bin/nroff","nroff",0);
			write(2,"can't find nroff!\n",18);
			exit(-1);
		}
		wait(&stat);
		if (((&stat & 0177400) >> 8) != -1)
		{       unlink(fnx);
			fnt = fnx; fnx = fny; fny = fnt; /* switch names */
		}
		break;

	case 'i':                       /* input from keyboard  */
		if (tmpfd <= 0)
		{       tmpfd = open(fnx,2);
			seek(tmpfd,0,2);
		}
		printf("Continue typing message, end with ^D\n");
		goto more;

	case 'f':                       /* include file         */
		printf("File: ");
		gather(bigbuf);
		if (bigbuf[0] == '\0') break;
		if ((infile=open(bigbuf,0)) < 0)
		{       printf("can't open file\n");
			break;
		}
		if (tmpfd <= 0)
		{       tmpfd = open(fnx,2);
			seek(tmpfd,0,2);
		}
		while (i=read(infile,bigbuf,512)) write(tmpfd,bigbuf,i);
		close(infile);
		printf("File has been included\n");
		break;

	case 'd':                       /* display message      */
		if (tmpfd <= 0)
			tmpfd = open(fnx,2);
		seek(tmpfd,0,0);
		printf("Message:\n\n");
		while(i=read(tmpfd,bigbuf,512)) write(1,bigbuf,i);
		break;

	case '?':
	   /*   printf("send  will wait for message to be sent\n"); */
		printf("queue will wait for local copies of the message\n");
		printf("       to be sent and will queue network copies\n");
		printf("send  is (for the time being) the same as queue\n");
		printf("input will let you type in more of the message\n");
		printf("file  will include a file as part of the message\n");
		printf("re    will call the rand editor\n");
		printf("ed    will call the unix editor\n");
		printf("justify will justify the borders of the message\n");
		printf("display will display the message\n");
		break;

	default:
		printf("For help type ?\n");
	}   /* end of switch */
}  /* end of loop */
if (c <= 0)
	printf("\n");
if (tmpfd <= 0)
	tmpfd = open(fnx,2);
}
/* name:
	chkill, rstkill, goaway1, goaway2

function:
	to remove and restore the line delete character;
	to take appropriate actions before terminating from a del.
globals:
	oldmode

calls:
	stty
	exit
	unlink

called by:
	input


 */
chkill()
{       gtty(0,ttys);
	savekill = ttys[1];
	sttyptr->kill = '\177';  /* change line delete to nothing! */
	signal(2,&goaway1);
	stty(0,ttys);
}

rstkill()
{       ttys[1] = savekill;
	stty(0,ttys);
	signal(2,0);
}

goaway1()
{       ttys[1] = savekill;
	stty(0,ttys);
	exit();
}

goaway2()
{       unlink(fnx);
	exit();
}



/* name:
	send

function:
	To take a list of recipients and take steps to deliver the
	message (now in the temp file) to all of them.

algorithm:
	parse off a name.
	If it is a name:, open that file and read from it for a while.
	If it is a local user id, call post
	If it is a network user id, call netpost

parameters:
	Pointer to a null terminated characterstring which is a list
	of recipients.

returns:

globals:
	unsent

calls:
	valid
	open	(system)
	printf
	network
	findlocal
	netpost
	post

called by:
	main


 */
send(list,infoflg)
char *list;
int infoflg;
{
register char *name,*curp;
register char c;
int ret;
int alias;
char *ap;

alias = false;
curp = list;
while (alias || file || *curp)
{	name = user;
	while (valid(c = file?getc(&ifilebuf):(alias?*ap++:*curp++)))
		*name++ = c;
	*name = '\0';
	switch (c)
		{
case ':':	if (file)
			{if (infoflg)
			    printf("Nested name files not allowed.\n");
			 continue;
			}


		if ((namefd = fopen(user,&ifilebuf)) < 0)
			{
			if (infoflg) printf("Couldn't open %s\n",user);
			unsent++;
			continue;
			}


		file++;
		continue;

case -1:        close(ifilebuf.fildes);
		file = false;
		continue;
case '\0':      if (!(file|alias))
			--curp;
case '\n':      if (!file && alias)
			alias = false;
case ',':
case ' ':
		if (*user == '\000') continue;
		if (infoflg) printf("%s -- ",user);
		if (ret=network(user))
		{       if (ret == -1)
			{       if (infoflg) printf("host unknown\n");
				unsent++;
				continue;
			}
			netpost(user,hostname,infoflg);
			continue;
		}
		if (!alias && findalias(user))
		{       alias++;
			ap = realname;
			if (infoflg) printf("(alias for %s)\n",realname);
			continue;
		}
		if (findlocal(user))
		{       stcat("/.mail",mailbox);
			post(infoflg);
			continue;
		}

		{       if (infoflg) printf("not a local user\n");
			unsent++;
			continue;
		}

default:        if (infoflg)
			{printf("Illegal terminator %c.\n",c);
			 printf("User name %s ignored.\n",user);
			}
		} /* end switch */
	}
} /* end send */
/* name:
	findlocal

function:
	To ascertain whether or not someone is a local user, and
	if he is, to find the name of his root directory.

algorithm:
	Rewind the password file.
	Search it for a line starting with the user name.
	If found:
		extract all the info about uid, gid mailbox, etc.
		return.
	Else, continue the search.
	Announce that he isn't a known user.

parameters:
	*char	pointer to null terminated user name.

returns:
	boolean	whether or not he was found.

globals:
	pwfd
	uid
	gid
	mailbox
	pwcount

calls:
	getpwchar
	seek	(system)

called by:
	send


 */
findlocal(name)
char *name;
{
register char *np;
register char pwc;

/*	rewind the password file */
seek(pwfd,0,0);
pwcount = 0;

pwc = 1;
while(pwc > 0)
{	np = name;	/* search for a line starting with name */
	for(pwc = getpwchar(); pwc == *np++; pwc = getpwchar());
	if ((pwc == ':') && (*--np == '\000')) goto gotuser;
	while( (pwc != '\n') && (pwc > 0) ) pwc = getpwchar();
}
return(false);

gotuser:	/* extract all neat info from pw entry */
for(pwc = getpwchar(); pwc != ':'; pwc = getpwchar());
/* we have skipped over the password */
for(uid = 0; (pwc = getpwchar()) != ':'; uid =+ (9*uid) + pwc - '0');
for(gid = 0; (pwc = getpwchar()) != ':'; gid =+ (9*gid) + pwc - '0');
for(pwc = getpwchar(); pwc != ':'; pwc = getpwchar());
/* we have skipped over the gecos bin */
np = mailbox;
for(pwc = getpwchar(); pwc != ':'; pwc = getpwchar()) *np++ = pwc;
*np = '\000';
return(true);
}
/*name:
	findalias

function:
	To determine whether a name is in the alias file, and
	if so to replace it with the real name.

algorithm:
	Search the file for a line beginning with the name.
	If found, move the real name(s) to 'realname'.

globals:
	afile
	afilebuf
	realname

calls:
	getc(III)
	seek(II)

called by:
	send

 */
findalias(name)
char *name;
{
register char c, *p;

if (afile<0) return(false);  /* alias file could not be opened */
seek(afilebuf.fildes,0,0);
loop
{       for (p=name; *p++ == (c=getc(&afilebuf));); /* check for same name */
	if (c<0) return(false);
	if ((*--p == '\0') && (c == ':')) break;  /* got it */
	while(getc(&afilebuf) != '\n'); /* get to end of this line */
}
p = realname;              /* move stuff after : to realname */
while ((*p++ = getc(&afilebuf)) != '\n');
*--p = '\0';
*++p = '\n';
return(true);
}
/* name:
	network

function:
	To determine whether a user is a network user, and if he
	is, to break the name up into a hostname and ultimately
	a list of user names.

algorithm:
	Look for an @ in the name, if not found, not net
	Copy the hostname into a separate buffer and truncate
		it off of the user name.

parameters:
	*char	pointer to null terminated user name.

returns:
	boolean	Whether or not guy was a network user.
		(will be -1 if network site name unknown)
	and	if he was:
		Hostname has been truncated off of user name and
		put into the global buffer hostname.

globals:
	hostname.

calls:
	hnconf (not part of this source) to verify host name

called by:
	send


 */
network(name)
char *name;
{	register char *c;
	register char *d;
	int hostnum;

	for(c = name; *c; c++) if (*c == '@') goto netname;
	return(false);

netname:
	*c++ = '\000';
	d = hostname;
	while(*d++ = *c++);
	if ((hostnum=atoi(hnconv(hostname))) <= 0)
		return(-1);
	if ((hostnum&077) != LOCIMP) snoop++; /* check for rand (any host) */
	return(true);
}
/* name:
	post

function:
	To mail a letter (in tempfile) to some individual unix user.

algorithm:
	Create a mailbox if he doesn't have one.
	If he had one make sure it is not in use.
	Append the contents of the tempfile to it.
	Make recipient the owner if he wasn't already.

parameters:
	none
	but..	null terminated name of recipients directory is in in mailbox.

returns:
	boolean	Whether or not it was successfully sent.

globals:
	mailbox
	mbfd
	tmpfd
	bigbuf
	unsent

calls:
	creat
	seek
	read
	write
	printf
	stcat

called by:
	send


 */
post(infoflg)
int infoflg;
{
	register int i;

	if (stat(mailbox,&statb) < 0)
	{       mbfd = creat(mailbox,0666);
		if (mbfd < 0)
		{       if (infoflg) printf("Unable to create mailbox\n");
			unsent++;
			return(false);
		}
	}
	else
	{ /***  if (statb.s_nlinks > 1)
		{       if (infoflg) printf("Mailbox is busy\n");
			unsent++;
			return(false);
		}
	  ***/  mbfd = open(mailbox,1);
		if (mbfd < 0)
		{       if (infoflg) printf("Mailbox is busy\n");
			unsent++;
			return(false);
		}
		seek(mbfd,0,2);	/* seek end of file */
	}

	seek(tmpfd,0,0); /* rewind the file containing the message */
	write(mbfd,MSGSEP,MSGSEPL);
    /*  writef(mbfd,"Mail received for ");
	writef(mbfd,user);
	writef(mbfd,"\n");
    */  if (!ftp)
	{       write(mbfd,"Date: ",6);
		writef(mbfd,date);
		write(mbfd,"\nFrom: ",7);
		writef(mbfd,from);
		if (*subject)
		{       write(mbfd,"\nSubject: ",10);
			writef(mbfd,subject);
		}
		write(mbfd,"\nTo: ",5);
		writef1(mbfd,to);
		if (*cc)
		{       write(mbfd,"\ncc: ",5);
			writef1(mbfd,cc);
		}
		write(mbfd,"\n\n",2);
	}
	while((i = read(tmpfd,bigbuf,512)) > 0) write(mbfd,bigbuf,i);
	if (!ftp) writef(mbfd,"-------\n");
	write(mbfd,MSGSEP,MSGSEPL);
	close(mbfd);
	chown(mailbox,uid);
	if (infoflg) printf("ok\n");
	return(true);
} /* end post */
/* name:
	netpost

function:
	to attempt to mail off a piece of network mail.

algorithm:
	Contrive a unique name for a network mail file.
	Create it in the netmail daemon's mailbox
	put destination host, users and sender names into file.
	copy message into the file.

parameters:
	*char	pointer to null terminated user name
	*char	pointer to null terminated host name

returns:
	boolean	whether or not it worked

globals:
	unsent
	netmsgs		count of number of net msgs so far
	tmpfd
	bigbuf

calls:
	read
	write
	printf
	creat
	close
	stat
	chown

called by:
	send


 */
netpost(uname,hname,infoflg)
 char *uname;
 char *hname;
 int infoflg;
{
	register char *p1;
	register char *p2;
	register int i;


	p2 = netfile;
	for(p1 = "/usr/netmail/"; *p1; *p2++ = *p1++);
	crname(p2);
	mbfd = creat(netfile, 0444);
	if (mbfd < 0)
	{       if (infoflg) printf("Unable to create mailfile for %s@%s\n",
			uname,hname);
		unsent++;
		return(false);
	}

	writef(mbfd,hname);
	write (mbfd,":",1);
	writef(mbfd,uname);
	write (mbfd,":",1);
	writef(mbfd,returnto);
	write (mbfd, ":\n",2);
	if (snoopcopy)
	{       writef(mbfd,"Copy of mail received for ");
		writef(mbfd,snoopcopy);
		writef(mbfd,"\n");
	}
	if (!ftp)
	{       write(mbfd,"Date: ",6);
		writef(mbfd,date);
		write(mbfd,"\nFrom: ",7);
		writef(mbfd,from);
		writef(mbfd," at ");
		writef(mbfd,HOSTNAME);
		if (*subject)
		{       write(mbfd,"\nSubject: ",10);
			writef(mbfd,subject);
		}
		write(mbfd,"\nTo: ",5);
		writef1(mbfd,to);
		if (*cc)
		{       write(mbfd,"\ncc: ",5);
			writef1(mbfd,cc);
		}
		write(mbfd,"\n\n",2);
	}
	seek(tmpfd, 0, 0);
	while ((i = read(tmpfd,bigbuf,512)) > 0)
		write(mbfd,bigbuf,i);
	if (!ftp) writef(mbfd,"-------\n");

	close(mbfd);
	chown(netfile,myuid);
	if (infoflg) printf("queued\n");
	netmsgs++;
	return(true);
}

/* name:
	getname 

function:
	To find out the name of the user who forked me.

algorithm:
	Get my real user id and look it up in the password table

parameters:
	*char	pointer to buffer where name should be placed.

returns:
	*char pointer to buffer wher callers's name was placed.

globals:

calls:
	getuid
	getpw
	printf

called by:
	main


 */
getname(caller)
char *caller;
{
char linebuf[100];
char *cp,*lp;

myuid = getuid() & 0377;
if (getpw(myuid, linebuf))
{
	printf("Your user id is not in the password file?!?\n");
	exit();
}
cp = caller;
lp = linebuf;
while(*lp != ':') *cp++ = *lp++;
*cp = '\000';
return(caller);
}


char getpwchar()
{
tryagain:
	if (pwcount > 0)
	{	pwcount--;
		return(*pwptr++);
	}
	else
	{	pwcount = read(pwfd,pwbuf,512);
		if (pwcount <= 0) return(-1);
		pwptr = pwbuf;
		goto tryagain;
	}
}
valid(ch)
char ch;
{
return(((ch >= 'a') && (ch <= 'z')) | ((ch >= '0') && (ch <= '9')) |
	(ch == '.') | (ch == '/') | (ch == '-') | (ch == ';') |
	(ch == '@') | (ch == '(') | (ch == ')') |
	((ch >= 'A') && (ch <= 'Z'))   );
}
stcopy(a,b)
char *a,*b;
{
register char *p,*q;
p = a;
q = b;
while(*q++ = *p++);
}


stcat(latter,former)
char *latter,*former;
{
register char *fp,*lp;
fp = former;
lp = latter;
while (*fp++);
fp--;
while (*fp++ = *lp++);
}
/* name:
	arpadate

function:
	to get an ARPA format date (format described in rfc 680)

algorithm:
	Get the unix format date and jumble it around to be arpa format
	Unix format is:
	day mmm dd hh:mm:ss yyyy
	Arpa format is:
	dd mmm yyyy at hhmm-zzz

parameters:
	pointer to the doubleword time

returns:
	pointer to the arpa format date corresponding to that time

globals:
	dbuf

calls:
	ctime
	localtime

called by:
	main


 */
char *arpadate(tvec)
 int tvec[2];
{	register int *i;
	register char *p;
	register char *t;

	t = ctime(tvec);
	p = dbuf;

	*p++ = t[8];
	*p++ = t[9];
	*p++ = ' ';
	*p++ = t[4];
	*p++ = t[5];
	*p++ = t[6];
	*p++ = ' ';
	*p++ = t[20];
	*p++ = t[21];
	*p++ = t[22];
	*p++ = t[23];
	*p++ = ' ';
	*p++ = 'a';
	*p++ = 't';
	*p++ = ' ';
	*p++ = t[11];
	*p++ = t[12];
	*p++ = t[14];
	*p++ = t[15];
	*p++ = '-';
	*p++ = 'P';
	i = localtime(tvec);
	if (i[8]) *p++ = 'D';
	else *p++ = 'S';
	*p++ = 'T';
	*p++ = '\000';

	return(dbuf);
}
/*name:
	crname

function:
	to create a unique file name

algorithm:
	use the process id number, the date and time, and a reference count,
	convert to hexadecimal

paramters:
	pointer to where the name is to be stored

globals:
	hex

calls:
	getpid
	time

called by:
	netpost

*/
crname(q)
  char *q;
{
	int i;
	int tvec[4];
	char *p;

	p = &tvec[0];
	tvec[2] = getpid();
	tvec[3] = filecnt++;
	if (filecnt==256)
		{filecnt = 0;
		sleep(1);
		}
	time(tvec);
	for (i=7; i; --i)
	{       *q++ = hex[(*p>>4)&017];
		*q++ = hex[ *p++  &017];
	}
	*q = '\0';
}
