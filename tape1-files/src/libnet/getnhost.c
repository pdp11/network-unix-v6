#define	LIBN
#include "netlib.h"
/*
 * convert host name and network number to network address
 */
netaddr
getnhost(name, net)
char	*name;
netnumb	net;
{
	netaddr	addr;
	char	hname[NETNAMSIZ+1];

	mkbadhost(addr);
	if (*_1parse(name, hname))
		return addr;
	if (!_loadmap())
		return addr;
	if (_isnum(hname))
		addr = _nhosta(hname, net); else
		addr = _shosta(hname, net);
	_freemap();
	return addr;
}
