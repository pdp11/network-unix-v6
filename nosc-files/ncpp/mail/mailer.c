#/*
Module Name:
	mailer.c -- ARPAnet mailer daemon

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc -c mailer.c
	if ! -r mailer.o exit
	cc mailer.o /usr/net/hnconv.c
	rm -f mailer.o hnconv.o
	exit
: newer
	if ! { newer mailer.c /usr/net/etc/mailer } exit
	echo ncpp/mail/mailer.c:
: finale
	cc -O -c mailer.c
	if ! -r mailer.o exit
	cc -O -s mailer.o /usr/net/hnconv.c -lj
	if ! -r a.out exit
	if ! -r /usr/sys/ncpp/mail/mailer.c goto same
	if { cmp -s mailer.c /usr/sys/ncpp/mail/mailer.c } goto same
	su cp mailer.c /usr/sys/ncpp/mail/mailer.c
	su chmod 444 /usr/sys/ncpp/mail/mailer.c
	rm -f mailer.c
: same
	if -r /usr/net/etc/mailer su rm -f /usr/net/etc/mailer
	su cp a.out /usr/net/etc/mailer
	rm -f a.out mailer.o hnconv.o
	su chmod 555 /usr/net/etc/mailer
	su chown bin /usr/net/etc/mailer
	su chgrp bin /usr/net/etc/mailer

Synopsis:
	mailer [ delay-time [ bypass-count [ log-word [ debug-dir ] ] ] ]

Function:
	Deliver ARPAnet mail.

Module History:
	modified for Illinois NCP
	return undeliverable mail as a message
	fixed write length in returned.mail
	6Mar79 Greg Noel.  Introduced paranoia about slugish hosts.
	30Apr79 Greg Noel.  Transient remote errors are now retried later.
	15May79 Greg Noel.  450 User Unknown is hard error, not transient.
*/
#include "net_open.h"

struct openparam openparam;

#define true 1
#define false 0
#define SLPTIME  600 /* default sleep time   */
#define NTBYPASS 3   /* default bypass count */
#define MSGSEP "\001\001\001\001\n"
#define MSGSEPL 5

char hostname[100];
char mailcomd[256] { "mail <list of user names>                           "};
char returnto[256];
char linebuf[256];
char badhost[256];
char mailbuf[512];
char netbuf[512];


int dirfd;
int nflag, nflag1, tvec[2];
int slptime;
int ntbypass;

/*	don't separate these two declarations	*/
int entry;
char filename[16];
/*	don't separate the preceding two declarations */

int statbuf[32];
int timoutflg;
int mailproc;
int timeout();
struct fcb {
	int f_fd;
	char *f_ptr;
	char *f_buf;
	int f_count;
}
	netfcb,mailfcb;

/* name:
	maildaemon

function:
	To see if there is any mail waiting delivery to some other
	site on the network, and deliver it.

algorithm:
	If there is any mail, send it.
	Wait a while and look for mail again.

parameters:
	If argv[1] is present, it is the number of seconds to sleep
	between passes through the mail directory.  If it is absent
	or 0, a default value is used (SLPTIME).
	If argv[2] is present, it is the number of passes through
	which to bypass mail for hosts which are not responding.
	If argv[3] is present, messages will be written out indicating
	the progress of the mailer.
	If argv[4] is present, use this as the mail directory and make
	one pass through it.  Used for debugging.

returns:

globals:
	dirfd		fd of /usr/netmail

calls:
	open	sys
	seek	sys
	printf	library
	get_next_file
	ship_it_off

called by:

history:
	Design and initial coding by Mark Kampe, 12/27/75

 */
main(argc, argv)
 int argc;
 char **argv;
{
	int i;
	extern int fout;

	signal(1,1);
	signal(2,1);
	signal(3,1);

	mailproc = getpid();
	if (argc < 2 ||  (slptime=atoi(argv[1])) <= 0) slptime = SLPTIME;
	if (argc < 3 || (ntbypass=atoi(argv[2])) <= 0) ntbypass = NTBYPASS;
	if (argc > 3) nflag++;

	if (chdir(argc > 4 ? argv[4] : "/usr/netmail") < 0  ||
	   (dirfd = open(".",0)) < 0) {
		printf("Unable to open /usr/netmail\n");
		exit(-1);
	}

	fout = dup(1);	/* buffer all further output */
	log("Sleep time = %d, Bypass = %d\n", slptime, ntbypass);
	nflag1 = 0;

	openparam.o_host = 0;
	netfcb.f_fd = -1;
loop:
		/*
		 *  The mailer should lock this directory so that conflicts
		 *  with other incarnations of the mailer will not occur.
		 *  The lock should be established here and removed below.
		 *  The routine get_next_file must be modified to skip
		 *  over the lock.  Another alternative is to fork for each
		 *  new destination host encountered and lock only on the
		 *  basis of destination; this will maximize parallelism
		 *  of transmission.
		 */
	seek(dirfd,0,0);
	while(get_next_file())
		ship_it_off();
	closenet();	/* close last connection */

	for (i=0; i<256; i++)
	if (badhost[i]) --badhost[i];

	if(nflag1) {
		log("Ending cycle\n");
		nflag1 = 0;
	}

		/* Remove lock here.... */

	if(argc>4) exit(0);	/* used for testing */

	sleep(slptime);
	goto loop;
}

