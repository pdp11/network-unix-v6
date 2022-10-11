#include "cbuf.h"
#define Cwrap(p,t,b) {if(p==t) p=b; else if(p>t) fdprintf(2, "\nCWRAP:OOPS\n");}

/* -------------------------- _ C G E T C --------------------------- */

_Cgetc(Ccb)
    struct CircBuf *Ccb;
{
    int c;

    if (Cempty(*Ccb))
    {
	fdprintf(2, "\nCGETC:EMPTY\n");
	return(-1);
    }
    c = *Ccb->c_get++;
    Cwrap(Ccb->c_get, Ccb->c_buf + CBUFSIZE, Ccb->c_buf);

    Ccb->c_full = 0;

    return(c & 0377);
}

/* -------------------------- _ C U N G E T C ----------------------- */

_Cungetc(ch, Ccb)
    char ch;
    struct CircBuf *Ccb;
{

    if (Cfull(*Ccb))
	return(-1);

    --(Ccb->c_get);
    if (Ccb->c_get < Ccb->c_buf)
	Ccb->c_get =+ CBUFSIZE;
    *(Ccb->c_get) = ch;
    if (Ccb->c_put == Ccb->c_get)
	Ccb->c_full = 1;
    return(0);
}

/* -------------------------- _ C P U T C --------------------------- */

_Cputc(ch, Ccb)
    char ch;
    struct CircBuf *Ccb;
{

    if (Cfull(*Ccb))
    {
	fdprintf(2, "FLUSHED\r\n");
	return(-1);
    }

    *Ccb->c_put++ = ch;
    Cwrap(Ccb->c_put, Ccb->c_buf + CBUFSIZE, Ccb->c_buf);

    if (Ccb->c_put == Ccb->c_get)
	Ccb->c_full = 1;
    return(0);
}

/* -------------------------- _ C L E N --------------------------- */

_Clen(bp)
    struct CircBuf *bp;
{
    register struct CircBuf *rbp;
    int diff;

    rbp = bp;
    if (rbp->c_full)
	return(CBUFSIZE);
    diff = rbp->c_put - rbp->c_get;
    if (diff >= 0)
	return(diff);
    else
	return(CBUFSIZE + diff);
}

/* -------------------------- _ C W R I T E ------------------------- */

_Cwrite(fd, cb, n)
    int fd;
    struct CircBuf *cb;
    int n;
{
    register char *get, *put;
    register int nwrite;
    char *bufstart, *bufend;
    char buffer[CBUFSIZE];  /* For when data is in two parts */

    if (n < 0)
	return(-1);
    if ((n == 0) || Cempty(*cb))
	return(0);

    get = cb->c_get;
    put = cb->c_put;
    bufstart = cb->c_buf;
    bufend = bufstart + CBUFSIZE;

/* Check for easy case -- get < put */

    if ((get < put) || (n <= (bufend - get)))
    {
	if (get < put)
	{
	    nwrite = put - get;
	    nwrite = nwrite < n ? nwrite : n;
	}
	else
	    nwrite = n;
	nwrite = write(fd, get, nwrite);
	if (nwrite > 0)
	{
	    cb->c_get =+ nwrite;
	    Cwrap(cb->c_get, bufend, bufstart);
	    cb->c_full = 0;	   /* It is unquestionably no longer full */
	}
	return(nwrite);
    }

/* Hard. Collect get...end & start...put into buffer, write out. */

    nwrite = bufend - get;
    nwrite = n < nwrite? n : nwrite;
    put = buffer;
    for (; nwrite > 0; --nwrite)
	*put++ = *get++;
    n =- (put - buffer);

    nwrite = cb->c_put - bufstart;
    nwrite = nwrite < n? nwrite : n;
    get = bufstart;
    for (; nwrite > 0; --nwrite)
	*put++ = *get++;

    nwrite = write(fd, buffer, put - buffer);
    if (nwrite <= 0)
	return(nwrite);
    cb->c_get =+ nwrite;
    if (cb->c_get >= bufend)
	cb->c_get =- CBUFSIZE;
    cb->c_full = 0;	   /* It is unquestionably no longer full */
    return(nwrite);
}

/* -------------------------- _ C R E A D --------------------------- */

_Cread(fd, cb, n)
    int fd;
    struct CircBuf *cb;
    int n;
{
    register char *get, *put;
    register int nread;
    int n2;
    char *bufstart, *bufend;
    char buffer[CBUFSIZE];  /* For when data is in two parts */

    if ((n < 0) || (n > CBUFSIZE))
	return(-1);
    if ((n == 0) || Cfull(*cb))
	return(0);

    get = cb->c_get;
    put = cb->c_put;
    bufstart = cb->c_buf;
    bufend = bufstart + CBUFSIZE;

/* Check for easy case -- put < get */

    if ((put < get) || (n <= (bufend - put)))
    {
	if (put < get)
	{
	    nread = get - put;
	    nread = (nread < n) ? nread : n;
	}
	else
	{
	    nread = n;
	}
	nread = read(fd, put, nread);
	if (nread > 0)
	{
	    cb->c_put =+ nread;
	    Cwrap(cb->c_put, bufend, bufstart);
	    if (cb->c_put == cb->c_get)
		cb->c_full = 1;
	}
	return(nread);
    }

/* Hard. Collect get...end & start...read in to buffer, put in cb */

    nread = Cfree(*cb);
    nread = (n < nread) ? n : nread;
    nread = read(fd, &buffer[0], nread);
    if (nread <= 0)
	return(nread);

    n = nread;		/* save it */

    nread = bufend - put;
    nread = n < nread? n : nread;
    n2 = nread;
    get = buffer;
    for (; nread > 0; --nread)
	*put++ = *get++;

    nread = cb->c_get - bufstart;
    n2 = n - n2;
    nread = (nread < n2) ? nread : n2;
    put = bufstart;
    for (; nread > 0; --nread)
	*put++ = *get++;

    cb->c_put =+ n;
    if (cb->c_put >= bufend)
	cb->c_put =- CBUFSIZE;
    if (cb->c_put == cb->c_get)
	cb->c_full = 1;
    return(n);
}
