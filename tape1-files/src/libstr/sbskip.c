/*
 * char *sbskip(stringtop, string, separators)
 * Skip all adjacent occurences of characters in separators starting
 * at stringtop and working backwards not past string.  If stringtop
 * is zero, the top of the string will be found by looking forward
 * from string for the null.  This routine is similar to sskip.
 * The pointer returned points to the character immediately after
 * the first character not in separator.
 */

/* [jp 18-Feb-81] Made to behave correctly.  [The search used to skip over
   characters which matched EVERY character in `separators', rather than ANY
   of the separators.]
 */

char   *sbskip (str, strlow, ccl)
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
    for (r = strlow; p > r;) {
	--p;
	for (q = ccl; *p != *q; q++) {
	    if (*q == 0)
		return (p + 1);
	}
    }
    return (r);
}
