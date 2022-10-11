#include "netlib.h"
#include "ctype.h"
/*
 * parse a network or host name string
 *	skip leading white space
 *	convert to lower case
 *	truncate to NETNAMSIZ characters
 *	return pointer to next char
 */
char *
_1parse(sname, tname)
char	*sname;
char	tname[NETNAMSIZ+1];
{
	register char *sp, *tp;
	register int c, nc;

	sp = sname;
	while ((c = *sp) && c == ' ' || c == '\t')
		++sp;
	tp = tname;
	nc = NETNAMSIZ;
	while ((c = *sp) && (isalnum(c) || c == '-' || c == '/')) {
		if (nc-- > 0)
			*tp++ = isupper(c)? tolower(c) : c;
		++sp;
	}
	*tp = '\0';
	return sp;
}
