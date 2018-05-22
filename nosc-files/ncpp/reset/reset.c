#/*
Module Name:
	reset -- send a reset to a network site

Installation:
	as net_reset.s;mv a.out net_reset.o
	if $1e = finale goto finale
	cc reset.c net_reset.o
	if ! -r a.out exit
	rm -f reset.o net_reset.o
	exit
: finale
	cc -O -n -x reset.c net_reset.o
	if ! -r a.out exit
	su cp a.out /usr/bin/reset
	rm -f reset.o net_reset.o a.out

Synopsis:

Function:

Restrictions:

Diagnostics:

Files:

See Also:

Bugs:

Globals contained:

Routines contained:

Modules referenced:

Modules referencing:

Compile time parameters and effects:

Module History:
*/
char hostname[80] "/dev/net/";
int	buf [32];
main(argc,argv)
char *argv[];
{

	if( argc > 1 )
	{
		cpystr( argv[1],&hostname[9] );
		if (stat (hostname, buf) < 0) {
			printf ("Unknown host name\n");
			exit ();
		}
		net_reset( &hostname );
	}
	else
		printf("Usage: 'reset <hostname>'\n");
}

cpystr( srcp,destp )
char *srcp;
char *destp;
{
	register char *s;
	register char *d;

	s = srcp;
	d = destp;
	do
		*d++ = *s++;
	while( *s );
	*d = 0;			/* return a null terminated */
}
