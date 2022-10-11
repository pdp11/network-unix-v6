#define	LIBN
#include "netlib.h"

/*
 * produce a formatted network number
 *	type	format
 *	 1	netnumb
 *	 2	netname
 * returns pointer to static area containing the string
 */
char *
netfmt(net, type)
netnumb	net;
{
	static char	buf[NETNAMSIZ+1];

	switch (type) {
	default:
		*buf = '\0';
		break;
	case 1:
		sprintf(buf, "%D", (long)net);
		break;
	case 2:
		sprintf(buf, "%s", netname(net));
		break;
	}
	return buf;
}
