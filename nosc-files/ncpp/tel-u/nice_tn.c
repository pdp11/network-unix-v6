#/*
Module Name:
	nice_tn.c

Installation:
	cc -O -s nice_tn.c
	su cp a.out /etc/bin/nice_tn
	rm a.out

Function:
	Establish an ARPAnet connection with the nice value set to a very
	low priority so there is little load on the system.  Primarily
	useful for use as a default shell.

Compile time parameters and effects:
	none.

Module History:
	Written 14 Dec 77 by Greg Noel
*/
main(argc, argv)
int argc;
char **argv;
{
	nice(127);	/* set priority way down */
	execl("/usr/bin/tn", "-Telnet", 0);	/* make connection */
}
