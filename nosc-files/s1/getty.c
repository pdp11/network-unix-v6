#/*
Module Name:
	getty.c -- adapt to terminal speed on dialup, and call login

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc getty.c
	exit
: newer
	if ! { newer getty.c /etc/getty } exit
	echo s1/getty.c:
: finale
	cc -O -s -n getty.c
	if ! -r a.out exit
	if ! -r /usr/sys/s1/getty.c goto same
	if { cmp -s getty.c /usr/sys/s1/getty.c } goto same
	su cp getty.c /usr/sys/s1/getty.c
	su chmod 444 /usr/sys/s1/getty.c
	rm -f getty.c
: same
	if -r /etc/getty su rm -f /etc/getty
	su cp a.out /etc/getty
	rm -f a.out
	su chmod 555 /etc/getty
	su chown bin /etc/getty
	su chgrp bin /etc/getty

Module History:
	Originally written by someone at Bell Labs.  Intended to be modified
		locally for different types of TTY present on systems.
	Extensivly revised Nov 77 by Greg Noel to include header line, reduce
		system overhead by sending longer strings, various types of
		TTYs (Omron, DataMedia, HP2640, ARPAnet, etc.)
	Revised Sep 78 by Greg Noel to get herald text from siteinfo.h.
	Revised Nov 78 by Greg Noel for special server code.
	Revised 7May79 by Greg Noel to get information from /etc/ttys.info
		instead of it being compiled in.  Use of siteinfo.h deleted.
		Special server code now just an option in calling line.  New
		feature:  if HUPCL is set in the initial flags, we will quit
		if too much time elapses.
*/
/**/
#define NGROUPS	10	/* max lines in a cyclic group */
#define STRSIZE	512	/* size of string buffer */

#include "tty.h" /* defines HUPCL, XTABS, LCASE, ECHO, CRMOD, RAW,
				ODDP, EVENP, CEOT, CERASE, and CKILL */

#define	ANYP	0300

/*
 * Delay algorithms
 */
#define	CR1	010000
#define	CR2	020000
#define	CR3	030000
#define	NL1	000400
#define	NL2	001000
#define	NL3	001400
#define	TAB1	002000
#define	TAB2	004000
#define	TAB3	006000
#define	FF1	040000
#define	BS1	0100000

#define	SIGINT	2
#define	SIGQIT	3

struct	sgtty {
	char	sgispd, sgospd;
	char	sgerase, sgkill;
	int	sgflag;
} tmode;

struct	tab {
	int	iflags;		/* initial flags */
	int	fflags;		/* final flags */
	char	ispeed;		/* input speed */
	char	ospeed;		/* output speed */
	char	*message;	/* login message */
} itab[NGROUPS];
struct tab *maxtab { itab };

char *msgs[10];
int maxmsg;

char strings[STRSIZE];
char *maxstr { strings };
char *outloc;

char	name[16];
int	crmod;
int	upper;
int	lower;

int inbuf[259];

int uid;
char tname[2] { "0\0" };
/**/
main(argc, argv)
char **argv;
{
	register struct tab *tabp;
	register c;
	extern timeout();

/*
	signal(SIGINT, 1);
	signal(SIGQIT, 0);
*/
	uid = getuid();
	if(argc > 1) tname[0] = argv[1][0];
	if (fopen("/etc/ttys.info", inbuf) < 0)	/* open info file */
		err("can't open /etc/ttys.info");

	msgs[maxmsg++] = "\r\nLogin: \7\7";
	while (msgs[maxmsg] = getmsg()) maxmsg++;

	for (;;) {	/* search file for this tty type */
		while ((c = getc(inbuf)) == '\n');	/* drop blank lines */
		if (c < 0) err("tty type not found");
		if (c != '\t' && c != ' ' && c != '*'
			&& (argc < 2 || c == argv[1][0]))
			break;		/* found it */
		while ((c = getc(inbuf)) != '\n');	/* skip to next line */
	}
	while (crackline()) maxtab++;
	if (maxtab == itab) err("no entries for this type");
	close(*inbuf);
	if(uid) err("Evaluation completed successfully");
	signal(14, &timeout);
	for (;;) for (tabp = itab; tabp < maxtab; tabp++) {
		tmode.sgispd = tabp->ispeed;
		tmode.sgospd = tabp->ospeed;
		tmode.sgerase = CERASE;
		tmode.sgkill = CKILL;
		tmode.sgflag = tabp->iflags ^ RAW;
		stty(0, &tmode);
		outloc = maxstr; puts(tabp->message); dumpout();
		stty(0, &tmode);
		if(tabp->iflags&RAW)
			execl("/bin/login", "login", tabp->fflags, 0);
		if(getname()) {
			tmode.sgflag = tabp->fflags;
			if(crmod)
				tmode.sgflag =| CRMOD;
			if(upper)
				tmode.sgflag =| LCASE;
			if(lower)
				tmode.sgflag =& ~LCASE;
			stty(0, &tmode);
			execl(	"/bin/login",
				tabp->iflags&HUPCL
					? "-login"
					: "login",
				name,
				0);
			exit(1);
		}
	}
}

