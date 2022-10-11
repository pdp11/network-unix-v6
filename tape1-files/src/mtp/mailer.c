/*
 * mailer
 *
 * modified for Illinois NCP
 *  return undeliverable mail as a message
 *  fixed write length in returned mail
 * Modified by BBN(dan) July 12, 79: to change call to wait() to give
 *  it an argument.
 * Add immediate fork so it gets adopted by init jsq BBN 21July79
 * Modified by BBN(josh) Aug 10, 79: exec sndmsg to return mail to sender.
 * Modified to do logging of everything received over net as it is read,
 * give reason if net open failed, timeout on net reads,
 * diagnose errors when trying to return mail,
 * close stdin before calling sndmsg to ensure that it completes.
 *	BBN:dan May 7, 1980
 * Modified to delete printing of host number and host_hash value:
 *	BBN:dan May 12, 1980
 * Modified to use either mtp (the new mail transfer protocol - rfc780)
 * or ftp.  Removed code dealing with old style return addresses
 * Arranged to have messages going from an internal network
 * out to an external network pass through a filter that changes
 * names of hosts on the internal net to the name of the forwarder.
 * Also reformatted:
 *	BBN:  Eric Shienbrood 27 April 1981
 * Changed to stdio for all I/O except network writes.  Didn't change
 * this because it would have been too hard too check for write errors.
 * Also generally cleaned up code.
 *	BBN:  Eric Shienbrood 29 May 1981
 * Modified to work in either NCP or TCP environment.  To get a TCP mailer,
 * compile with TCP #defined.  Otherwise you get an NCP mailer.
 *	BBN:  Eric Shienbrood 1 June 1981
 * Modified to use new network library.  Installed new host-hashing package.
 *	BBN: Eric Shienbrood 15 Sept 1981
 */

#include <stdio.h>
#	include <con.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netlib.h>

#ifndef NETMAILDIR
#define NETMAILDIR "/usr/spool/netmail"
#endif


/*
 * The following macro returns TRUE if
 * <hostname> is on the network whose number
 * is <netnum>, FALSE otherwise
 */
#define is_on_net(hostname,netnum)	(!isbadhost(getnhost(hostname,netnum)))

#define TMPFILE	"/tmp/mailertmp"
#define HOSTFILTER "/etc/net/hostfilter"
#	define NETFILE "/dev/net/tcp"

#define SLPTIME	600		/* default sleep time */
#define RETDAYS 1		/* return mail which is undelivered after */
				/*   this many days */
#define DOWNRET		((long)(24L*60L*60L*RETDAYS))
#define NTBYPASS 3		/* default bypass count */

#define OPEN_TIMEOUT 60 	/* Timeout on network open call */
#define READ_TIMEOUT 300	/* Timeout on network read calls */
#define WRITE_TIMEOUT 30	/* Minimum timeout on network write calls */

#define MTPSOCK	57		/* MTP socket number */
#define FTPSOCK	3		/* FTP socket number */

#define TCP_CAP	1		/* Code used in host map to indicate host */
				/*    has TCP capability */
#define NCP_CAP	2		/* Same for NCP capability */

/* Codes returned by ship_it_off */

#define NO_OPEN		0
#define DID_OPEN	1
#define SENT_MAIL	2

#define FALSE		0
#define TRUE		1
/*
 * Following structure tells us how many times we should
 * skip a host because it is down (h_bypass) and
 * some information that we know about a host
 * (h_flags).  Currently, we only keep track of
 * whether or not each host speaks MTP (the default)
 * or only FTP.
 */
struct hostinfo {
	struct hostinfo *h_next;
	netaddr h_addr;
	char h_flags;
	char h_bypass;
};

char	sndhnm[60];		/* destination host name */
char	*dest_host;		/* host to which we are currently talking */
char	mailcomd[256];		/* buffer into which mtp or ftp */
				/*     mail command is constructed */
char	recipient[60];		/* name of destination mailbox */
char	sender[60];		/* name of sending mailbox */
char	linebuf[256];
char	mailbuf[512];
char	*ourname;		/* the name of this host */
netaddr	ourhostaddr;		/* net address of this host */

