#define	LIBN
#include "netlib.h"
#include "netmap.h"
#define	NULL	0
/*
 * network address to dial digits
 *
 * dial digits are stashed away in a buffer at the end of the network map
 * the imp field of a host address is actually the index into the buffer.
 * since the buffer cannot be extended at run time, an extension is used
 * to store any extra digits encountered during the address conversion.
 */
char *
dialnum(hosta)
netaddr	hosta;
{
	register int x;
	register char *p;
	static char buf[NETNAMSIZ+1];

	if (!_loadmap())
		return NULL;
	/*
	 * make sure it's a dialup network
	 */
	if (NETA_TYPE(netcap((netnumb)net_part(hosta))) != NETA_DIAL)
		goto nonum;
	/*
	 * string is either in map buffer or in extension
	 *	if neither, punt!
	 */
	if ((x = imp_part(hosta)) < _netmap->map_ndial)
		p = &_dialbuf[x];
	else if ((x -= _netmap->map_ndial) < _dialcnt)
		p = &_dialext[x];
	else
		goto nonum;
	/*
	 * copy it into static space and return
	 */
	strcpy(buf, p);
	_freemap();
	return buf;
nonum:
	_freemap();
	return NULL;
}
