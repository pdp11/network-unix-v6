#include "ftp.h"

/* subroutines shared by ftpsrv & srvrftp */

CALL(subr, s)
int subr;
char *s;
{
	extern int errno;

	if(subr<0) cmderr(errno,"can't %s\n", s);
	return(subr);   /* so calling routine can take action */
}
ECALL(subr, s)
int subr;
char *s;
{
	extern int errno;

	if(subr<0) ecmderr(errno,"can't %s\n", s);
}

inherit_net(handle)
/* do initialization of the handle structure                       */
/* NOTE: this routine has an important side-effect: srvrftp does a */
/*  NetInit() followed immediately by a get_stuff(), which has the */
/*  effect of doing an fstat() on fds 0.                           */

struct net_stuff *handle;
{
	handle->fds = 0;
}

ins()
{
    signal(SIGINR,&ins);
    synchno++;
}

urgon(handle)
struct net_stuff *handle;
{
	sendins();
}

urgoff(handle)
struct net_stuff *handle;
{
	return;
}

NetInit(handle)
struct net_stuff *handle;
{
	handle->fds = 0;
}

net_open(handle, hnum, socket)
struct net_stuff *handle;
struct long_host hnum;		/* host to open a connection to */
portsock socket;		/* socket desired */
{
	extern int errno;

	handle->np.o_host = host;
	handle->np.o_fskt[0] = 0;
	handle->np.o_fskt[1] = socket;
	handle->np.o_timo = FTPTIMO;
	handle->np.o_type = (RELATIVE | DIRECT | INIT );
	return(handle->fds = open("/dev/net/ncp", &(handle->np)));
}


net_close(handle)
struct net_stuff *handle;
{
	close(handle->fds);
}


net_listen(handle)      /* listen for a connection, leave stuff in handle */
struct net_stuff *handle;
{
	handle->np.o_lskt = FTPSOCK;
	handle->np.o_type = 2;
	handle->np.o_fskt[0] = 0;
	handle->np.o_fskt[1] = 0;
	handle->np.o_host = 0;
	while ((handle->fds = open ("/dev/net/ncp", &(handle->np))) < 0)
		sleep (DAEMSLEEP);
}

get_stuff(handle)       /* fill the net_stuff structure with useful info */
struct net_stuff *handle;        /* stuff block pointer */
{
	ECALL(fstat(handle->fds, &(handle->np)), "get status of network connection");
}

ftpsrv_plumb(handle)   /* juggle file descriptors for the ftpsrvr program */
struct net_stuff *handle;        /* stuff block pointer */
{
	close (0);
	dup (handle->fds);
	close (1);
	dup (handle);
	close (handle->fds);
}

ftp_plumb(handle, which, ftpip) /* juggle file descriptors for the user */
struct net_stuff *handle;       /* ftp program */
int which;
int ftpip[2];
{
	if(which == 0){
		close(ftpip[0]);
		ftp2_plumber(0, handle->fds);
		if(ftpip[1] == 2) ftpip[1] = dup(2);
		close(2); dup(handle->fds);
		ftp2_plumber(3, ftpip[1]);
	}
	if(which == 1){
		close (ftpip[1]);
		ftp2_plumber (2, ftpip[0]);
		close(3); dup(handle->fds);
	}
}

ftp2_plumber(want, is)
int want, is;
{
	if (is == want) return;
	close (want);
	dup (is);
	close (is);
}

long GetHostNum(handle)
/* extract the host number from the net_stuff structure */
struct net_stuff *handle;        /* stuff block pointer */
{
	return(handle->np.o_host);
}


net_read(handle, buff, count)
struct net_stuff *handle;        /* stuff block pointer */
char *buff;
int count;
{
	return(read(handle->fds, buff, count));
}

net_write(handle, buff, count)
struct net_stuff *handle;        /* stuff block pointer */
char *buff;
int count;
{
	return(write(handle->fds, buff, count));
}

net_close(handle)
struct net_stuff *handle;
{
	close(handle -> fds);
}

net_pclos(handle)
struct net_stuff *handle;
{
	close(handle -> fds);
}
