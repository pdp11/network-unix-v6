#define	LIBN
#include "netlib.h"
/*
 * convert network name to network number
 */
netnumb
getnet(name)
char	*name;
{
	netnumb	net;
	char	nname[NETNAMSIZ+1];

	if (*_1parse(name, nname))
		return BADNET;
	if (!_loadmap())
		return BADNET;
	net = _isnum(nname)? _nnetn(nname) : _snetn(nname);
	_freemap();
	return net;
}
