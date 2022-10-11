/* Server MTP daemon */

#include "con.h"
#include "signal.h"
#include "netlib.h"

#define MTP_PORT 57
#define SERVER	"/etc/net/srvrmtp"

struct con openparam;

/*name:
	mtpsrv

function:
	to handle server mtp connections to port 57 and start
	up a program to handle the traffic.  It was done as two
	pieces to keep down the size of process waiting for connections
	to open( that is a smaller program to swap in and out ).
	Currently only allows one servermtp at a time, should be
	set up to deal with multiple server connections.


algorithm:
	forever
		set up params for a server connection to port 57
		wait for a connection to be established

		fork a child process
			make file desc 0 and 1 the telnet file
			exec the server process
		close the network file


parameters:
	none

returns:
	nothing

globals:
	openparams=

calls:
	open (sys)
	fork (sys)
	exec (sys)
	dup  (sys)
	close(sys)
	hostname        (libn)

called by:
	/etc/rc or /etc/net/neton

history:
	initial coding 4/18/76 by S. F. Holmgren
	use long hosts jsq BBN 3-20-79
	while error from net open don't die, but don't use all system time
		trying to reopen the mother jsq BBN 16July79.
	leave forking to call by daemon command and let network library
		handle setting localnet jsq BBN 18Feb80; also reformat.
	compiled with pcc, also anti-reformat dm BBN 20Mar80.
	modified to be mtpsrv Eric Shienbrood BBN 3 Apr 81
	modified to use new network library Eric Shienbrood BBN 23 Sep 81
*/

main ()
{
	register  netfid;
	struct netstate statparam;
	char *us, *them;
	char *getuc ();

	dup2 (1, 2);			/* make log file standard error */
	while (1)
	{
		openparam.c_mode = CONTCP;
		openparam.c_lport = MTP_PORT;
		openparam.c_rbufs = 4;	/* Ask for 4K receive buffer */
		while ((netfid = open ("/dev/net/tcp", &openparam)) < 0)
			sleep (60);
		ioctl (netfid, NETSETE, 0);

		if (spawn () == 0)
		{
			us = getuc (thisname());	/* who are we? */
			ioctl (netfid, NETGETS, &statparam);
			them = hostname (statparam.n_con); /* who are they? */
			dup2 (netfid, 0);
			dup2 (netfid, 1);
			close (netfid);
			execl (SERVER, "srvrmtp", them, us, 0);
			exit (1);
		}
		close (netfid);
	}
}

/*
 * Uppercase a string in place. Return pointer to
 * null at end.
 */
char *
getuc(s)
    char *s;
{
    register char *p,
		  c;
    for (p = s; c = *p; p++)
    {
	if (c <= 'z' && c >= 'a')
	    *p -= ('a' - 'A');
    }
    return(s);
}
