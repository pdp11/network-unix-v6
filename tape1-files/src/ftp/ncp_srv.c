#include "ftp.h"

/* subroutines for srvrftp only (use srvrftp externals) */

/*name:
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

dataconnection( offset, handle )
int offset;
struct net_stuff *handle;
{
	extern struct net_stuff NetParams;

	register netfid;	/* id of network data connection */
	struct netopen *netps;   /* structure of network gubbish */

	netps = &(handle->np);
	netps->o_host = NetParams.np.o_host;
	netps->o_lskt = offset - 2;      /* set local from telnet base */
	netps->o_fskt[0] = 0;        /* clear any high bits */
	netps->o_fskt[1] = (offset^01);  /* make (u+4,u+5) or (u+5,u+4) */
	netps->o_relid = 1; /* specify local telnet base */
	/* specify a relative, direct open that we initiate */
	netps->o_type = (RELATIVE | DIRECT | INIT );
	netps->o_nmal = 3000;        /* set allocation level */
	netps->o_timo = 60;		/* give up after sixty seconds */

	/* try and open network data connection */
	if( (netfid = open( ncpfile,&netps )) < 0 )
	{
		netreply("454 Can't Establish Data Connection; %s.\r\n",errmsg(0));
		exit( -1 );
	}
	return( netfid );	/* give file id back */
}


/*name:
	sendsock

function:
	send ftp sock command

algorithm:
	send ascii sock string
	send ascii local socket number
	send cr lf

parameters:
	offset from base socket to send

returns:
	nothing

globals:
	openparam.o_lskt

calls:
	netreply
	locv (sys)

called by:
	store
	retrieve
	append

history:
	initial coding 4/12/76 by S. F. Holmgren

*/

sendsock( offset )
{

	netreply("255 SOCK %s\r\n",locv(0,NetParams.np.o_lskt+offset-2));

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

netreply(printargs)
char *printargs;
{
	extern struct net_stuff NetParams;
	extern int go_die();

	if(fprintf(NetParams.fds,"%r", &printargs) < 0){
		fdprintf(ERRFD,"(netreply) error in writing [");
		fdprintf(ERRFD, "%r", &printargs);
		fdprintf(ERRFD,"]; %s\n", errmsg(0));
		go_die(ERRFD, "\n");
	}
}

