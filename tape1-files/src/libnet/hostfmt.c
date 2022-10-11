#define	LIBN
#include "netlib.h"

/*
 * produce a formatted network address
 *	type	format
 *	 0	address
 *	 1	address,netnumb
 *	 2	address,netname
 *	-1	netnumb address
 *	-2	netname address
 * returns pointer to static area containing the address
 */
char *
hostfmt(addr, type)
netaddr	addr;
{
	netnumb	net;
	register char *endp;
	static	char	buf[2*(NETNAMSIZ+1)];

	*(endp = buf) = '\0';
	net = net_part(addr);
	/*
	 * add leading network, if any
	 */
	switch (type) {
	case -1:
	case -2:
		sprintf(endp, "%s ", netfmt(net, -type));
		break;
	}
	/*
	 * advance to next empty spot
	 */
	while (*endp)
		++endp;
	/*
	 * add network address
	 */
	switch (NETA_TYPE(netcap(net))) {
	case NETA_LONG:
		/* long octal */
		sprintf(endp, "0%O", (long)imp_part(addr));
		break;
	case NETA_DIAL:
		/* dial digits */
		sprintf(endp, "%s", dialnum(addr));
		break;
	case NETA_REG:
		/* hoi/imp */
		sprintf(endp, "%d/%d", hoi_part(addr), imp_part(addr));
		break;
	}
	/*
	 * advance to next empty space
	 */
	while (*endp)
		++endp;
	/*
	 * add trailing network name, if any
	 */
	switch (type) {
	case 1:
	case 2:
		sprintf(endp, ",%s", netfmt(net, type));
		break;
	}
	return buf;
}
