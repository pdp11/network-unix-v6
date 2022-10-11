#define	LIBN
#include "netlib.h"
#include "netmap.h"
#define	NULL	0
/*
 * convert network number to network name
 *	name is returned in static area
 */
char *
netname(net)
netnumb net;
{
	register char *np;
	
	return (np = netsyn(net, 0))? np : NONETNAME;
}

/*
 * return the nth name associated with a network
 *	0 is the primary name
 *	1 - N are aliases
 *	name is returned in static area
 */
char *
netsyn(net, syn)
netnumb	net;
{
	register net_ent *np;
	register char *lp;
	static char name[NETNAMSIZ+1];

	if (!_loadmap())
		return NULL;
	*name = '\0';
	if (np = _nnetp(net)) {
		lp = &_namebuf[np->net_name];
		while (*lp && syn > 0) {
			while (*lp)
				++lp;
			++lp, --syn;
		}
		strcpy(name, lp);
	}
	_freemap();
	return *name? name : NULL;
}
