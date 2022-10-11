#define	LIBN
#include "netlib.h"
/*
 * parse a host name with optional leading or trailing network name
 *
 * type:	parsing control
 *	< 0 implies leading (SRI) net name
 *	= 0 implies host name only
 *	> 0 implies trailing (BBN) net name
 */
char *
_2parse(name, type, hname, nname)
char	*name;
char	hname[NETNAMSIZ+1];
char	nname[NETNAMSIZ+1];
{
	register char *sp;

	sp = name;
	/*
	 * if SRI format, get leading network name
	 */
	if (type < 0) {
		sp = _1parse(sp, nname);
		if (*sp != ' ' && *sp != '\t') {
			sp = name;
			*nname = '\0';
		}
	}
	/*
	 * get host name
	 */
	sp = _1parse(sp, hname);
	/*
	 * if BBN format, get trailing host name
	 */
	if (type > 0) {
		while (*sp == ' ' || *sp == '\t')
			++sp;
		if (*sp == ',')
			sp = _1parse(sp + 1, nname);
		else
			*nname = '\0';
	}
	while (*sp == ' ' || *sp == '\t')
		++sp;
	return sp;
}
