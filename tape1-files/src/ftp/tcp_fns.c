#include "ftp.h"

/* subroutines shared by ftpsrv & srvrftp */
int synchno = 0;

ECALL(subr, s)
int subr;
char *s;
{
	extern int errno;

	if(subr<0) ecmderr(errno,"can't %s\n", s);
}

inherit_net(handle,fdnum)
	/* do initialization of the handle structure                       */
/* NOTE: this routine has an important side-effect: srvrftp does a */
/*  NetInit() followed immediately by a get_stuff(), which has the */
/*  effect of doing an fstat() on fds 0.                           */
int fdnum;
struct net_stuff *handle;
{
	handle -> fds = fdnum;

}

ins()
{
    signal(SIGURG,ins);
    synchno = 1;
}

urgon(handle)
struct net_stuff *handle;
{
	    ioctl(handle -> fds, NETSETU, NULL);
}

urgoff(handle)
struct net_stuff *handle;
{
	    ioctl(handle -> fds, NETRSETU, NULL);
}

NetInit(handle)
struct net_stuff *handle;
{
	handle -> fds = -1;
}

net_open(handle, hnum, socket)
struct net_stuff *handle;
netaddr hnum;
portsock socket;		/* socket desired */
{
	extern int errno;
	struct con npo;

	npo.c_fcon = hnum;
	mkanyhost(npo.c_lcon);
	npo.c_lport = 0;
	npo.c_fport = socket;
	npo.c_timeo = FTPTIMO;
	npo.c_mode = (CONACT | CONTCP);
	npo.c_sbufs = npo.c_rbufs=1;
	npo.c_lo = npo.c_hi = 0;
	if ((handle -> fds = open(tcpfile, &npo)) >= 0)
	    ioctl(handle -> fds, NETSETE, NULL);
	return(handle -> fds);
}


net_pclose(handle)
struct net_stuff *handle;
{
    int i;
    if (handle -> fds >= 0)
    {
	ioctl(handle -> fds, NETCLOSE, NULL);
	read(handle -> fds, &i, sizeof i);/* if this is sendonly, this waits
					   for the fin. */
	close(handle -> fds);
	handle -> fds = -1;
    }
}


net_listen(handle)      /* listen for a connection, leave stuff in handle */
struct net_stuff *handle;
{

	handle -> np.c_lport = FTPSOCK;
	handle -> np.c_mode = CONTCP;
	handle -> np.c_fport = 0;
	mkanyhost(handle->np.c_fcon);
	mkanyhost(handle->np.c_lcon);
	handle -> np.c_sbufs = handle -> np.c_rbufs=1;
	handle -> np.c_hi = handle -> np.c_lo = 0;

	while ((handle -> fds = open (tcpfile, &handle -> np)) < 0)
		sleep (DAEMSLEEP);
}

net_close(handle)
struct net_stuff *handle;
{
    if (handle -> fds >= 0)
	{
	    close(handle -> fds);
	    handle -> fds = -1;
	}
}

net_vclose(handle)		/* suitable for use from within a VFORK just 
				   before an EXEC */
struct net_stuff *handle;
{
    if (handle -> fds >= 0)
	    close(handle -> fds);
}

get_stuff(handle)       /* fill the net_stuff structure with useful info */
struct net_stuff *handle;        /* stuff block pointer */
{
	ECALL(ioctl(handle -> fds, NETGETS, &(handle -> ns)),
		"get status of network connection");

	ECALL(ioctl(handle -> fds, NETSETE, NULL),
		"set status of network connection");
}

ftpsrv_plumb(handle)   /* juggle file descriptors for the ftpsrvr program */
struct net_stuff *handle;        /* stuff block pointer */
{
	dup2 (handle -> fds, 0);
	dup2 (handle -> fds, 1);
	if (handle -> fds>1) close (handle -> fds);
}

netaddr GetHostNum(handle)
/* extract the host number from the net_stuff structure */
struct net_stuff *handle;        /* stuff block pointer */
{
	return(handle -> ns.n_fcon);
}


net_read(handle, buff, count)
struct net_stuff *handle;        /* stuff block pointer */
char *buff;
int count;
{
	return(read(handle -> fds, buff, count));
}

net_write(handle, buff, count)
struct net_stuff *handle;        /* stuff block pointer */
char *buff;
int count;
{
	return(write(handle -> fds, buff, count));
}


int
tsturg(handle)
struct net_stuff *handle;        /* stuff block pointer */
{
    	ioctl(handle -> fds, NETGETS, &(handle -> ns));
	return handle -> ns.n_state & UURGENT;
}
    
