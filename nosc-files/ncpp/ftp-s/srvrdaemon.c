#               /* Server FTP daemon */

#include "net_open.h"
#define SIGINR 15
#include "spawn.c"

/*name:
	srvrdaemon

installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc -L/usr/include:..:/usr/sys/ncpp srvrdaemon.c
	exit
: newer
	if ! { newer srvrdaemon.c /usr/net/etc/ftpsvr } exit
	echo ncpp/ftp-s/srvrdaemon.c:
: finale
	cc -L/usr/include:..:/usr/sys/ncpp -O -s srvrdaemon.c
	if ! -r a.out exit
	if ! -r /usr/sys/ncpp/ftp-s/srvrdaemon.c goto same
	if { cmp -s srvrdaemon.c /usr/sys/ncpp/ftp-s/srvrdaemon.c } goto same
	su cp srvrdaemon.c /usr/sys/ncpp/ftp-s/srvrdaemon.c
	su chmod 444 /usr/sys/ncpp/ftp-s/srvrdaemon.c
	rm -f srvrdaemon.c
: same
	su rm -f /usr/net/etc/ftpsvr
	su cp a.out /usr/net/etc/ftpsvr
	rm -f a.out
	su chmod 544 /usr/net/etc/ftpsvr
	su chown root /usr/net/etc/ftpsvr
	su chgrp system /usr/net/etc/ftpsvr

function:
	to handle server ftp connections to socket 3 and start
	up a program to handle the traffic.  It was done as two
	pieces to keep down the size of process waiting for connections
	to open( that is a smaller program to swap in and out ).


algorithm:
	forever
		set up params for a server connection to socket 3
		wait for a connection to complete

		spawn a child process
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

called by:
	/etc/rc file as /etc/srvrdaemon

history:
	initial coding 4/18/76 by S. F. Holmgren

*/

struct openparams openparams;

main( argc, argv )
int argc;
char **argv;
{
	register netfid;

signal(SIGINR,1);               /* ignore INS interrupts */
while( 1 )
{
	netfid = -1;
	while( netfid < 0 )
	{
		openparam.o_lskt = 3;
		openparam.o_type = 2;
		openparam.o_fskt[0] = 0;
		openparam.o_fskt[1] = 0;
		netfid = open( "/dev/net/anyhost",&openparam );
	}

	if( spawn() == 0 )
	{
		close( 0 );
		dup( netfid );
		close( 1 );
		dup( netfid );
		close( netfid );
		execl( "/usr/net/etc/srvrftp", argv[0], "server process", 0);
		exit( 1 );
	}
	close( netfid );
}
}
