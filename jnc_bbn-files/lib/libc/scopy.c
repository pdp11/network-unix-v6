/* -------------------------- S C O P Y ----------------------------- */
/*
 * str = scopy(str1, str2, &str2[lastbyte])
 * Copies str1 into str2, truncating if necessary. Returns a pointer to the
 * null. The simplest way to use this function is as follows:
 *     scopy(src, dest, 0);
 * which copies without regard for the length of dest, overwriting it if it
 * is not big enough. If you want to copy and check, do
 *     scopy(src, dest, &dest[sizeof dest-1]);
 * that is, supply a pointer to the last byte of dest. scopy will go no further.
 * If src does not fit, it will be truncated. The last byte of dest will still
 * contain a null.
 * The last way to use scopy is for concatenating strings together. This is what
 * the pointer to the null is for. Use:
 *     dest = scopy(src, dest, &dest[DESTSIZE-1]);
 * repeatedly, just changing the src string. Each successive scopy will move
 * the pointer dest to point farther down the string.
 *
 * Note that scopy handles overlapping strings correctly.
 */
char *
scopy(s1, s2, s3)
    char *s1;
    char *s2;
    char *s3;
{
    register char *p1;
    register char *p2;
    register char *p3;
    char *retval;

    p1 = s1;
    p2 = s2;
    p3 = s3;

    if (p3 == 0)
	p3 = ~0;	/* Highest possible address */

    if (p2 <= p1)
    {
	while (p2 < p3 && *p1 != '\0')
	    *p2++ = *p1++;
/* Either p2 == p3 or p1 points at null byte. Either way, terminate. */
	*p2 = '\0';
	return(p2);
    }
    else /* Since strings might overlap, copy in the other direction. */
    {
	while (p2 < p3 && *p1 != '\0')
	{
	    p1++;
	    p2++;
	}
	*p2 = '\0';
	retval = p2;
	p3 = s1;
	while (p1 > p3)
	    *--p2 = *--p1;
	return(retval);
    }
}
