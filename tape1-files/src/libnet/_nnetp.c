#define	LIBN
#include "netlib.h"
#include "netmap.h"
#define	NULL	0
/*
 * convert network number to network entry pointer
 */
net_ent *
_nnetp(net)
netnumb	net;
{
	if (net >= 0 && net < _netmap->map_nnet)
		return &_nettab[net];
	return NULL;
}
