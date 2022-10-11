/*  This package provides the data transmission routines used by both the
    user and server ftp for data connections.  It also provides the routines
    for dealing with performance statistics on the data transmissions.
    It is designed to meet the specifications of RFC 765 (aka IEN 149) - File
    Transfer Protocol by J. Postel, June, 1980.

	The current version supports the following combinations:
mode - only stream mode is supported
type - ASCII non-print, image, and local (8 bit byte size)
structure - file, record (if also in ASCII mode)
 
 The following external routines are defined within this package:
 inittime() - sets up an assortment of timers and counters
 dumptime(logfn) - calls logfn with a printable string of performance
	statistics
 rcvdata(NetFile, LocalFile, logfn) - receive data from the network
 senddata(LocalFile, NetFile, logfn) - send data to network
 
 The following externals are tested by these routines:
    Each of the following variables should really be an enumeration type, but
    too much of the rest of the code would have to be changed to support this
    bit of cleanliness for now.

 type -  TYPEA,TYPEAN - ASCII Non-print
	 TYPEAT - ASCII Telnet format effectors (not supported)
	 TYPEAC - ASCII Carriage Control characters (not supported)
	 TYPEEN - EBCDIC Non-print (not supported)
	 TYPEET - EBCDIC Telnet format effectors (not supported)
	 TYPEEC - EBCDIC Carriage control characters (not supported)
         TYPEI - Image mode
         TYPEL - Local mode (only 8 bit byte supported)
 stru -  STRUF - file structure
	 STRUR - record structure
	 STRUP - page structure (not supported)
 mode -  MODES - stream mode
	 MODEB - block mode (not supported)
	 MODEC - compressed mode (not supported)

 abrtxfer - this variable will cause a data transfer to be stopped if it
            becomes true (1) during the transfer.  Intended for use by signal
	    handlers.  It is set false (0) on entry to the data transfer
	    routines.

   Initial coding - Alan G. Nemeth, BBN, May 21, 1981
*/

#include "ftp_lib.h"
#include "sys/types.h"
#include "sys/timeb.h"
#include "stdio.h"
#include "errno.h"
#include "ftp.h"

struct timeb    starttime;

long    bytesmoved = 0;		/* # of bytes on net connection */

time_t totaltime = 0;		/* elapsed time of transmission */

extern int  errno;
#define FTPESC '\0377'
#define FTPEOR '\01'
#define FTPEOF '\02'

/*
	rcvdata (NetHandle, file, logfn) is used to receive data from the
        network and write it into a file.  When entered, the network file and
	local file are already open and hooked up.  Upon completion, the files
	remain connected and the user of this routine should see to closing
	them.  logfn is a function which may be called with a string in case
	of failures during the transfer.  The string is formatted with an
	appropriate FTP reply code so that the usual case will be to call
	with logfn set to netreply during srvrftp, and printf during usrftp.

	rcvdata returns 0 if the transfer completes entirely successfully and
	-1 in case of any difficulty at all. 
*/

