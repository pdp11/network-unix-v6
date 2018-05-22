#/*
Module Name:
	start_up.c -- default login startup routine

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc start_up.c
	exit
: newer
	if ! { newer start_up.c /etc/start_up } exit
	echo s1/login/start_up.c:
: finale
	cc -O -s start_up.c
	if ! -r a.out exit
	if ! -r /usr/sys/s1/login/start_up.c goto same
	if { cmp -s start_up.c /usr/sys/s1/login/start_up.c } goto same
	su cp start_up.c /usr/sys/s1/login/start_up.c
	su chmod 444 /usr/sys/s1/login/start_up.c
	rm -f start_up.c
: same
	su rm -f /etc/start_up
	su cp a.out /etc/start_up
	rm -f a.out
	su chmod 555 /etc/start_up
	su chown bin /etc/start_up
	su chgrp bin /etc/start_up

Module History:
	Written 7Dec77 by Greg Noel, revised same day.  (Original was much
		more complex because it still had the mail check logic.)
	Revised 31Jan78 by Greg Noel to do copy ourselves instead of an
		execl -- much faster.
	Revised 12Apr79 by Greg Noel to not print /etc/motd if .llog is
		newer (that is, if it hasn't changed since last login).
*/
long int buf[128];	/* 512-byte word-aligned buffer */

main(argc, argv)
int argc;
char **argv;
{
	register int cnt, fi;

	if( (fi = open("/etc/motd", 0)) >= 0) {
		fstat(fi, &buf[1]);
		if(stat(".llog", &buf[0]) == -1 || buf[9] > buf[8])
			while( (cnt = read(fi, buf, 512)) > 0)
				write(1, buf, cnt);
		close(fi);
	}
	signal(1, 0);
	execl("/usr/bin/today", "today", 0);
}
