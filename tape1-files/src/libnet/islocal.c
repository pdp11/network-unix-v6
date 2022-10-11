#define	LIBN
#include "netlib.h"
#include "netmap.h"

/*
 * return true if address is on local network
 */
islocal(addr)
netaddr	addr;
{
	register int i;

	if (!_loadmap())
		return 0;
	for (i = 0; i < _lhostcnt; ++i)
		if (net_part(addr) == net_part(_lhosttab[i])) {
			_freemap();
			return 1;
		}
	_freemap();
	return 0;
}