rcvdata (NetHandle, file, logfn)
struct net_stuff   *NetHandle;
FILE   *file;
int     logfn ();
{
    register int    cnt;
    int     escflg,
            retflg;
    char    buf[BUFSIZ];
    register char  *ptr;
    register char   c;
    char    msgbuf[128];

    errno = 0;
    escflg = retflg = 0;
    abrtxfer = 0;
    timeon ();
    switch (type)
    {
	case TYPEL: 
	case TYPEI: 
	    {
		int     fid = fileno (file);

		if (mode != MODES || stru != STRUF)
		{
		    timeoff ();
		    logfn (
			    "554 Unimplemented mode/type/structure combination\n");
		    return (-1);
		}
		while ((cnt = net_read (NetHandle, buf, BUFSIZ)) > 0)
		{
		    bytesmoved += cnt;
		    if (write (fid, buf, cnt) != cnt)
		    {
			timeoff ();
			sprintf (msgbuf, "552 Write failed: %s\n",
				errmsg (0));
			logfn (msgbuf);
			return (-1);
		    }
		    if (abrtxfer)
		    {
			timeoff ();
			return (-1);
		    }
		}
		timeoff ();
		if (cnt == 0)
		    return (0);
		else
		{
		    sprintf (msgbuf, "426 Data connection failure: %s\n",
			    errmsg (0));
		    logfn (msgbuf);
		    return (-1);
		}
	    }

	case TYPEA: 
	    {
		if ((mode != MODES) ||
			((stru != STRUF) && (stru != STRUR)))
		{
		    timeoff ();
		    logfn (
			    "554 Unimplemented mode/type/structure combination\n");
		    return (-1);
		}

		for (;;)
		{
		    if (abrtxfer)
		    {
			timeoff ();
			return (-1);
		    }
		    if (ferror (file))
		    {
			timeoff ();
			sprintf (msgbuf, "552 Write failed: %s\n",
				errmsg (0));
			logfn (msgbuf);
			return (-1);
		    }
		    cnt = net_read (NetHandle, buf, BUFSIZ);
		    if (cnt <= 0)
			break;
		    bytesmoved += cnt;
		    for (ptr = buf; ptr < buf + cnt; ptr++)
			switch (c = *ptr)
			{
			    case '\r': 
				if (escflg)
				{
				    putc (FTPESC, file);
				    escflg = 0;
				}
				if (retflg++)
				    putc (c, file);
				continue;

			    case '\n': 
				retflg = 0;
				putc (c, file);
				continue;

			    case '\0': 
				if (retflg)
				{
				    retflg = 0;
				    putc ('\r', file);
				}
				else
				    putc (c, file);
				continue;

			    case FTPESC: 
				if (retflg)
				{
				    putc ('\r', file);
				    retflg = 0;
				}
				if (stru != STRUR || escflg++)
				    putc (c, file);
				continue;

			    case FTPEOR: 
				if (escflg)
				{
				    putc ('\n', file);
				    escflg = 0;
				}
				else
				    putc (c, file);
				continue;

			    case FTPEOF: 
				if (escflg)
				{
				    timeoff ();
				    return (0);
				}
				else
				    putc (c, file);
				continue;

			    default: 
				if (retflg)
				{
				    putc ('\r', file);
				    retflg = 0;
				}
				if (escflg)
				{
				    putc (FTPESC, file);
				    escflg = 0;
				}
				putc (c, file);
				continue;
			}
		}
		timeoff ();
		if (cnt == 0)
		    return (0);
		else
		{
		    sprintf (msgbuf, "426 Data connection failure: %s\n",
			    errmsg (0));
		    logfn (msgbuf);
		    return (-1);
		}
	    }

	default: 
	    timeoff ();
	    logfn (
		    "554 Unimplemented mode/type/structure combination\n");
	    return (-1);
    }

/* NOTREACHED */

}

