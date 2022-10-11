#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * return address of this host on network
 *	if anynet, use 1st address
 */
netaddr
thishost(net)
netnumb net;
{
	register unsigned i;
	netaddr	hosta;

	mkbadhost(hosta);
	if (!_loadmap())
		return hosta;
	if (isanynet(net)) {
		if (_lhostcnt > 0)
			hosta = _lhosttab[0];
	}
	else for (i = 0; i < _lhostcnt; ++i) {
		if (net_part(_lhosttab[i]) == net) {
			hosta = _lhosttab[i];
			break;
		}
	}
	_freemap();
	return hosta;
}