FILE	*dirf;
int	nflag;
int	timed_out;
int	slptime;
int	ntbypass;
char    *getline(), *getlc(), *ctime();
char	*net_getline();
struct hostinfo *host_lookup();

char	filename[16];

#define NO_MTP	1

#define OTIMEOUT OPEN_TIMEOUT
struct con openparam;

int netfd = -1;		/* file descriptor of network connection */
FILE *mailfile, *netin;

/* -------------------------- M A I L D A E M O N ------------------------- */
/*
 *	See if there is any mail waiting delivery to some other
 *	site on the network, and try to deliver it.
 *	Calling sequence is
 *
 *		mailer [slptime] [npass] [logflag]
 *
 *	If slptime is present, it is the number of seconds to sleep
 *	between passes through the mail directory.  If it is absent
 *	or 0, a default value is used(SLPTIME).
 *	If npass is present, it is the number of passes through
 *	which to bypass mail for hosts which are not responding.
 *	If logflag is not present or is non-zero, messages will be
 *	written out indicating the progress of the mailer.
 */

main(argc, argv)
int	argc;
char	**argv;
{
	time_t	tvec;

	/* Dup 1 into 2 so all error messages go to logfile: BBN(dan) */
	/* Make log output unbuffered: BBN(shienbrood) */
	dup2 (1, 2);
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	setsig(0);	/* will be changed while sleeping between passes */

	if (geteuid() != 0)
		ecmderr (0, "Mailer not running as root.\n");
	ourname = thisname();
	ourhostaddr = thishost();
	getlc(ourname);
	if (argc < 2 || (slptime = atoi(argv[1])) <= 0)
		slptime = SLPTIME;
	if (argc < 3 || (ntbypass = atoi(argv[2])) <= 0)
		ntbypass = NTBYPASS;
	if (argc > 3) {
		nflag = atoi(argv[3]);
		if (nflag < 0)
			nflag = 1;
	}
	else
		nflag = 1;

	if (!loadnetmap())
		ecmderr(0, "Error loading network map\n");
	if (chdir(NETMAILDIR) == -1)
		ecmderr(-1,
		   "Cannot change current directory to \"%s\".\n", NETMAILDIR);
	dirf = fopen(".", "r");
	if (dirf == NULL)
		ecmderr(-1, "Unable to open \"%s\".\n", NETMAILDIR);

loop:
	if (nflag) {
		time(&tvec);
		printf("\nStarting at %s", ctime(&tvec));
		printf("sleep time = %d, bypass = %d\n", slptime, ntbypass);
	}

	fseek(dirf, 0L, 0);
	while (get_next_file()) {
		if (ship_it_off() != NO_OPEN)
			sleep(30);
	}

	if (nflag) {
		time(&tvec);
		printf("Ending at %s", ctime(&tvec));
	}
	dec_bypass_cnt();

	setsig(1);    /* let signal 11 wake us up during wait between passes */
	xsleep(slptime);
	setsig(0);	    /* jsq BBN 11Jul19 */
	goto loop;
}

/* -------------------------- X S L E E P --------------------------------- */
/*
 * This routine is like the sleep library routine, but it returns
 * if interrupted by a signal, instead of going back to sleep.
 */

xsleep(n)
unsigned n;
{
	int sleepx();

	if (n==0)
		return;
	signal(SIGALRM, sleepx);
	alarm(n);
	pause();
	alarm(0);
	signal(SIGALRM, SIG_IGN);
}

sleepx() {}

dosig11()
{				       /* jsq BBN 11Jul19 */
	setsig(0);
	if (nflag)
		printf("\nAwakened by signal 11\n");
}

setsig(aflag)			       /* jsq BBN 11Jul19 */
{
	if (aflag)
		signal(11, dosig11);
	else
		signal(11, SIG_IGN);
}

/* -------------------------- T I M E O U T ------------------------------- */
/*
 * Set/reset timeout. timeout(period) [period > 0] causes a reset in
 * period seconds. timeout(0) turns it off. Check the global variable timed_out
 * to see if it happened.
 */
timeout(period)
int period;
{
	extern int timed_out;
	int clock_int();

	if (period > 0) {	/* Turn on */
		signal(SIGALRM, clock_int);
		alarm(period);
		timed_out = 0;
	}
	else {			/* Turn off */
		alarm(0);
		signal(SIGALRM, SIG_IGN);
	}
}

