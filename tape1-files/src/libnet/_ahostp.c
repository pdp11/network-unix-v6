#define	LIBN
#include "netlib.h"
#include "netmap.h"
#define	NULL	0
/*
 * convert network address to host entry pointer
 *	the algorithm is a binary search
 */
host_ent *
_ahostp(addr)
netaddr	addr;
{
	register host_ent *lp, *mp, *hp;
	register int cc, imp, hoi;
	register net_ent *np;

	/*
	 * find network entry, if bad, give up
	 */
	if ((np = _nnetp((netnumb)net_part(addr))) == NULL)
		return NULL;
	/*
	 * setup lo and hi search pointers
	 *	extract imp and hoi for search
	 */
	lp = &_hosttab[np->net_host];
	hp = lp + np->net_nhost - 1;
	imp = imp_part(addr);
	hoi = hoi_part(addr);
	/*
	 * search for matching host address
	 *	comparison order is imp, then hoi
	 */
	while (lp <= hp) {
		mp = lp + (hp - lp) / 2;
		cc = imp - imp_part(mp->host_addr);
		if (cc == 0)
			cc = hoi - hoi_part(mp->host_addr);
		/*
		 * if exact match, return
		 */
		if (cc == 0)
			return mp;
		/*
		 * compute new lo or hi pointer
		 */
		if (cc > 0)
			lp = mp + 1; else
			hp = mp - 1;
	}
	/*
	 * not found
	 */
	return NULL;
}
