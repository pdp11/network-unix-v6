#/*
Module Name:
	login.c -- login [ name ] -- log a user into the system

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc login.c
	exit
: newer
	if ! { newer login.c /bin/login } exit
	echo s1/login/login.c:
: finale
	cc -O -s login.c
	if ! -r a.out exit
	if ! -r /usr/sys/s1/login/login.c goto same
	if { cmp -s login.c /usr/sys/s1/login/login.c } goto same
	su cp login.c /usr/sys/s1/login/login.c
	su chmod 444 /usr/sys/s1/login/login.c
	rm -f login.c
: same
	su rm -f /bin/login
	su cp a.out /bin/login
	rm -f a.out
	su chmod 4555 /bin/login
	su chown root /bin/login
	su chgrp bin /bin/login

Module History:
	Originally written by someone at Bell Labs.
	Modified 7 Dec 77 by Greg Noel to execute start_up in the user's
		directory, otherwise execute a default.  Mail check also
		deleted here and moved to shell.
	Modified 8 Dec 77 by Greg Noel to ask for password if name illegal
		(security provision).
	Modified 23 May 78 by Greg Noel to handle majuscle tty identifiers.
	Modified 12 Dec 78 by Greg Noel to avoid extra stty if username is
		provided as parameter.  (Corrects problem with type-ahead.)
	Merged with DoD distribution code 12 Apr 79 by Greg Noel.  DoD update
		information reproduced below.  Changes:
		.  The check for new mail remains in the shell.
		.  The test for .llog newer than /etc/motd goes into
		   /etc/start_up.
		.  /etc/start_up also executes the 'today' program.
		.  Some code was reorganized to provide more security.
		.  Only the super-user can log on when SHUTSW is set.
		.  The user's bin directory is now searched for the
		   start_up file before the login directory.
	Modified 17 Apr 79 by Greg Noel to permit anyone in the "system"
		group (GID zero) to login when SHUTSW is set.
*/
/*
 *      1.2 14-Sep-76  Dennis L. Mumaugh
 *      Test to see if there are any mail messages since the last
 *      time the user read his mail. Checks for a file .msgtime
 *      in login directory. If the modtime of the .mail file is
 *      later than the modtime of the .msgtime file the there
 *      is new mail (almost always).
 *
 *      1.3 14-Jan-77  Dennis L. Mumaugh
 *      Test file .llog to see if /etc/motd has changed since
 *      last login -- if not don't bother to print it.
 *      Prints time for previous login.
 *
 *      1.4 15-Feb-77  Dennis L. Mumaugh
 *      If /shutsw exist and not a local terminal refuse login.
 *
 *      1.5 4-May-77   Dennis L. Mumaugh
 *      Ask for password even if the login name is bad.
 *
 *      1.6 24-June-77  Dennis L. Mumaugh
 *      Restrict logins by user vice terminal
 *      Shut off logins to remote terminals when /shutsw exists
 *      Default .profile is run if exists
 *
 *      1.7 09-Nov-77  Dennis L. Mumaugh
 *      Use information provided by ksys if possible
 *      and give warning of impending shutdown (doom)
 *
 *      1.8 10-Mar-78  Dennis L. Mumaugh
 *      Fix path name of today
 */
/*
 *      for the restricted login feature to be implemented one must define
 *      a define of RESTRICT in login.h
 */
#include "login.h"
#include "tty.h"  /* defines ECHO, CERASE, and CKILL */
extern char partab[];
char id[] "~|^` login:  1.8+    10-Mar-77";

long tvec;

struct {
	char	name[8];
	char	utty;
	char	ifill;
	int	time[2];
	int	ufill;
} utmp;

struct {
	int	speeds;
	char	*canons;
	int	tflags;
} ttyb;

#define CANONS	((CKILL<<8)|CERASE)	/* default erase-kill */

char	*ttyx;

struct ksys ksys;
char tsmsg1[] "\nTime sharing will cease in %d minutes\n";