/* -------------------------- C L O C K _ I N T --------------------------- */
/*
 * Called on a SIGALRM signal.
 */
clock_int()
{
	extern int timed_out;

	timed_out++;
}

/* ----------------------- G E T _ N E X T _ F I L E --------------------- */
/*
 *	Find get the next file in NETMAILDIR and set up all
 *	of the parameters for the mailing.  Return TRUE if
 *	a file was found, FALSE if no more files in directory.
 */

get_next_file()
{
	register char  *p;
	register char  *s;
	char tempf[16];
	extern char *errmsg();
	extern char *getdirent();
	char *scopyto();

loop:
	if ((s = getdirent (dirf, NULL)) == NULL)
		return(FALSE);
	scopy(s, filename, 0);

	mailfile = fopen (filename, "r");
	if (mailfile == NULL) {
		printf("Could not open mailfile \"%s\": %s\n",
			filename, errmsg(-1));
		return(FALSE);
	}

	p = getline (mailfile);
	if (p == 0) {
		printf("No characters in mailfile \"%s\".\n", filename);
		goto badfile;
	}

	/*
	* The first line of the mailfile contains:
	*  the hostname
	*  the destination at that host
	*  the 'return-to' field, which is the name of
	*	the person to return the mail to.
	*/

	/* Copy the hostname into sndhnm. */

	p = scopyto(p, sndhnm, ":", sizeof(sndhnm));
	if (*p == '\0') {
		printf("no end to host name\n");
		goto badfile;
	}
	getlc (sndhnm);		/* Put hostname in lowercase */
	p++;

	/* Copy the addressee into the right place in the mail command. */
	p = scopyto(p, recipient, ":", sizeof(recipient));
	if (*p == '\0') {
		printf("no end to addressee\n");
		goto badfile;
	}
	p++;

	/* Copy the return address into sender. */
	if (*p == ':')		/* Null return address */
		scopy ("root", sender, 0);
	else {
		p = scopyto(p, sender, ":", sizeof(sender));
		if (*p == '\0') {
			printf("no return address\n");
			goto badfile;
		}
	}

	if (nflag)
		printf("Mail found in file %s for %s at %s\n",
			filename, recipient, sndhnm);
	return(TRUE);

badfile:
	printf("bad file found and removed:%s\n", filename);
	if (mailfile != NULL)
		fclose (mailfile);
	unlink(filename);
	goto loop;
}

/* ------------------------- S H I P _ I T _ O F F ------------------------ */
/*
 *	Carry out the protocol to mail off a file over the network.
 *	Tries using MTP first, FTP if that fails.
 *	A TCP mailer will use only MTP.
 *	If there is a permanent failure, the message is returned to
 *	the sender.
 *	Returns one of 3 values for the following 3 cases:
 *		a) Didn't even get as far as trying to open
 *		   a connection (destination host unknown, or
 *		   host bypassed because it has been down).
 *		b) Opened or tried to open a connection,
 *		   but didn't succeed in sending mail.
 *		c) Successfully transmitted mail.
 */

