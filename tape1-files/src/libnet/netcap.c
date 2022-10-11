#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * return capability associated with network number
 */
unsigned
netcap(net)
netnumb	net;
{
	register net_ent *np;
	register unsigned cap;

	if (!_loadmap())
		return 0;
	cap = (np = _nnetp(net))? np->net_cap : 0;
	_freemap();
	return cap;
}
