#define	LIBN
#include "netlib.h"
#include "netmap.h"
#define	NULL	0
/*
 * convert network address to host name
 */
char *
hostname(addr)
netaddr	addr;
{
	register char *np;

	return (np = hostsyn(addr, 0))? np : NOHOSTNAME;
}

/*
 * return nth synonym associated with network address
 *	0 is primary, 1 - N are aliases
 */
char *
hostsyn(addr, syn)
netaddr	addr;
{
	register host_ent *hp;
	register char *lp;
	static char name[NETNAMSIZ+1];

	if (!_loadmap())
		return NULL;
	*name = '\0';
	if (hp = _ahostp(addr)) {
		lp = &_namebuf[hp->host_name];
		while (*lp && syn > 0) {
			while (*lp)
				++lp;
			++lp, --syn;
		}
		strcpy(name, lp);
	}
	_freemap();
	return *name? name : NULL;
}
