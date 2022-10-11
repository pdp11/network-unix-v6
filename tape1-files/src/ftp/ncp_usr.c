#include "ftp.h"
#include "usr.h"

chekds(gender)
{
    register int fd, *fp;
    register struct netopen *sp;
    int i, oldsig;

    extern struct net_stuff dsfds, dsfdr, NetParams;
    extern char *HOST_ID;
    extern long gethost();

	if (gender)
	{
	    fp = &(dsfds.fds);
	    sp = &(dsfds.np);
	}
	else
	{
	    fp = &(dsfdr.fds);
	    sp = &(dsfdr.np);
	}

	sp->o_type = RELATIVE | DIRECT;
	sp->o_relid = NetParams.fds;
	sp->o_lskt = gender + 4;
	sp->o_host = gethost(HOST_ID);
	sp->o_timo = FTPTIMO;

	*fp = fd = open("/dev/net/ncp", sp);
	if (fd < 0)
	{       fdprintf(1,"Can't open data socket: %s\n", errmsg(0));
		return(1);
	}
	return(0);
}

sndcmd()
{
    register char *p;
	p = linptr;

	*p++ = '\r'; *p++ = '\n'; *p++ = '\0';

	if (write(OUTSOCK, linbuf, strlen(linbuf)) < 0)
		die(24, "%s:  can't write command %s to net; %s\n",
		       progname, linbuf, errmsg(0));
}

GetC(ib)
struct iobuf *ib;
{
	if (--(ib)->ncleft < 0) return(PFill(ib));
	else return(*(ib)->nextp++);
}

PutC(c, ob)
int c;
struct iobuf *ob;
{
	*(ob)->nextp++ = c;
	if (--(ob)->ncleft < 0) Pflush(ob);
}

PFlush(b)
struct iobuf *b;
{
	if (b->flavor == NetHandle) net_write(b->fildes, b->buffer,
		(sizeof b->buffer)-(b->ncleft));
	else write(b->fildes, b->buffer, (sizeof b->buffer)-(b->ncleft));
	b->ncleft = 0;
	b->nextp = b->buffer;
}

PFill(ib)               /* returns first character in the buffer read */
struct iobuf *b;
{
	register i;

	if (b->flavor == NetHandle) i = net_read(b->fildes, b->buffer,
		(sizeof b->buffer)-(b->ncleft));
	else i = read(b->fildes, b->buffer, (sizeof b->buffer)-(b->ncleft));

	b->ncleft = (sizeof b->buffer) - i;
	b->nextp = b->buffer;
}
