#/*
Module Name:
	newer.c -- to see if one file is newer than another

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc newer.c
	if ! -r a.out exit
	echo testing....
	if { a.out newer.c a.out } echo err1
	if ! { a.out a.out newer.c } echo err2
	if { a.out a.out a.out } echo err3
	if { a.out /x/x/x/x a.out } echo err4
	if ! { a.out a.out /x/x/x/x } echo err5
	if { a.out newer.c newer.c a.out } err6
	if ! { a.out a.out a.out newer.c } err7
	if { a.out a.out a.out a.out } echo err8
	exit
: newer
	if ! { newer newer.c /usr/bin/newer } exit
	echo s2/newer.c:
: finale
	cc -O -s newer.c
	if ! -r a.out exit
	if ! -r /usr/sys/s2/newer.c goto same
	if { cmp -s newer.c /usr/sys/s2/newer.c } goto same
	su cp newer.c /usr/sys/s2/newer.c
	su chmod 444 /usr/sys/s2/newer.c
	rm -f newer.c
: same
	if -r /usr/bin/newer su rm -f /usr/bin/newer
	su cp a.out /usr/bin/newer
	rm -f a.out
	su chmod 555 /usr/bin/newer
	su chown bin /usr/bin/newer
	su chgrp bin /usr/bin/newer

Synopsis:
	if { newer <source>... <object> } command [arg ...]

Function:
	Primarily to see if sources are newer than the corresponding
	object modules.

	In the above synopsis, the command is executed if all of the
	sources are present and either (a) the object is missing, or
	(b) the newest source is newer than the object.

Restrictions:

See Also:

Bugs:

Module History:
	Original from Rand, dated 3 February 1978.
	Modified 29Mar79 by Greg Noel to return true if the object isn't there.
*/
main (nargs, args)
int     nargs;
char   *args[];
{
	long	buf[10];	/* to check dates between .c and .o files */

	/* if arguments bad, return false */
	if (--nargs < 2) exit(1);

	buf[9] = 0;
	while (--nargs) {
		/* if source doesn't exist, return false */
		if (stat (*++args, buf) == -1) exit(1);
		/* keep time of newest source */
		if (buf[8] > buf[9]) buf[9] = buf[8];
	}

	/* if object doesn't exist, return true */
	if (stat (*++args, buf) == -1) exit(0);

	/* both exist, return true if newer */
	exit (buf[9] > buf[8] ? 0 : 1);
}
