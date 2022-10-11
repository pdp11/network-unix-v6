#include "ftp.h"

/* subroutines for srvrftp only (use srvrftp externals) */

/*
name:
	dataconnection

function:
	connect with either U4,U5 or U+5,U+4 from the base
	currently available thru file descriptor 0.

algorithm:
	set up appropriate open parameters
	try to open the file
		signal error
		exit
	return opened file

parameters:
	offset - offset from local socket

returns:
	file descriptor of opened data connection

globals:
	openparam=

calls:
	open (sys)
	netreply

called by:
	retrieve
	store
	append

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

dataconnection(offset, handle)
int offset;
struct net_stuff *handle;
{
	extern struct net_stuff NetParams;

	register int netfid;	/* id of network data connection */
	struct con *netps;   /* structure of network gubbish */

	netps = &(handle->np);
	netps->c_fcon = NetParams.ns.n_fcon;
	mkanyhost(netps->c_lcon);
	netps->c_lport = FTPSOCK - 1;		/* default unless PASV */
	netps->c_fport = NetParams.ns.n_fport;
	/* specify a relative, direct open that we initiate */
	netps->c_mode = CONACT | CONTCP;
	netps->c_rbufs = netps->c_sbufs = 0;
	*((offset & 1) ? &netps->c_sbufs : &netps->c_rbufs) = 3;
	netps->c_lo = netps->c_hi = 0;
	netps->c_timeo = 60;	/* give up after sixty seconds */
	sleep(2);

	/* try and open network data connection */
	if ((netfid = open (tcpfile, netps)) < 0)
	{
		netreply ("454 Can't Establish Data Connection; %s.\r\n",
			errmsg(0));
		exit (-1);
	}

/* 	ioctl(netfid, NETSETE, NULL); */
	return handle->fds = netfid;		/* give file id back */
}


/*name:
	netreply

function:
	send & log appropriate ascii responses

algorithm:
	send it to the standard output file & to the log file

parameters:
	str to be sent to net

returns:
	nothing

globals:
	none

calls:
	fdprintf(sys)
	go_die()

called by:
	the world

history:
	initial coding 4/13/76 by S. F. Holmgren
	replaced by fdprintf facility to permit greater
		versatility in replies (e.g.,
		including errmsg, etc.) dm/bbn Apr 10, '80
*/

/* VARARGS */
netreply(printargs)
char * printargs;
{
	extern struct net_stuff NetParams;
	extern go_die();
	char replyline [256];

	sprintf (replyline,"%r", &printargs);
	if (write (NetParams.fds, replyline, strlen (replyline)) < 0)
	{
		fprintf (ERRFD,"(netreply) error in writing [\n");
		fprintf (ERRFD, "%r", &printargs);
		fprintf (ERRFD,"]; %s\n", errmsg(0));
		go_die (ERRFD, "\n");
	}
}

