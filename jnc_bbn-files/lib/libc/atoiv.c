/*
 * C library - atoiv: get an int
 * from a string and return it where the second argument
 * points (with pointer to next char returned directly).
 * Atoint is like atoiv except third argument
 * is base if non-zero (0 defaults to octal on leading zero, else decimal).
 */

char *atoiv(as, ap)
char *as;
int *ap;
{
	extern char *atoint();
	return(atoint(as, ap, 0));
}

char *atoint(as, ap, ab)
char *as;
int *ap, ab;
{
	register char *i;
	register int n, base;
	int neg;

	for(i = as; *i == ' ' || *i == '\t'; i++);
	for(neg = 0; *i == '-'; i++) neg++;
	if (ab) base = ab;
	else base = (*i == '0') ? 8 : 10;
	for(n = 0; *i >= '0' && *i <= '9'; i++) n = (n * base) + (*i - '0');
	if (neg) n = -n;
	*ap = n;
	return(i);
}

