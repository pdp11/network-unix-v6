#include "ftp.h"
#include "usr.h"

extern struct net_stuff dsfds, dsfdr, NetParams;
extern errno;

chekds(gender)
{
    register int fd, *fp;
    register struct con *sp;

    extern char *HOST_ID;

	if(gender)
	{
	    fp = &(dsfds.fds);
	    sp = &(dsfds.np);
	    sp->c_sbufs  = 3;
	    sp->c_rbufs = 0;
	}
	else
	{
	    fp = &(dsfdr.fds);
	    sp = &(dsfdr.np);
	    sp->c_rbufs  = 3;
	    sp->c_sbufs = 0;
	}

	sp->c_mode = CONTCP;
	sp->c_lport = NetParams.ns.n_lport;	/* same as telnet port */
	sp->c_fport = FTPSOCK - 1;	/* default server data socket. */
	sp->c_timeo = 5; /* FTPTIMO;*/
	sp->c_hi = sp->c_lo = 0;
	sp->c_fcon = NetParams.ns.n_fcon;
	mkanyhost(sp->c_lcon);
	if (isbadhost(sp->c_fcon))
		abort();
	*fp = fd = open(tcpfile, sp);
	if (fd < 0)
	{
	    if (errno != ENETTIM)
		    printf("Can't open data socket: %s\n", errmsg(0));
	    return(1);
	}

/*	ioctl(fd, NETSETE, NULL); */
	return(0);
}

extern char * linptr;
extern char linbuf[LINSIZ];
extern char progname [];

sndcmd()
{
    register char *p;
	p = linptr;

	*p++ = '\r'; *p++ = '\n'; *p++ = '\0';

	if (net_write(&NetParams, linbuf, strlen(linbuf)) < 0)
		die(24, "%s:  can't write command %s to net; %s\n",
		       progname, linbuf, errmsg(0));
}