/*
	senddata(file, NetHandle, logfn)  is the reverse of rcvdata - it copies
	data from the local file to the network.  Otherwise, it's calling
	conventions are identical with rcvdata.
*/
senddata (file, NetHandle, logfn)
struct net_stuff   *NetHandle;
register FILE  *file;
int     logfn ();
{
    register int    cnt;
    char    buf[BUFSIZ];
    register char  *ptr;
    register int    c;
    char    msgbuf[128];

    errno = 0;
    abrtxfer = 0;
    timeon ();
    switch (type)
    {
	case TYPEL: 
	case TYPEI: 
	    {
		int     fid = fileno (file);

		if (mode != MODES || stru != STRUF)
		{
		    timeoff ();
		    logfn (
			    "554 Unimplemented mode/type/structure combination\n");
		    return (-1);
		}
		while ((cnt = read (fid, buf, BUFSIZ)) > 0)
		{
		    bytesmoved += cnt;
		    if (dat_write (NetHandle, buf, cnt) != cnt)
		    {
			timeoff ();
			sprintf (msgbuf,
				"426 Data connection failure: %s\n",
				errmsg (0));
			logfn (msgbuf);
			return (-1);
		    }
		    if (abrtxfer)
		    {
			timeoff ();
			return (-1);
		    }
		}
		timeoff ();
		if (cnt == 0)
		    return (0);
		else
		{
		    sprintf (msgbuf, "552 Read of file failed: %s\n",
			    errmsg (0));
		    logfn (msgbuf);
		    return (-1);
		}
	    }

	case TYPEA: 
	    {
		if ((mode != MODES) ||
			((stru != STRUF) && (stru != STRUR)))
		{
		    timeoff ();
		    logfn (
			    "554 Unimplemented mode/type/structure combination\n");
		    return (-1);
		}

#define myputc(chr)  {   *ptr++ = chr; \
		if (ptr == &buf[BUFSIZ]) \
		{ \
		    if ((cnt = dat_write (NetHandle, buf, BUFSIZ)) != BUFSIZ)			\
			    goto finishup; \
			bytesmoved += BUFSIZ; \
			ptr = buf; \
		} \
	    }

	    ptr = buf;
	    cnt = 0;
	    for (;;)
	    {
		if (abrtxfer)
		{
		    timeoff ();
		    return (-1);
		}
		if (ferror (file))
		{
		    timeoff ();
		    sprintf (msgbuf, "552 Read failed: %s\n",
			    errmsg (0));
		    logfn (msgbuf);
		    return (-1);
		}

		switch (c = getc (file))
		{
		    case '\r': 
			myputc (c);
			myputc ('\0');
			continue;

		    case '\n': 
			if (stru == STRUR)
			{
			    myputc (FTPESC);
			    myputc (FTPEOR);
			}
			else
			    myputc (c);
			continue;

		    case (FTPESC & 0377): 
			if (stru == STRUR)
			    myputc (FTPESC);
			myputc (FTPESC);
			continue;

		    case EOF: 
			if (stru == STRUR)
			{
			    myputc (FTPESC);
			    myputc (FTPEOF);
			}
			if ((cnt =
				    dat_write (NetHandle, buf, ptr - buf))
				!= (ptr - buf))
			    goto finishup;
			bytesmoved += ptr - buf;
			timeoff ();
			return (0);

		    default: 
			myputc (c);
			continue;
		}
	    }
    finishup: 
	    timeoff ();
	    if (cnt == 0)
		return (0);
	    else
	    {
		sprintf (msgbuf, "426 Data connection failure: %s\n",
			errmsg (0));
		logfn (msgbuf);
		return (-1);
	    }
    }

default: 
    timeoff ();
    logfn (
	    "554 Unimplemented mode/type/structure combination\n");
    return (-1);
}

/* NOTREACHED */

}

/*
	inittime() - establishes a base by zeroing a count of total bytes
		     moved, and the totaltime spent moving them.
        timeon() - internal routine.  Starts the clock.
	timeoff() - internal routine.  Stops the clock and adds accumulated
		    time to totaltime.
	dumptime() - provides a formatted string describing the accumulated
		     performance statistics.
*/

inittime ()
{
    bytesmoved = 0;
    totaltime = 0;
}

static  timeon ()
{
    ftime   (&starttime);
}

static  timeoff ()
{
    struct timeb    endtime;

    ftime (&endtime);
    totaltime += endtime.time - starttime.time;
}

char   *dumptime ()
{
    static char msgbuf[128];

    msgbuf[0] = '\0';
    if (bytesmoved > 0 && totaltime != 0)
	sprintf (msgbuf,
		"Transferred %D bytes in %D seconds ( %D bps, %D bytes/sec )\n",
		bytesmoved,
		totaltime,
		(8 * bytesmoved) / totaltime,
		bytesmoved / totaltime);
    else
	if (bytesmoved > 0)
	    sprintf (msgbuf,
		    "Transferred %D bytes in %D seconds\n",
		    bytesmoved,
		    totaltime);
    return (msgbuf);
}

dat_write(handle, buff, count)
struct net_stuff *handle;        /* stuff block pointer */
char *buff;
int count;
{
    int i;

    errno = 0;
    while ((i = net_write(handle, buff, count)) < 0)
	{
	    if (
		  (errno == ENETSTAT) &&
		  (
		    (ioctl(handle -> fds, NETGETS, &(handle -> ns))),
		    (handle -> ns.n_state & URXTIMO)
		  )
		)
	    {
		errno = 0;
		fprintf (stderr, "!");
	    }
	    else break;
	}
    return i;
}
