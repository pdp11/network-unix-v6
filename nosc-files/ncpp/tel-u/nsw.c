#/*
Module Name:
	nsw.c

Installation:
	cc -O -s nsw.c
	su cp a.out /usr/bin/nsw
	rm a.out

Function:
	Set the priority to very low to reduce the load on the system, and
	connect to the NSW front end.  If a front-end host is specified,
	that one is used, otherwise the default NSW host is used.

Globals contained:
	none

Routines contained:
	main

Modules referenced:
	none

Modules referencing:
	none

Compile time parameters and effects:
	none

Module History:
	Written Nov 77 by Greg Noel
	Modified 14 Dec 77 by Greg Noel to permit host specification
*/
main(argc, argv)
int argc;
char **argv;
{
	nice(127);	/* set priority way down */
	execl("/usr/bin/telnet", "-NSW",
		argc>1 ? argv[1] : "nsw", "-s", "033", 0);
}
