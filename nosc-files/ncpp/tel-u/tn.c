#/*
Module Name:
	tn -- simple telnet connection

Installation:
	if $1x = finalx goto final
	cc -c tn.c
	if ! -r tn.o exit
	cc tn.o /usr/net/hnconv.c
	if ! -r a.out exit
	rm tn.o hnconv.o
	chmod 755 a.out
	mv a.out tn
	exit
: final
	cc -O -c tn.c
	if ! -r tn.o exit
	cc -O -s -n tn.o /usr/net/hnconv.c
	if ! -r a.out exit
	su cp a.out /usr/bin/tn
	rm a.out tn.o hnconv.o
	if -r tn rm tn

Synopsis:
	tn [ <options> ] [ <host> [ <socket> ] ]

Function:
	Makes a Telnet connection to the specified host on the specified
	socket.  If a host is not specified, or if it is not recoginzed,
	the user is prompted for a valid name.  If a socket is not given
	the standard socket is assumed.  Only a single control character
	is recognized: the FS character (usually control-backslant) zaps
	the connection and exits.  The only option currently recognized
	is '-e' which will cause local echoing.

Restrictions:
	Only works with Old Telnet.  The next modification should change
	it to use New Telnet instead.

Diagnostics:
	"Can't connect to host" -- the open attempt failed.

Files:
	/dev/net/anyhost

See Also:
	telnet(I)

Bugs:
	None known.

Compile time parameters and effects:

Module History:
	Original by Geoff Goodfellow, date unknown.
	Several modifications made 3May78 by Greg Noel:
		Added timeout so passive host doesn't bother it.
		A remote disconnect causes us to go away.
		Status attempted if open fails.
		Retrys on invalid host name.
*/
#include "/usr/sys/h/net/open.h"
#define SIGINR 15
#define BUFSIZ 100

int fk, ttyflags[3], oldflags;

main(argc,argv)
int argc;
char *argv[];
{	int nf;
	int hnum;
	int echoflg;
	char buf[BUFSIZ+BUFSIZ];
	char c;
	register int i, j, k;
	int endit();

	while (argc > 1  &&  argv[1][0] == '-') {
		argc--;  argv++;
		switch (argv[0][1]) {

		case 'e':
			echoflg++;
			break;

		default:
			printf("Illegal option '%s' ignored\n", argv[0]);
		}
	}
	if (argc > 1)
		hnum = atoi(hnconv(j = argv[1]));
	while(hnum == 0)
	{	printf("Host: ");
		if ((i = read(0, buf, BUFSIZ)) <= 0) exit();
		buf[i-1] = '\0';
		hnum = atoi(hnconv(j = buf));
	}
	openparam.o_host = hnum;

	if (argc > 2)
		openparam.o_fskt[1] = atoi(argv[2]);

	openparam.o_timo = 8;
	nf = open("/dev/net/anyhost",&openparam);
	if (nf < 0)
	{	printf("Can't connect to host -- will try to get status.\n");
		execl("/usr/bin/hosts", "tn", j, 0);
		exit();
	}
	signal(SIGINR,1);
	printf("Open\n");
	gtty(0,ttyflags);
	oldflags = ttyflags[2];
	if (!echoflg)				/* disable echo, tabs,  */
		ttyflags[2] =& ~000032; 	/*  CR-LF mapping       */
	ttyflags[2] =|  000040;
	stty(0,ttyflags);
	i = getpid();
	fk = fork();
	if (fk == 0)
	{	fk = i;
		while ((j=read(nf,buf,BUFSIZ)) > 0)
		{	for (i=0; i<j; i++)
				if (buf[i] < 0)
					buf[i] = '\0';
			write(1,buf,j);
		}
		kill(fk,1);
	}
	else
	{	signal(1, &endit);		/* hangup */
		while ((j=read(0,buf,BUFSIZ)) > 0)
		{	for (i=0; i<j;)
			{	if (buf[i] == '\034')
					endit();
				if (echoflg && buf[i] == '\n')
					buf[i] == '\r';
				if (buf[i++] == '\r')
				{	for (k=j; k>i; --k)
						buf[k] = buf[k-1];
					buf[i] = '\n';
					j++;
				}
			}
			if (write(nf,buf,j) < 0)
				break;
		}
		endit();
	}
}
endit()
{
	kill(fk,9);
	ttyflags[2] = oldflags;
	stty(0,ttyflags);
	stty(0,ttyflags);
	exit();
}