ship_it_off()
{
	netaddr hnum;
	time_t life;
	register char *p;
	register char *datestr;
	register int i, j;
	register FILE *rtnfile;
	int proto, wantreply, fd, errnum, retcode, downdays;
	static char netbuf[512];
	char from[60];
	extern int errno;
	extern char *sfind();
	extern char *errmsg();
	struct stat filestat;
	struct hostinfo *hp;

	retcode = NO_OPEN;
	dest_host = sndhnm;
	hnum = gethost(sndhnm);
	if (isbadhost(hnum)) {
		p = "Destination host is unknown.";
		goto fail;
	}

	if ((hp = host_lookup(hnum))->h_bypass) {
		if (nflag)
			printf("Message bypassed because host has been down.\n");
		goto nogood;
	}

	if (*(sfind (sender, "@")) == '\0')
		sprintf (from, "%s@%s", sender, ourname);
	else
		scopy (sender, from, 0);

	/*
	 * The first time we connect to each host,
	 * we assume that it speaks mtp.  If we
	 * time out waiting for a connection on the
	 * mtp socket, we decide the host doesn't speak
	 * mtp, and from then on we only try that host
	 * on the ftp socket.  The TCP mailer only speaks
	 * on the mtp port.
	 */
#	define FTP 0
#	define MTP 1
		proto = MTP;
		sprintf (mailcomd, "mail from:<%s> to:<%s@%s>",
				from, recipient, sndhnm);
		setup_open (&openparam, hnum, MTPSOCK, OTIMEOUT);

top:
	if (nflag)
		printf("Attempting connection to %s on %s socket.\n",
			dest_host, proto == FTP ? "FTP" : "MTP");

	retcode = DID_OPEN;
	netfd = open(NETFILE, &openparam);
	if (netfd < 0) {
		errnum = errno;
		if (nflag)
			printf("Unable to open connection: %s\n", errmsg(errnum));
		/*
		 * Record fact that host was down.
		 */
		host_lookup(hnum)->h_bypass = ntbypass;
		stat(filename, &filestat);
		if ((life = filestat.st_atime - filestat.st_mtime) > DOWNRET) {
			if (nflag)
				printf("\tmessage will be returned to sender -- host down for more\n\tthan %ld secs\n", DOWNRET);
			p = "Host has been down for over 1 day";
			goto fail;
		}
		goto nogood;
	}
	ioctl (netfd, NETSETE, NULL);
	netin = fdopen (netfd, "r");
	i = 0;
	wantreply = proto == FTP ? 300 : 220;
	while (i != wantreply) {
		p = net_getline(netin, nflag);
		if (p == 0)
			goto tempfail;
		i = reply(p);
		if (i >= 400)
			goto tempfail;
	}
	i = net_putline (netfd, mailcomd);
	if (i < 0)
		goto tempfail;
	i = 0;
	wantreply = proto == FTP ? 350 : 354;
	while (i != wantreply) {
		p = net_getline(netin, nflag);
		if (p == 0)
			goto tempfail;
		i = reply(p);
		if (proto == FTP) {
			if (i == 331)
				goto fail;
			if (i == 504)
				switch(loginfirst()) {
				case -1:
					goto fail;
				case 0:
					goto tempfail;
				case 1:
					continue;
				}

			/*
			 * "user unknown" codes from various places
			 * 	jsq BBN 10Jul79
			 */
			if (i == 450 || i == 451 || i == 431)
				goto fail;
		}
		if (i >= 400 && i < 500)
			goto tempfail;
		if (i >= 500 && !(proto == FTP && i == 951))
			goto fail;
	}

	while (p = getline(mailfile)) {
		if (p[0] == '.' && proto == MTP)
			*--p = '.';	/* Double leading period */
		i = net_putline (netfd, p);
		if (i < 0)
			goto tempfail;
	}

	i = net_putline (netfd, ".");
	printf("Finished sending mail.\n");
	if (i < 0)
		goto tempfail;

	i = 0;
	wantreply = proto == FTP ? 256 : 250;
	while (i != wantreply) {
		p = net_getline(netin, nflag);
		if (p == 0)
			goto tempfail;
		i = reply(p);
		if (i > 300)
			goto fail;
	}

	net_putline (netfd, proto == FTP ? "bye" : "quit");
		/* keep NCP happy until 'flush user' fixed */
	net_getline(netin, nflag);
	fclose(netin);
	fclose(mailfile);
	unlink(filename);
	if (nflag)
		printf("Transmission completed\n");
	return(SENT_MAIL);

fail:
	if (nflag)
		printf("Unrecoverable error\n");
	/* Return the message to the sender */

	close (creat( TMPFILE, 0666));
	if ((rtnfile = fopen (TMPFILE, "w")) == NULL) {
		cmderr(-1, "Cannot create \"%s\".\n", TMPFILE);
		goto done;
	}
	if (nflag)
		printf("Returning mail to %s\n", sender);
	time(&life);
	datestr = ctime(&life);
	datestr[3] = datestr[7] = datestr[10] =
					datestr[19] = datestr[24] = '\0';
	fprintf (rtnfile, "Date: %s %s %s %s (%s)\n", &datestr[8], &datestr[4],
						&datestr[20], &datestr[11],
						&datestr[0]);
	fprintf (rtnfile, "From: ~MAILER~DAEMON at %s\n", thisname());
	fprintf (rtnfile, "Subject: Undeliverable mail\n");
	fprintf (rtnfile, "To: %s\n\n", sender);
	fprintf (rtnfile, "Mail addressed to %s at %s could not be sent.\n", 
			recipient, sndhnm);
	fprintf(rtnfile, "%s\n------- Unsent message is below -------\n\n", p);
	fclose(mailfile);
	if ((mailfile = fopen(filename, "r")) == NULL) {
		cmderr (-1, "Couldn't open mail file.\n");
		goto done;
	}
	getline (mailfile);
	while ((i = fread(mailbuf, 1, 512, mailfile)) > 0)
		fwrite(mailbuf, 1, i, rtnfile);
	fclose(rtnfile);
	close(0);	/* Make sure sndmsg reads EOF */
	open(TMPFILE, 0);
	execute("/etc/delivermail", "delivermail", "-f",
		"~MAILER~DAEMON", sender, 0);
	close(0);
	unlink(TMPFILE);
done:
	unlink(filename);
tempfail:
	if (nflag)
		printf("Failure during transmission: %s\n", p);
	if (netfd >= 0) {
		net_putline (netfd, proto == FTP ? "bye" : "quit");
		fclose(netin);
	}
nogood:
	fclose(mailfile);
	return(retcode);
}

