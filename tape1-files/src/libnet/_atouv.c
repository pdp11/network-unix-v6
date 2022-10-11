/*
 * some special routines to read numbers
 *	base is octal if leading 0, decimal otherwise
 *	don't bother with sign, everything is positive
 */

/*
 * ascii to long
 */
char *
_atoulv(sp, lp)
register char *sp;
register long *lp;
{
	register int dval;
	register long lval;
	register int base;

	base = (*sp == '0')? 8 : 10;
	for (lval = 0; (dval = digit(*sp, base)) >= 0; ++sp) {
		lval *= base;
		lval += dval;
	}
	*lp = lval;
	return sp;
}

/*
 * ascii to unsigned int
 */
char *
_atouiv(sp, ip)
register char *sp;
register unsigned int *ip;
{
	register int dval;
	register unsigned int ival;
	register int base;

	base = (*sp == '0')? 8 : 10;
	for (ival = 0; (dval = digit(*sp, base)) >= 0; ++sp) {
		ival *= base;
		ival += dval;
	}
	*ip = ival;
	return sp;
}

/*
 * return value of digit d in base b
 */
static
digit(d, b)
register int d, b;
{
	static char s[] = "0123456789abcdef";
	register char *p;

	for (p = s; b-- > 0 && *p; ++p)
		if (*p == d)
			return p - s;
	return -1;
}