/* name:
	log

function:
	to log something if logging is enabled.

algorithm:
	if logging is enabled
		print leading banner
		print message to be logged

parameters:
	identical to printf

returns:
	nothing

globals:
	nflag	set if logging is enabled
	tvec	time at which current cycle began

calls:
	printf	(sys)
	time	(sys)
	ctime	(sys)

called by:
	many places

history:
	24 Aug 77 coded by Greg Noel
	24 Jul 78 modified by Greg Noel to condense logging printout

*/
log(t, a, b, c, d)
{
	if (nflag)
	{	time(tvec);
		printf("%-16.16sMailer: ", ctime(tvec)+4);
		printf(t, a, b, c, d);
		flush();
		nflag1++;
	}
}

/* name:
	get_next_file

function:
	To find get the next file in /usr/netmail and set up all
	of the parameters for the mailing.

algorithm:
	Read next non-empty record
	if end of file, return error
	set up hostname
		mail command line
		file name (for future deletion)
		root of sender for possible return.
	return true

parameters:

returns:
	boolean	true means a file was found and set up
		false means no more files in directory

globals:
	hostname
	mailcomd
	mailfcb
	filename
	dirfd
	returnto

calls:
	read	(sys)
	open	(sys)
	getline
	unlink	(sys)

called by:
	main

history:
	Initial coding by Mark Kampe	11/27/75

 */
get_next_file()
{	register char *p;
	register char *s;
	register int i;

 loop:	i = read(dirfd,&entry,16);
	if (i != 16) return(false);
	if (entry == 0) goto loop;
	if (filename[0] == '.') goto loop;

	if (filename[13]) filename[14] = '\000';

	mailfcb.f_fd = open(filename,0);
	if (mailfcb.f_fd < 0)
		goto badfile;

	mailfcb.f_count = 0;
	mailfcb.f_buf = mailbuf;
	p = getline(&mailfcb);
	if (p == 0) goto badfile;

	s = hostname;
	while(*p != ':') if ((*s++ = *p++) == '\000') goto badfile;
	*s++ = '\000';
	p++;

	s = &mailcomd[5];
	while(*p != ':') if ((*s++ = *p++) == '\000') goto badfile;
	*s++ = '\000';
	p++;
	
	s = returnto;
	if (*p == ':')
		*s = '\0';
	else {  while(*p != ':') if ((*s++ = *p++) == '\000') goto badfile;
		for(p = "/.mail"; *s++ = *p++;);
	}

	return(true);

badfile:
	log("File %s in illegal format -- deleted\n", filename);
	unlink(filename);
	goto loop;
}
/* name:
	ship_it_off

function:
	To carry out the protocol to mail off a file over the network

algorithm:
	Connect to the forign host
	Await a ready command
	send the mail command
	Await the send your mail response.
	send the mail
	Await the confirmation.
	Delete the file if the transmission was successful.
	If there was a permanent failure, put the file in the sender's
	 root with an explanatory message.

parameters:

returns:

globals:
	hostname	contains name of foreign host
	mailcomd	contains a mail command with all appt names
	netfcb		file descriptor of network file
	filename	name of the current file
	returnto	sender's mailbox (if return is necessary);
	badhost

calls:
	getline
	writef
	reply
	open	sys
	close	sys
	getline

called by:
	main

history:
	Initial coding by Mark Kampe  12/27/75
	Modified by Greg Noel 7/24/78 to maintain connection if destination
	    host does not change

 */
