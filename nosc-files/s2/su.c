/*
Module Name:
	su.c -- the su command -- execute a command as the super-user

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc su.c
	if ! -r a.out exit
	if -r su.new rm -f su.new makeme.new
	mv a.out su.new
	ln su.new makeme.new
	chmod 4555 su.new
	su chown root su.new
	exit
: newer
	if ! { newer su.c /bin/su } exit
	echo s2/su.c:
: finale
	cc -O -s su.c
	if ! -r a.out exit
	if -r su.new rm -f su.new makeme.new
	if ! -r /usr/sys/s2/su.c goto same
	if { cmp -s su.c /usr/sys/s2/su.c } goto same
	su cp su.c /usr/sys/s2/su.c
	rm -f su.c
: same
	/bin/su sh -
	rm -f /bin/su /usr/bin/makeme
	cp a.out /bin/su
	rm -f a.out
	chmod 4555 /bin/su
	chown root /bin/su
	chgrp bin /bin/su
	ln /bin/su /usr/bin/makeme

Synopsis:
	su [command [arguments]]
	-- or --
	makeme <name> [command [arguments]]

Description:
	The UID of the current real user is compared against a list of
	"trusted" users; if the user is NOT on this list, an attempt is
	made to get the password of the root from the standard input.
	If the standard input is not a tty, or if the password is not
	given correctly, the command is aborted.  Otherwise, command
	(including any arguments) is executed with the UID set to the
	super-user for the su command or to the specified user for the
	makeme version.  If no command is given, the shell is assumed,
	making its actions identical to the original su command.

	The list of "trusted" users are those users authorized to use
	group zero (the "system" group) in /etc/group.

Bugs:
	This program also doesn't know about the Yale shell's user bin
	directories, although it's not obvious which bin directory it
	should search, if any.

Module History:
	Creation Nov 1977 by Greg Noel, based upon the original su plus nohup
	Revised 6 Dec 77 by Greg Noel to execute shell files correctly
	Revised 26 June 78 by Greg Noel to add makeme variation.  Also changed
	    all useages of printf to write.
	Revised 17 April 79 by Greg Noel to use the system group (GID zero) as
	    the list of authorized users.
*/
char	string[100] {"/usr/bin/"};
char	password[100];
char	pwbuf[100];

main(argc, argv)
int argc;
char *argv[];
{
	register char *strp, *p;
	register i;

	ckpw();		/* go check authorization */
	setuid(0);	/* become the super-user */
	if(str_comp(argv[0], "makeme") == 0) {
		if(argc < 2  ||  !makeme(*++argv)) oops("Makeme who?\n");
		argc--;
	}
	for(i = 3; i < 15; i++) close(i);
	if(argc < 2) {	/* if no command, execute the shell */
		execl("/bin/sh", "-su", 0);
		oops("Couldn't get shell!\n");
	}
	argv[argc] = 0;	/* set end-of-list marker correctly for execv */
	argv++;
	execute(argv[0], argv);	/* try current directory */
	strp = &string[9];
	p = *argv;
	while(*strp++ = *p++);	/* make up potential system program names */
	execute(string+4, argv);	/* try /bin directory */
	execute(string, argv);	/* try /usr/bin directory */
	for(p = argv[0]; *p;) write(1, p++, 1);
	oops(": not found\n");
}

ckpw()
{
	register char *p, *q;
	int ttybuf[4];

	if((q = getuid()&0377) == 0) return;	/* super-user is authorized! */
	if(!getpw(q, pwbuf) && is_sys(pwbuf))
		return;		/* in authorized list */

		/* not in authorized list, see if they know password */

	if(getpw(0, pwbuf))
		goto badpw;
	p = pwbuf;
	while(*p != ':')
		if(*p++ == '\0')
			goto badpw;
	if(*++p == ':')	/* null password -- let anybody in */
		return;
	if(gtty(0, ttybuf) < 0) {	/* input must be from tty */
		oops("Sorry -- need TTY for password\n");
	}
	ttybuf[4] = ttybuf[2];
	ttybuf[2] =& ~010;
	stty(0, ttybuf);
	write(1, "Password: ", 10);
	q = password;
	while((*q = getchar()) != '\n')
		if(*q++ == '\0')
			exit();
	*q = '\0';
	ttybuf[2] = ttybuf[4];
	stty(0, ttybuf);
	write(1, "\n", 1);
	q = crypt(password);
	while(*q++ == *p++);
	if(*--q == '\0' && *--p == ':')
		return;

		/* Access not permitted, tell them and quit */
	oops("Sorry\n");

badpw:
	oops("Bad password file\n");
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
			if(*--p == ':' && (c == ',' || c == '\n')) {
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

makeme(name)
char *name;
{
	register uid;

	if(*name >= '0'  &&  *name <= '9')
		uid = atoi(name);
	else
	if((uid = getnam(name, pwbuf)) < 0)
		return 0;	/* failure */
	setuid(uid);
	return 1;
}

execute(name, parms)
char *name, **parms;
{
	extern int errno;
	register char *n, **p;

	n = name;  p = parms;
	execv(n, p);
	if(errno==8/*ENOEXEC*/) {
		*p = n;
		*--p = "-su";
		execv("/bin/sh", p);
		oops("Can't get Shell!\n");
	}
}

str_comp(a, b)
char *a, *b;
{
	register char  *p, *q, c;

	p = a; q = b;	/* for speed and space */
	while((c = *p++) == *q++ && c);
	return *--p - *--q;
}

oops(msg)
char *msg;
{
	register char *p;

	for(p = msg; *p++;);
	write(1, msg, p-msg-1);
	exit();
}

/*  This really should be a system utility, located in /lib/libc.a  */

getnam(name, buf)
char *name;
char buf[];
{
	auto pbuf[259];
	static pwf;
	register char *n;
	register c;
	register char *bp;

	if(pwf == 0)
		pwf = open("/etc/passwd", 0);
	if(pwf < 0)
		return(-1);
	seek(pwf, 0, 0);
	pbuf[0] = pwf;
	pbuf[1] = 0;
	pbuf[2] = 0;

	for (;;) {
		bp = buf;
		while((c=getc(pbuf)) != '\n') {
			if(c <= 0)
				return(-1);
			*bp++ = c;
		}
		*bp++ = '\0';
		bp = buf;
		n = name;
		while(*n++ == *bp++);
		if(*--n != '\0'  ||  bp[-1] != ':')
			continue;
		while(*bp++ != ':');
		n = 0;
		while((c = *bp++) != ':') {
			if(c<'0' || c>'9')
				continue;
			n = n*10+c-'0';
		}
		return(n);
	}
	return(-1);
}