main(argc, argv)
char **argv;
{
	int cty;
	char pbuf[128];
	register char *namep, *np;
	char pwbuf[9];
	int t, sflags, f, c, uid, gid;
	int count;
	int ttime; /* time until ksys occurs; used by ksys code section */
	struct { long LONG; };
	int timeo; extern timeout();

	signal(3, 1);
	signal(2, 1);
	nice(2);
	ttyx = "/dev/ttyx";
	if ((utmp.utty=ttyn(0)) == 'x') {
		write(1, "Sorry.\n", 7);
		exit();
	}
	ttyx[8] = utmp.utty;
	gtty(0, &ttyb);
	if(ttyb.canons != CANONS) {
		ttyb.canons = CANONS;
		stty(0, &ttyb);
	}
	timeo = 0;
	if(ttyb.tflags&HUPCL || (argc > 0 && **argv == '-')) {
		timeo++;
		signal(14, &timeout);
	}
	count = 0;
loop:
	if ( count++ > 6 ) goto trouble;
	namep = utmp.name;
	if (argc>1) {
		np = argv[1];
		while (namep<utmp.name+8 && *np && (*np != ' ') )
			*namep++ = *np++;
		argc = 0;
	}
	while (namep == utmp.name) {
		write(1, "Name: ", 7);
		if (timeo) alarm(60);
		while ((c = getchar()) != '\n') {
			if (c <= 0)
				exit();
			if (namep < utmp.name+8 && c != ' ' )
				*namep++ = c;
		}
		alarm(0);
	}

	while (namep < utmp.name+8)
		*namep++ = ' ';
	if (getpwentry(utmp.name, pbuf))
		np = "junk:";	/* doesn't match any possible password */
	else
		np = colon(pbuf);
	if (*np!=':') {
		sflags = ttyb.tflags;
		ttyb.tflags =& ~ ECHO;
		stty(0, &ttyb);
		write(1, "Password: ", 10);
		namep = pwbuf;
		if (timeo) alarm(60);
		while ((c=getchar()) != '\n') {
			if (c <= 0)
				exit();
			if (namep<pwbuf+8)
				*namep++ = c;
		}
		alarm(0);
		*namep++ = '\0';
		ttyb.tflags = sflags;
		stty(0, &ttyb);
		write(1, "\n", 1);
		namep = crypt(pwbuf);
		while (*namep++ == *np++);
		if (*--namep!='\0' || *--np!=':')
			goto bad;
	}
	np = colon(np);
	uid = 0;
	while (*np != ':')
		uid = uid*10 + *np++ - '0';
	np++;
	gid = 0;
	while (*np != ':')
		gid = gid*10 + *np++ - '0';
	np++;
	np = colon(np);
	namep = np;
	np = colon(np);

#ifdef RESTRICT
/*
 *      restricted logins
 */
	if( !(ttybits(utmp.tty) & userbits(uid)) ) {
		write(1, "Permission Denied.\n",19);
		sleep(5);
		exit();
	}
/*
 *      local installation definition of "local" terminals
 */
#define CONSOLE 01
#define LOCAL   02
#define SYSTEM  04
       /*  rlg mods to refuse logins during shutdown */
       if ( (f = open(SHUTSW,0) ) >= 0) {
                close(f);
		if( ttybits((utmp.tty))  & (CONSOLE|LOCAL|SYSTEM))
			goto contin;
		write( 1, "No remote users allowed!\n",25);
		sleep(5);
		exit();
	contin: ;
       }
#endif RESTRICT

/*
 *      check to see if a ksys is pending and announce to
 *      potential users this fact
 */
	time(utmp.time);
	f = open(KSYST,0);
	if( f > 0) {
		read(f,&ksys, sizeof ksys);
		close(f);
		ttime = ksys.dtime - utmp.time[0].LONG;
		printf(tsmsg1,ttime/60);
	}
	if (print(SHUTSW) && uid != 0 && gid != 0 && !is_sys(pbuf)) {
		write(1, "Logins not permitted at this time.\n\n", 36);
		sleep(10);
		exit();
	}
	if (chdir(namep)<0) {
		write(1, "No directory\n", 13);
		goto loop;
	}
	if ((f = open("/etc/utmp", 1)) >= 0) {
		t = utmp.utty;
		if(t>='A' && t<='Z')
			t =+ 'z' - 'A' + 1;
		if (t>='a')
			t =- 'a' - (10+'0');
		seek(f, (t-'0')*16, 0);
		write(f, &utmp, 16);
		close(f);
	}
	if ((f = open("/usr/adm/wtmp", 1)) >= 0) {
		seek(f, 0, 2);
		write(f, &utmp, 16);
		close(f);
	}
	chown(ttyx, uid);
	setgid(gid);
	setuid(uid);
	if ((t = fork()) == 0) {
		/* child */
		start_up("bin/start_up");
		start_up("start_up");
		start_up("/etc/start_up");
		print("/etc/motd");	/* last-ditch try */
		exit();
	} else if (t == -1) {
		printf("Sorry, not enough resources, try again later/n");
		exit();
	}
	while (wait(&f) != t);	/* wait until start_up complete */

	/* create and close .llog to record login time */
	close (creat (".llog", 0644));

	if (*np == '\0')
/*
 *      if the file .profile  exists run the profile shell first
 */
		if ( (f = open(".profile",0)) <0 ) np = "/bin/dnsh";
		else {close(f); np = "/bin/prfsh";};
	execl(np, "-Shell", 0);
	write(1, "No shell.\n", 9);
	exit();
bad:
	write(1, "Login incorrect.\n", 17);
	goto loop;
trouble:
	time(&tvec);
	cty = open("/dev/tty8",1);
	write(cty, "\r\n\07", 3);
	write(cty, ctime(&tvec)+4, 16);
	write(cty,"\r\nSomebody is eating my porridge!\r\n\07",36);
	write(cty,"Unsuccessful login on tty",25);
	write(cty,&utmp.utty,1);
	write(cty,"\r\n",2);
	close(cty);
	write(1, "Naughty, naughty, mustn't do.\n",30);
	write(1, "System will punish you.\07\n",27);
	count = 10 ;
	sleep(10);
	while ( count-- ) {
		sleep(3);
		write (1, "\33H\33J\014\07",6); /* Clear screen ; Bell */
	}
	write (1, "\06",1);     /* code to turn SuperBee off-line */
	exit(99);
}

