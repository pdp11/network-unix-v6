#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * convert numeric host address to network address
 */
netaddr
_nhosta(name, net)
char	*name;
netnumb	net;
{
	register char	*sp;
	netaddr		hosta;		/* host address */
	long		hostn;		/* host number input */
	netnumb		anet;
	net_ent		*netp;
	unsigned	imp, hoi;	/* imp, host-on-imp */
	unsigned	logh;		/* logical host */
	netaddr 	stolhost();

#ifdef	DEBUG
	printf("_nhosta(%s, %d)\n", name, net);
#endif	DEBUG
	if (!*(sp = name))
		goto badhost;
	/*
	 * if no net, use 1st local net
	 */
	anet = isanynet(net)? _nnetp(ANYNET)->net_next : net;
	netp = _nnetp(anet);
	/*
	 * Parse the host number, allowing for special networks.
	 * Set the non-network part of the host id.
	 */
	switch (NETA_TYPE(netp->net_cap)) {
	case NETA_LONG:	/* long address */
		if (*(sp = _atoulv(sp, &hostn)) != '\0')
			goto badhost;
		host_set(hosta, hostn);
		break;

	case NETA_DIAL:	/* dial digits */
		imp = _dialstr(sp);
		imp_set(hosta, imp);
		break;

	case NETA_REG:	/* regular hoi/imp format */
		/* if no '/', then old format */
		if (*(sp = _atoulv(sp, &hostn)) == '\0') {
			hosta = stolhost(hostn, anet);
			break;
		}
		if (*sp++ != '/')
			goto badhost;
		hoi = hostn;
		if (*(sp = _atouiv(sp, &imp)) == '\0') {
			/* hoi/imp */
			imp_set(hosta, imp);
			hoi_set(hosta, hoi);
			break;
		}
		if (*sp++ != '/')
			goto badhost;
		/* logh/hoi/imp */
		logh = hoi;
		hoi = imp;
		if (*(sp = _atouiv(sp, &imp)) != '\0')
			goto badhost;
		imp_set(hosta, imp);
		logh_set(hosta, logh);
		hoi_set(hosta, hoi);
		break;
	}
	/*
	 * Finally set the network part of the host id.
	 * If user specified a net, use that; if not, then if
	 * this is not "host 0", use the default. If it
	 * is "host 0", leave it all zero.
	 */
	if (!isanynet(net))
		net_set(hosta, net);
	else if (!isanyhost(hosta))
		net_set(hosta, anet);
	return hosta;
badhost:
	mkbadhost(hosta);
	return hosta;
}

/*
 * stolhost -- convert from old short form to standard host number
 */
static netaddr
stolhost(hostn, net)
long	hostn;
netnumb	net;
{
	netaddr	hosta;

	mkoldhost(hosta, hostn);
	/*
 	 * if already long, return it
	 */
	if (net_part(hosta) != 0)
		return hosta;
	/*
	 * if bad, return bad
	 */
	if (hostn < 0 || hostn > 255) {
		mkbadhost(hosta);
		return hosta;
	}
	/*
	 * form netaddr from old stuff
	 */
	if (!isanyhost(hosta)) {
		imp_set(hosta, hostn & 077);
		hoi_set(hosta, (hostn & 0300) >> 6);
		net_set(hosta, net);
	}
	return hosta;
}