ship_it_off()
{
	extern char *hnconv();
	int hnum;
	register char *p;
	register int i;
	register int rtnfd;
	int timeproc;

	netfcb.f_count = 0;
	openparam.o_fskt[1] = 3;
	openparam.o_timo = 60;
	if ((hnum=atoi(hnconv(hostname))) <= 0) /***/
	{	log("Mail in %s for %s at %s\n",
			filename, &mailcomd[5], hostname); /***/
		p = "Destination host is unknown.";
		goto fail;
	}

	if (badhost[hnum])
		goto nogood;

	log("Mail in %s for %s at %s\n", filename, &mailcomd[5], hostname);

	if ((openparam.o_host&0377) != hnum) {
		closenet();
		openparam.o_host = hnum;
		set_alarm(120);
		netfcb.f_fd = open("/dev/net/anyhost",&openparam);
		alarm(0);
		if (netfcb.f_fd <= 0)
		{	log("Unable to open connection\n");
			badhost[hnum] = ntbypass; /* number of times to bypass */
			goto nogood;
		}
		netfcb.f_count = 0;
		netfcb.f_buf = netbuf;

		do {	p = getline(&netfcb);
			if (p == 0) goto tempfail;
			i = reply(p);
			if (i >= 400) goto tempfail;
		} while(i != 300);
	}

	i = writef(netfcb.f_fd,mailcomd);
	if (i < 0) goto tempfail;
	i = writef(netfcb.f_fd,"\r\n");
	if (i < 0) goto tempfail;
	i = 0;
	while(i != 350)
	{	p = getline(&netfcb);
		if (p == 0) goto tempfail;
		i = reply(p);
		if (i == 951) continue;		/* Mail will be forwarded... */
		if (i == 504)
			switch (loginfirst())
			{	case -1: goto fail;
				case 0:  goto tempfail;
				case 1:  continue;
			};
		if (i >= 500) goto fail;
		if (i == 450) goto fail;	/* User unknown */
		if (i > 400) goto tempfail;
		if (i == 400) goto fail;	/* Service not implemented */
		if (i == 331) goto fail;	/* Need account parameter */
	}

	while(p = getline(&mailfcb)) {
		i = writef(netfcb.f_fd,p);
		if (i < 0) goto tempfail;
		i = writef(netfcb.f_fd,"\r\n");
		if (i < 0) goto tempfail;
	}

	i = writef(netfcb.f_fd, ".\r\n");
	if (i < 0) goto tempfail;

	i = 0;
	while(i != 256)
	{	p = getline(&netfcb);
		if (p == 0) goto tempfail;
		i = reply(p);
		if (i >= 500) goto fail;
		if (i > 400) goto tempfail;
		if (i > 300) goto fail;
	}

	close(mailfcb.f_fd);
	unlink(filename);
	return(true);

fail:
	log("Unrecoverable error; mail returned to sender\n");
	/* now return the message to the sender */
	if (returnto  &&
	       (   ((rtnfd=open(returnto,1))     >= 0) ||
		   ((rtnfd=creat(returnto,0666)) >= 0)   )   )
	{	seek(rtnfd,0,2);
		writef(rtnfd,"From: mailer\n");
		writef(rtnfd,"Subject: Undeliverable mail\n\n");
		writef(rtnfd, "Mail for "); writef(rtnfd, &mailcomd[5]);
		writef(rtnfd, " at "); writef(rtnfd, hostname);
		writef(rtnfd, " undeliverable because:\n\t");
		writef(rtnfd,p);
		writef(rtnfd,"\n------- Unsent message is below -------\n\n");
		seek(mailfcb.f_fd,0,0);
		mailbuf[0] = 0;
		while(mailbuf[0] != '\n') read(mailfcb.f_fd,mailbuf,1);
		while((i = read(mailfcb.f_fd,mailbuf,512)) > 0)
			write(rtnfd,mailbuf,i);
		close(rtnfd);
	}
	unlink(filename);
tempfail:
	closenet();
nogood:
	close(mailfcb.f_fd);
	return(false);
}

closenet()
{
	openparam.o_host = 0;
	if(netfcb.f_fd < 0) return;	/* not currently open */
	writef(netfcb.f_fd,"BYE\r\n");
	getline(&netfcb);	/* keep NCP happy until 'flush user' fixed */
	close(netfcb.f_fd);
	netfcb.f_fd = -1;
}

timeout()
{	timoutflg++;
	wait();		/* get rid of zombie */
	reset();	/* return to ship_it_off */
}

/* name:
	getline

function:
	To retrieve one "line" from the file on the specified descriptor.

algorithm:
	Ignore carriage returns.
	While the next character is not a new line, stash it in linebuf;
	null terminate the string in linebuf;
	return a pointer to linebuf;

	If we should reach end of file and have a line started, terminate
		it and return.  If no line is started, return a zero.

parameters:
	*fcb	file control block for the file in question.

returns:
	*char	pointer to a null terminated string.
	or a zero indicating end of file.

globals:
	linebuf		destination of read

calls:
	read	(sys)

called by:
	get_next_file
	ship_it_off

history:
	Initial coding by Mark Kampe 12/27/75

 */
getline(afcb)
 struct fcb *afcb;
{	register int count;
	register char *nextin;
	register char *nextout;
	char lastchar;

