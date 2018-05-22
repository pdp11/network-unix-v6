extern int errno;
#
#include "mailer.h"
#include "ffio.h"
#define FFAKE   0		  /* ff_read does no buffer stuffing  */
#define MAXNUM 077777

struct adrstruct    snoopadr;
#ifdef LOCDLVR
struct netstruct    net_loc;
#endif
#ifdef  ARPANET
struct netstruct    net_arpa;
#endif
#ifdef  POBOX
struct netstruct    net_pobox;
#endif


net_tabopen (pnetinfo, fromstart)
struct netstruct   *pnetinfo;
char    fromstart;
{
    register struct netstruct  *netinfo;
    register char  *path;

    switch ((netinfo = pnetinfo) -> net_tabfd)
    {
	case 0: 
	    if ((path = netinfo -> net_tpath) != 0)
	    {
		if ((netinfo -> net_tabfd = ff_open (path, 0)) == NOTOK) {
		    sysabrt (RPLLOC, "Unable to open %s name table",
			    netinfo -> net_ref);
		}
		break;
	    }
	    netinfo -> net_tpath = NOTOK;
	case NOTOK: 
	    return (FALSE);
	default: 
	    if (fromstart)	  /* back to the beginning    */
		ff_seek (netinfo -> net_tabfd, 0, 0);
    }
    return (TRUE);
}

host2adr (netinfo, fromstart, pname, adrline)
struct netstruct   *netinfo;
char    fromstart;		  /* start at beginning of list?  */
char   *pname,			  /* name of net "member" */
       *adrline;		  /* put corresponding "address" into this
				     buffer */
{
    int     thefd;
    int     tmp;
    char    host[20];
    register char  *name;

    if (!net_tabopen (netinfo, fromstart))
	return (FALSE);

    for (name = pname, thefd = netinfo -> net_tabfd;
	    (tmp = ff_read (thefd, &host, sizeof host, ": \t\n\377")) > 0;)
    {
	host[tmp - 1] = '\0';
	if (strequ (name, host))
	{
	    if ((tmp = ff_read (thefd, adrline, LINESIZE, "\n\000\377")) < 0)
		break;
	    adrline[tmp - 1] = '\0';
	    return (TRUE);
	}
	if ((tmp = ff_read (thefd, FFAKE, MAXNUM, "\n\000\377")) < 0)
	    break;
    }
    if (tmp < 0)
	sysabrt (RPLLOC, "Error reading %s name file", netinfo -> net_ref);
    return (FALSE);
}

adr2first (netinfo, adrstr, adrline)
struct netstruct   *netinfo;
char   *adrstr,
       *adrline;		  /* stuff official hostname into here */
{
    int thefd;
    char    linebuf[LINESIZE];
    register int    c;

    if (!net_tabopen (netinfo, TRUE))
	return (FALSE);
    thefd = netinfo -> net_tabfd;
    while ((c = ff_read (thefd, adrline, LINESIZE, ": \t\377")) > 0)
    {
	adrline[c - 1] = '\0';
	if ((c = ff_read (thefd, linebuf, LINESIZE, "\n\377")) <= 0)
	    break;
	linebuf[c - 1] = '\0';
	if (strequ (adrstr, linebuf))
	{
	    for (adrline[0] = lowtoup (adrline[0]), c = 1; adrline[c];)
		if (adrline[c++] == '-')
		{
		    adrline[c] = lowtoup (adrline[c]);
		    c++;
		}
	    return (TRUE);
	}
    }

    if (c < 0)
	sysabrt (RPLLOC, "Error reading %s name table", netinfo -> net_spec);
    return (FALSE);
}

host2first (netinfo, fromstart, hostr, adrline)
struct netstruct   *netinfo;
char    fromstart;		  /* start at beginning of list?  */
char   *hostr,
       *adrline;		  /* stuff official hostname into here */
{
    char    linebuf[LINESIZE];

    if (net_tabopen (netinfo, fromstart))
    {
	if (!host2adr (netinfo, fromstart, hostr, linebuf))
	    strcpy (hostr, linebuf);
	if (adr2first (netinfo, linebuf, adrline))
	    return (TRUE);
    }
    strcpy (hostr, adrline);
    return (FALSE);
}
