#define	LIBN
#include "netlib.h"
/*
 * convert host name to network address
 */
netaddr
gethost(name)
{
	netnumb	net;
	netaddr	addr;
	char	hname[NETNAMSIZ+1];
	char	nname[NETNAMSIZ+1];

	mkbadhost(addr);
	if (*_2parse(name, 1, hname, nname))
		return addr;
	if (!_loadmap())
		return addr;
	net = _isnum(nname)? _nnetn(nname) : _snetn(nname);
	if (!isbadnet(net))
		if (_isnum(hname))
			addr = _nhosta(hname, net); else
			addr = _shosta(hname, net);
	_freemap();
	return addr;
}
