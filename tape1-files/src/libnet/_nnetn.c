#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * convert numeric network name to network number
 */
netnumb
_nnetn(name)
char *name;
{
	unsigned	net;

#ifdef	DEBUG
	printf("_nnetn(%s)\n", name);
#endif	DEBUG
	if (*_atouiv(name, &net) || net >= _netmap->map_nnet)
		return BADNET;
	return (netnumb)net;
}
