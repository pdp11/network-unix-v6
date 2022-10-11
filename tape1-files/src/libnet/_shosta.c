#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * convert symbolic host name to network address
 */
netaddr
_shosta(name, net)
char	*name;
netnumb	net;
{
	netaddr addr;
	register host_ent *hp;

#ifdef	DEBUG
	printf("_shosta(%s, %d)\n", name, net);
#endif	DEBUG
	if (hp = _shostp(name, net))
		addr = hp->host_addr; else
		mkbadhost(addr);
	return addr;
}
