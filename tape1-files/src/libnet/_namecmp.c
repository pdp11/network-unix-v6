#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * search a name list for match
 *	return 1 if name matches exactly
 *	return 0 if no match
 *	return -N if name matches as initial substring N times
 */
_namecmp(name, index)
char	*name;
unsigned index;
{
	register char *lp, *tp;
	int	match;

	match = 0;
	for (lp = &_namebuf[index]; *lp; ++lp) {
		/*
		 * compare name, return 1 if exact match
		 */
		tp = name;
		while (*tp == *lp) {
			if (!*lp++)
				return 1;
			++tp;
		}
		/* count substring matches */
		if (!*tp)
			--match;
		/* advance to end of name */
		while (*lp)
			++lp;
	}
	return match;
}
