#include "ctype.h"
/*
 * return true is string is pure numeric address
 */
_isnum(name)
char	*name;
{
	register int c;
	register char *sp;

	for (sp = name; c = *sp; ++sp)
		if (!isdigit(c) && c != '/')
			return 0;
	return 1;
}
