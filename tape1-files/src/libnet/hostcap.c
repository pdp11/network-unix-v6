#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * return capability associated with network address
 */
unsigned
hostcap(addr)
netaddr	addr;
{
	register host_ent *hp;
	register unsigned cap;

	if (!_loadmap())
		return 0;
	cap = (hp = _ahostp(addr))? hp->host_cap : 0;
	_freemap();
	return cap;
}
