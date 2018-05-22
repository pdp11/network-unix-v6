#/*
Module Name:
	host.c -- make a connection to a NALCON host

Installation:
	if $1x = finalx goto finale
	cc host.c
	if ! -r a.out exit
	if -r host rm -f host
	mv a.out host
	chmod 4555 host
	su chown root host
	exit
: finale
	cc -O host.c
	if ! -r a.out exit
	if -r host rm -f host
	su cp a.out /usr/bin/host
	rm -f a.out

Synopsis:
	host

Function:

Restrictions:

Diagnostics:

Files:
	/dev/host[1-4]

See Also:

Bugs:

Compile time parameters and effects:

Module History:
	Started July, 78 by Ron Broersma, worked on off and on until
		October, 78.
	Able assistance by Greg Noel.
	Modified 19 Jan 78 Ron Broersma.  Got rid of one process. Removed
		direct associations to type of host so that it is transferrable
		to other types of hosts.
*/
#define ever ;;
#define B50 1
#define B75 2
#define B110 3
#define B150 5
#define B300 7
#define B1200 9
#define B1800 10
#define B2400 11
#define B4800 12
#define B9600 13
#define BextA 14
#define BextB 15
#define ANYP 0300
#define EVENP 0200
#define RAW   040
#define ECHO 010
#define HUPCL 1
#define EOT 04
#define ESC 033

#include "/usr/sys/ncpp/host/host.h"
struct sgtty {
	char sgispd, sgospd;
	char sgerase, sgkill;
	int sgflag;
} tmode;

struct userid {
	char real, effective;
};

int fi, uid;
int pid, pidf, save, term;
int stat;
int out();
#ifdef DRAIN
int drain();
#endif DRAIN
char host[] "/dev/hostx";
char line[] "1234";

main(argc, argv)
char *argv[];
{
	register char *lineno;

	for(lineno = line; host[9] = *lineno++;)
		if((fi = open(host,2)) >= 0)
			goto found;
	printf("No %s lines available.\n",hostname);
	exit();

found:
	uid = getuid();
	chown(host, uid.real);
	chmod(host, 0);
	tmode.sgospd = HOSTOSPD;
	tmode.sgispd = HOSTISPD;
	tmode.sgflag = EVENP + RAW + HUPCL;
	stty(fi, &tmode);
#ifdef DRAIN
	signal(1, drain);
#endif DRAIN
	SETUP
	printf("%s Port %d...", hostname, lineno - line);
	gtty(0, &tmode);
	save = tmode.sgflag;
	tmode.sgflag = ANYP + RAW + ECHO + HUPCL;
	if (tmode.sgispd == 4) tmode.sgflag =& ~ECHO;
	stty(0, &tmode);
	term = 0;
	pid = getpid();
	if ((pidf = fork()) == 0) {
		fromhost();
		restore();
		printf("Connection terminated by host.\n");
		kill(pid, 2);
		exit();
	}
#ifdef DRAIN
	signal(1, 0);
#endif DRAIN
	signal(2, out);
	tohost(&stat);
	kill(pidf, 9);
	if (stat != 2) save =| HUPCL;
	restore();
	printf("%s Port %d closed\n",hostname, lineno - line);
	if (stat == 2) execl("/bin/login", "login", 0);
	exit();
}

#ifdef DRAIN
drain()
{
	char c;
	int rd;

	do {
		alarm(15);
		rd = read(fi, &c, 1);
	} while (rd>0);
	alarm(0);
	restore();
	exit();
}
#endif DRAIN

out()
{
	term = 1;
}

restore()
{
	gtty(0, &tmode);
	tmode.sgflag = save;
	if (tmode.sgispd == 4) tmode.sgflag =& ~ECHO;
	stty(0, &tmode);
	chmod(host, 0600);
	chown(host, 0/*root*/);
	close(fi);
}
tohost(st)
int *st;
{
	int co, pco;

	co=0;
	do {
		pco=co;
		co = getchar();
		write (fi, &co, 1);
	}  while ((pco != co ||  (pco != EOT && pco !=  ESC)) && term == 0);
	if (co == ESC) {
		*st = 2;
		return(0);
	}
	*st = 0;
}
fromhost()
{
	int rd;
	char c;

	do {
		rd = read (fi, &c, 1);
		if (rd > 0) putchar (c);
	} while (rd > 0);
}
