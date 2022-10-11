#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * local network number
 */
netnumb
thisnet()
{
	netnumb	net;

	if (!_loadmap())
		return BADNET;
	net = _nnetp(ANYNET)->net_next;
	_freemap();
	return net;
}