int fi;		/* holds file number of /dev/tty8 during errors */

err(msg)
char *msg;
{
	register char *p;

	for (p = msg; *p++;);  p =- msg + 1;
	if(uid) {
		write(1, msg, p);
		write(1, "\n", 1);
		exit();
	}
	if((fi = open("/dev/tty8", 1)) >= 0) {
		write(fi, "\r\ngetty(", 8);
		errn(getpid());
		write(fi, "): ", 3);
		write(fi, msg, p);
		write(fi, "\r\n", 2);
		close(fi);
	}
	close(*inbuf);
	execl("/etc/getty.bak", "-", tname, 0);
	for(;;) sleep(65535);
}

errn(nbr)
{
	int c;

	if (c = nbr/10) errn(c);
	c = nbr%10 + '0';
	write(fi, &c, 1);
}

crackline()
{
	register struct tab *tabp;
	register int c;

loop:
	while((c = getc(inbuf)) == '\n');
	if(c == '*') {
		while ((c = getc(inbuf)) != '\n');
		goto loop;
	}
	if(c != '\t' && c != ' ') return 0;	/* new cyclic group */
	tabp = maxtab;
	if(getfield()) err("input speed missing or invalid");
	tabp->ispeed = getspeed();
	if(getfield()) err("output speed missing or invalid");
	tabp->ospeed = getspeed();
	if(getfield()) err("initial flags not present");
	tabp->iflags = getflags() & ~RAW;
	if(getfield()) err("final flags not present");
	if(!getlogin()) tabp->fflags = getflags();
	if((tabp->message = getmsg()) == 0)
		err("login message not present");
	return 1;
}

getfield()
{
	register char *p;
	register int c;

	while ((c = getc(inbuf)) == '\t' || c == ' ');	/* skip whitespace */

	if (c == '\n' || c < 0) return 1;	/* field not present */

	p = maxstr;		/* get pointer to scratch area */
	do {
		*p++ = c;	/* copy field to scratch area */
		if ((c = getc(inbuf)) == '\n' || c < 0) return -1;
	} while c != '\t' && c != ' ';
	*p = '\0';		/* null termintate */
	return 0;
}

int speedtab[] { 50, 75, 110, 134, 150, 200, 300, 600, 1200,
		1800, 2400, 4800, 9600 };

getspeed()
{
	register int n, c;
	register char *p;

	for (p = maxstr, n = 0; c = *p++&0377;) {
		if (c < '0' || c > '9')
			err("illegal character in speed field");
		n = n*10 + (c - '0');
	}
	if (n < 50) return n;
	for (c = 0; c < sizeof speedtab/sizeof speedtab[0];)
		if (speedtab[c++] == n)
			return c;
	err("illegal speed specified");
}

getlogin()
{
	register char *p, *q;

	for (p = "login=\377", q = maxstr; *p++ == *q++;);
	if (*p) return 0;
	for (p = maxstr, q--; *p++ = *q++;);
	maxtab->iflags =| RAW;
	maxtab->fflags = maxstr;
	maxstr = p;
	return 1;
}