timeout()
{
	write(1, "\n\7\7\7Terminated due to inactivity\n", 33);
	sleep(10);
	exit();
}

getpwentry(name, buf)
char *name, *buf;
{
	extern fin;
	int fi, r, c;
	register char *gnp, *rnp;

	fi = fin;
	r = 1;
	if((fin = open(PASSWORD, 0)) < 0)
		goto ret;
loop:
	gnp = name;
	rnp = buf;
	while((c=getchar()) != '\n') {
		if(c <= 0)
			goto ret;
		*rnp++ = c;
	}
	*rnp++ = '\0';
	rnp = buf;
	while (*gnp++ == *rnp++);
	if ((*--gnp!=' ' && gnp<name+8) || *--rnp!=':')
		goto loop;
	r = 0;
ret:
	close(fin);
	fin = 0;
	(&fin)[1] = 0;
	(&fin)[2] = 0;
	return(r);
}

colon(p)
char *p;
{
	register char *rp;

	rp = p;
	while (*rp != ':') {
		if (*rp++ == '\0') {
			write(1, "Bad password file format\n", 25);
			exit();
		}
	}
	*rp++ = '\0';
	return(rp);
}

print(name)
char *name;
{
	register f, i;
	int prbuf[32];

	if ((f = open(name, 0)) < 0) return 0;
	while ((i = read(f, prbuf, sizeof prbuf)) > 0)
		write (1, prbuf, i);
	close (f);
	return 1;
}

is_sys(name)
char *name;
{
	register int c;
	register char *p;
	int _file_[259];

	if(fopen("/etc/group", _file_) < 0) return 0;

	for(;;) {
		if((c = getc(_file_)) < 0) {
	nope:		close(*_file_);
			return 0;
		}
		while(c != ':') c = getc(_file_);
		while((c = getc(_file_)) != ':');
		if(getc(_file_) == '0' && getc(_file_) == ':') for(;;) {
			for(p = name; (c = getc(_file_)) == *p++;);
			if(*--p == '\0' && (c == ',' || c == '\n')) {
				close(*_file_);
				return 1;
			}
			while(c != ',') {
				if(c == '\n') goto nope;
				c = getc(_file_);
			}
		}
		while(getc(_file_) != '\n');
	}
}

int errno;		/* error code returned from various system calls */
#define ENOEXEC errno==8	/* marked executable, but not execute format */

start_up(name)
char *name;
{
	execl(name, "Start_up", 0);
	if (ENOEXEC)
		execl("/bin/sh", "Start_up", name, 0);
}

#ifdef RESTRICT
/*
 *      testaccess -- routines to determine of a given user is
 *              should be given access to the tty specified.
 */

#define ttyfile "/etc/ttys1"
#define aclist  "/etc/access_list"

/*
 *      ttybits -- get tty access bits
 *      assumes a file  ttyfile of 4 bytes per entry (see tlist for
 *      format); The bits specified are the same as in those of
 *      react;  The subroutine reads the file until it finds
 *      a tty name that matches and returns the bits
 */
ttybits(tty)
char tty; {
	int mode;
	int fd;
	struct tlist{
		int bits;
		char ttyname;
		char nl;} ttylst;

	fd = open(ttyfile,0);
	if( fd < 0 ) return(-1);
	while( read(fd,&ttylst, sizeof ttylst) > 0)
	if (ttylst.ttyname == tty ) goto out;
	close(fd);
	return(-1);    /* not aknown tty */
out:    ;
	close(fd);
	return(ttylst.bits);
}

/*
 *      userbits -- read access list for user bits
 *      assumes a file access-list of 1 word per entry
 *      The bits specified are the same as in those of
 *      react;  The subroutine reads the access list
 *      file the nth word of that file is the permissions for the
 *      user specified.
 */
userbits(uid){
	int mode;
	int fd;

	if( (fd = open(aclist,0) ) <0 ) return(-1);
	seek(fd,(uid&0377)*2,0);
	if( read(fd,&mode,2) < 0 ) { close(fd); return(-1);}
	close(fd);
	return(mode);
}
#endif RESTRICT
