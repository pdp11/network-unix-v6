#define	LIBN
#include "netlib.h"
#include "netmap.h"
#define	NULL	0
/*
 * convert symbolic host name to host entry pointer
 */
host_ent *
_shostp(name, net)
char	*name;
netnumb	net;
{
	int	match, best = 0;
	int	nnet, nhost;
	net_ent *np;
	host_ent *hp, *bhp;

#ifdef	DEBUG
	printf("_shostp(%s, %d)\n", name, net);
#endif	DEBUG
	if (net < 0 || net >= _netmap->map_nnet)
		return NULL;
	/*
	 * for single network or entire search chain
	 */
	nnet = 1;
	if (isanynet(net)) {
		net = _nettab[0].net_next;
		nnet = _netmap->map_nnet - 1;
	}
	best = 0;
	do {
		if ((np = _nnetp(net)) == NULL)
			return NULL;
		/*
		 * for each host on network
		 *	search name list
		 *	stop on exact match
		 *	remember best substring match
		 */
		hp = &_hosttab[np->net_host];
		nhost = np->net_nhost;
		while (nhost-- > 0) {
			match = _namecmp(name, hp->host_name);
			if (match == 1)
				return hp;
			if (match < best) {
				best = match;
				bhp = hp;
			}
			++hp;
		}
		net = np->net_next;
	} while (net != 0 && --nnet > 0);
	return best? bhp : NULL;
}