/*
 * Setup_open: set up an open structure to be used in opening a network
 *	connection.  Conditionally compiled (TCP flag) to set up for
 *	either a TCP or NCP open.
 */
setup_open (open_struct, hostn, socket, otimeout)
struct con *open_struct;
netaddr hostn;
int socket;
int otimeout;
{
	open_struct->c_mode = CONACT | CONTCP;
	*((long *) &(open_struct->c_con)) = *((long *)&hostn);
	open_struct->c_fport = socket;
	open_struct->c_timeo = otimeout;
}

/*
 * Execute: invoke another program and wait for it.  Args: as to execl.
 */

static  execute(name, args)
char   *name,
       *args[];
{
	int     chpid, waitres, status;
	int     fork(), wait();

	chpid = fork();
	if (chpid == -1) {	       /* could not fork */
		if (nflag)
			printf("Cant fork\n");
		return (-1);
	}
	else if (chpid == 0) {	       /* if we are the child */
		execvp(name, &args);
		if (nflag)
			printf("Exec failed\n");
		exit(-1);
	}
	else {		       /* if we are the parent */
		do
			waitres = wait(&status);
		       /* wait for death */
		while (waitres != chpid && waitres != -1);
	}
	return (status);
}

/*
 * Getline - read a line into a buffer, and replace the newline
 * with a null character.  Leave some extra room at the beginning
 * of the line buffer in case we have to prepend a character later.
 */

char *
getline (inp)
register FILE *inp;
{
	register int ch;
	register char *p;

	p = &mailbuf[2];
	while ((ch = getc (inp)) != '\n' && ch != EOF && p < &mailbuf[(sizeof mailbuf) - 3])
		*p++ = ch;

	*p = '\0';
	return (ch == EOF || ferror(inp) ? NULL : &mailbuf[2]);
}


/* -------------------------- N E T _ G E T L I N E ----------------------- */
/*
 * Read a line from the specified stream, performing timeouts and logging,
 * if requested. Return 0 on EOF or error, address of line otherwise.
 * The returned line is null-terminated, with no newline at the end.
 * Carriage returns are ignored.
 * If the last line of the stream does not end in a newline, one is appended.
 * The line is always put in the global static array 'linebuf'.
 */
char *
net_getline (infile, lflag)
register FILE *infile;
int lflag;	/* Log if on */
{
	register char  *nextout;
	register int ch;
	extern int errno;
	extern char *errmsg();

	nextout = linebuf;

	timeout(READ_TIMEOUT);
	while ((ch = getc (infile)) != '\n') {
		if (ch == EOF) {
			if (nextout > linebuf)
				*nextout++ = ch = '\n';
			break;
		}
		if (ch != '\r')
			*nextout++ = ch;
	}
	timeout(0);
	*nextout = '\0';
	if (lflag) {
		if (ch == '\n')
			printf("%s|%s\n", dest_host, linebuf);
		else {
			printf("%s returned %s",
				dest_host, feof(infile) ? "EOF" : errmsg(-1));
			if (errno == EINTR && timed_out)
				printf(" (timed out)\n");
			else
				printf("\n");
		}
	}
	if (ch == EOF)
		return(NULL);
	return(linebuf);
}

