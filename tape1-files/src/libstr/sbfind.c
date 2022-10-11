/*
 * char *sbfind(stringtop, string, separators)
 * Find the first occurence of a character in separators starting
 * at stringtop and working backwards not past string.  If stringtop
 * is zero, the top of the string will be found by looking forward
 * from string for the null.  This routine is similar to sskip.
 * The pointer returned points to the character immediately after
 * the found character.
 */

char   *sbfind (str, strlow, ccl)
char   *str,
       *strlow,
       *ccl;
{
    register char  *p,
                   *q,
                   *r;

    p = str;
    if (p == 0)
	for (p = strlow; *p; p++);
    for (r = strlow; p > r;)
    {
	--p;
	for (q = ccl; *q; q++)
	{
	    if (*p == *q)
		return (p + 1);
	}
    }
    return (r);
}