	count = afcb->f_count;
	nextin = afcb->f_ptr;
	nextout = linebuf;
	lastchar = '\000';

	while(lastchar != '\n') {
		if (count <= 0) {
			set_alarm(60);
			count = read(afcb->f_fd,afcb->f_buf,512);
			alarm(0);
			if (count <= 0) 
			{	count = -1;
				nextin = "\n";
			}
			else nextin = afcb->f_buf;
		}
		count--;
		lastchar = *nextin++;
		if(nextout < &linebuf[sizeof linebuf])
			*nextout++ = lastchar;
		if (lastchar == '\r') nextout--;
	}
	afcb->f_count = count;
	afcb->f_ptr = nextin;
	*--nextout = '\000';
	if (nextout == linebuf)
		if (count < 0) return(0);
	if (nextout == &linebuf[1] && linebuf[0] == '.') {
		*nextout++ = ' ';
		*nextout = '\0';
	}
	return(linebuf);
}
/* name:
	writef

function:
	To write a null terminated string out to the specified file descriptor.

algorithm:
	Find the length of the string.
	Write it out .

parameters:
	int	file descriptor
	*char	address of string to be written.

returns:
	int	value returned by write.

globals:

calls:
	write	(sys)

called by:
	ship_it_off

history:
	Initial coding by Mark Kampe 12/27/75

 */
writef(afd,as)
 int afd;
 char *as;
{	register char *s;
	register int i;
	register int j;

	i = 0;
	for(s = as; *s++; i++);
	if(i == 0) return 0;
	set_alarm(60);
	j = write(afd, as, i);
	alarm(0);
	if (j != i)	/* did error occur? */
	{	log("write(%d,0%o,%d) returned %d.\n",afd,as,i,j);
		log("line was '%s'.\n", as);
	}
	return(j);
}
/* name:
	reply

function:
	To find the reply number from a network message.

algorithm:
	Convert the first three characters into an integer.
	If any are non numeric, return a zero.

parameters:
	*char	pointer to the string in question (null terminated)

returns:
	int

globals:

calls:

called by:
	ship_it_off

history:
	Initial coding by Mark Kampe 12/27/75

 */
reply(ap)
 char *ap;
{	register char c;
	int i;
	register int j;
	register char *s;

	s = ap;
	log("%s\n",s);

	while((*s<'0') || (*s>'9')) if (*s++ == '\000') return(0);	/* fuckers at multics */

	j = 0;
	for(i = 3; i; i--)
	{	c = *s++;
		if ((c < '0') || (c > '9')) return(0);
		j =* 10;
		j =+ c;
		j =- '0';
	}
	return(j);
}
/* name:
	loginfirst

function:
	to login to a bastard host who insists on it

algorithm:
	send a "user NETML\npassNETML\n";
	wait for the 330 230
	resend the mail command

	if a transmission failure return 0
	if a bad reply return -1
	else return 1

parameters:

returns:
	-1	permanent failure
	0	temperary failure
	1	success

globals:
	netfcb
	mailcomd

calls:
	getline
	reply
	writef

called by:
	ship_it_off

history:
	Designed and coded by a disgruntled Mark Kampe 1/5/76

 */
loginfirst()
{	register int i;
	register char *p;

	i = writef(netfcb.f_fd,"USER NETML\r\n");
	if (i<=0) return(0);

	i = 0;
	while(i != 330)
	{	p = getline(&netfcb);
		if (p == 0) return(0);
		i = reply(p);
		if (i == 230) goto gotin;
		if (i >= 400) return(-1);
	};

	i = writef(netfcb.f_fd, "PASS NETML\r\n");
	if (i <= 0) return(0);
	i = 0;
	while(i != 230)
	{	p = getline(&netfcb);
		if (p == 0) return(0);
		i = reply(p);
		if (i >= 400) return(-1);
	}

	gotin:
	i = writef(netfcb.f_fd, mailcomd);
	if (i<=0) return(0);
	i = writef(netfcb.f_fd, "\r\n");
	if (i <= 0) return(0);

	return(1);
}
/*
name:
	set_alarm, ring

function:
	to terminate an operation if the remote host is too slugish.

algorithm:
	set an alarm for the specified period.
	when the alarm occurs, just return -- the interrupted system call
	    will return -1.

parameters:
	the number of seconds to wait

returns:
	yes

globals:
	none.

calls:
	alarm	(sys)
	signal	(sys)
	log

called by:
	several people.

history:
	coded 6Mar79 by Greg Noel
*/
set_alarm(v)
int v;
{
	int ring();

	signal(14, &ring);
	alarm(v);
}
ring()
{
	log("Timeout\n");
}