/* -------------------------- N E T _ P U T L I N E ----------------------- */
/*
 *	Write a null terminated string out to the network
 */

net_putline (fd, str)
int fd;
char   *str;
{
	register char  *s, *p;
	register int    i;
	register int    j;
	char wbuf[BUFSIZ];

	p = wbuf;
	for (s = str; *s; )
		*p++ = *s++;
	*p++ = '\r';
	*p++ = '\n';
	*p++ = '\0';
	i = p - wbuf - 1;
	timeout(i > WRITE_TIMEOUT ? i : WRITE_TIMEOUT);
	j = write (fd, wbuf, i);
	timeout(0);
	if (nflag && j < 0) {
		printf("write failed\n");
		printf("line was '%s'.\n", str);
		if (errno == EINTR && timed_out)
			printf(" (timed out)\n");
	}
	return(j);
}

/* ------------------------------- R E P L Y ------------------------------ */
/*
 *	Find the reply number from a network message.
 *	Converts the first three characters into an integer.
 *	If any are non numeric, return a zero.
 */

reply(ap)
register char   *ap;
{
	register char c;
	int i;
	register int j;

	while ((*ap < '0') || (*ap > '9'))
		if (*ap++ == '\0')
			return(0);	/* multics */

	j = 0;
	for (i = 3; i; i--) {
		c = *ap++;
		if ((c < '0') || (c > '9'))
			return(0);
		j *= 10;
		j += c;
		j -= '0';
	}
	return(j);
}

/* -------------------------- L O G I N F I R S T ------------------------- */
/*
 *	Login to a bastard host who insists on it
 *
 *	send a "user NETML\npassNETML\n";
 *	wait for the 330 230
 *	resend the mail command
 *
 *	if a transmission failure return 0
 *	if a bad reply return -1
 *	else return 1
 */

loginfirst()
{
}

/* -------------------------- S C O P Y T O ------------------------------- */
/*
 * Copy from source to destination until one of specified chars in source
 * reached. Return pointer to ending place in source.
 */
char *
scopyto(s, d, cl, max)
char *s;
char *d;
char *cl;
int max;	/* Size of destination array */
{
	char * end;
	int size;
	extern char * sfind();

	end = sfind(s, cl);
	size = end - s + 1;	/* Including null at end */
	if (size > max)
		size = max;
	scopy(s, d, d + size - 1);
	return(end);
}

/* -------------------------- H O S T _ L O O K U P ----------------------- */
/*
 * Return a pointer to a per-host information entry,
 * creating it if one doesn't already exist.
 */

#define HSHSIZE 257
struct hostinfo *host_status[HSHSIZE];

struct hostinfo *
host_lookup(hostaddr)
netaddr hostaddr;
{
	register char *cp;
	register struct hostinfo *hp;
	register int index;
	int i;

	index = hashost(hostaddr);
	for (hp = host_status[index]; hp != NULL; hp = hp->h_next)
		if (iseqhost(hp->h_addr, hostaddr))
			return (hp);
	if ((hp = (struct hostinfo *)calloc(1, sizeof(struct hostinfo))) == NULL)
		ecmderr(-1, "No room for host info.\n");

	hp->h_addr = hostaddr;
	hp->h_next = host_status[index];
	host_status[index] = hp;
	return (hp);
}

/*
 * Lowercase a string in place.
 */
char *
getlc(s)
    char *s;
{
    register char *p,
		  c;
    for (p = s; c = *p; p++)
    {
	if (c <= 'Z' && c >= 'A')
	    *p += ('a' - 'A');
    }
    return(s);
}

/*
 * Decrement the number-of-times-to-bypass count
 * for each host that is marked as being down.
 */
dec_bypass_cnt()
{
	register int i;
	register struct hostinfo *hp;

	for (i = 0; i < HSHSIZE; i++)
		for (hp = host_status[i]; hp != NULL; hp = hp->h_next)
			if (hp->h_bypass)
				hp->h_bypass--;
}
