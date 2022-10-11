#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * convert symbolic network name to network number
 */
netnumb
_snetn(name)
char *name;
{
	register net_ent *np;
	register int nn;

#ifdef	DEBUG
	printf("_snetn(%s)\n", name);
#endif	DEBUG
	np = &_nettab[0];
	nn = _netmap->map_nnet;
	while (nn-- > 0) {
		if (_namecmp(name, np->net_name) == 1)
			return (netnumb)(np - &_nettab[0]);
		++np;
	}
	return BADNET;
}