struct flgtab {
	char *type;
	int flag;
} flgtab[] {
	"cr1",	CR1,		"cr2",	CR2,		"cr3",	CR3,
	"nl1",	NL1,		"nl2",	NL2,		"nl3",	NL3,
	"tab1",	TAB1,		"tab2",	TAB2,		"tab3",	TAB3,
	"ff1",	FF1,		"bs1",	BS1,		"raw",	RAW,
	"hup",	HUPCL,		"tab",	XTABS,		"lc",	LCASE,
	"echo",	ECHO,		"lf",	CRMOD,		"null",	0,
	"odd",	ODDP,		"even",	EVENP,		"any",	ODDP|EVENP,
	0
};

getflags()
{
	register struct flgtab *flgp;
	register char *p, *q;
	char *pp;
	int flags;

	pp = maxstr;
	flags = 0;
	for(;;) {
		for (flgp = flgtab; q = flgp->type; flgp++)
			for(p = pp; *p++ == *q++;)
				if (!*q) goto found;
	flgerr:
		err("illegal flag specified");
	found:
		if(*p != '+' && *p != '\0') goto flgerr;
		pp = p + 1;
		flags =| flgp->flag;
		if (*p == '\0') return flags;
	}
}

getmsg()
{
	register int c;
	register char *p, *a;

	while ((c = getc(inbuf)) == '\t' || c == ' ');	/* skip spaces */
	if (c == '\n') return 0;	/* null line */
	p = a = maxstr;			/* pick up first free location */
	do	*p++ = c;		/* copy message into text buffer */
	while (c = getc(inbuf)) != '\n';
	*p++ = '\0';			/* null terminate */
	maxstr = p;			/* save updated location */
	return a;
}

getname()
{
	register char *np;
	register c;
	static cs;

	crmod = 0;
	upper = 0;
	lower = 0;
	np = name;
	if (tmode.sgflag&HUPCL) alarm(60);
	do {
		if (read(0, &cs, 1) <= 0)
			exit(0);
		if ((c = cs&0177) == 0)
			return(0);
		if (c==CEOT) exit();
		write(1, &cs, 1);
		if (c>='a' && c <='z')
			lower++;
		else if (c>='A' && c<='Z') {
			upper++;
			c =+ 'a'-'A';
		} else if (c==CERASE) {
			if (np > name)
				np--;
			continue;
		} else if (c==CKILL) {
			np = name;
			continue;
		}
		*np++ = c;
	} while (c!='\n' && c!='\r' && np <= &name[16]);
	alarm(0);
	*--np = 0;
	if (c == '\r') {
		write(1, "\n", 1);
		crmod++;
	} else
		write(1, "\r", 1);
	return(1);
}

timeout()
{
	write(1, "\r\n\7\7\7Terminated due to inactivity\r\n", 35);
	sleep(10);
	exit();
}

puts(as)
char *as;
{
	register char *s;
	register int ss, n;

	for (s = as; ss = *s++&0377;) {
		if (ss != '\\') put(ss);
		else switch (ss = *s++&0377) {

		default:	put(ss);	break;

		case 'n':	put('\n');	break;

		case 'r':	put('\r');	break;

		case 'b':	put('\b');	break;

		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			for (n = ss - '0'; (ss = *s++&0377) >= '0' &&
					ss <= '7';)
				n = n*8 + (ss - '0');
			put(n);
			--s;
			break;

		case 'm':
			puts(((n = (*s++&0377) - '0') < 0 || n >= maxmsg)
				? "<<< error: invalid message number >>>"
				: msgs[n]);
			break;

		case 's':
			if ((n = (*s++&0377) - '0') < 0 || n > 9)
				puts("<<< error: invalid sleep code >>>");
			else {
				dumpout();
				sleep(n);
			}
		}
	}
}

put(c)
{
	if (outloc >= &strings[sizeof strings])
		dumpout();
	*outloc++ = c;
}

dumpout()
{
	write(1, maxstr, outloc-maxstr);
	outloc = maxstr;
}
