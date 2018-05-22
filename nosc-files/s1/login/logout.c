#/*
Module Name:
	logout -- disconnect the user from the system

Installation:
	cc -O -s logout.c
	if ! -r a.out exit
	su cp a.out /usr/bin/logout
	rm a.out

Synopsis:
	logout

Function:
	Disconnect the current user from the system by simulating a hangup.

Restrictions:

Diagnostics:
	none.

Files:
	none.

See Also:
	sh (I) for ctl-D function.

Bugs:
	In a program this size?

Compile time parameters and effects:
	none.

Module History:
	Written 23Nov77 by Greg Noel
	Modified 9Mar78 by Greg Noel to force hangup on dialed lines.
	Modified 20Jun79 by Ken Harrenstien to print goodbye msg.
*/
main()
{
	int buf[3];

	printf("\nGoodbye.\n");		/* Print farewell */
	sleep(5);		/* Wait long enough for msg to appear */
	gtty(2, buf); buf[0] = 0; stty(2, buf);	/* hang up dialed line */
	kill(0, 1);		/* send a hangup to the process group */
}
