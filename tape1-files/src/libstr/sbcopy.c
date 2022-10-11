/*
 * char *sbcopy (sourcetop, source, dest, destlow)
 * copy the string source backwards onto the string dest, with destlow
 * as the limit.  If sourcetop is non-zero, copying will start from
 * there in source.  If sourcetop is zero, the end of source will be
 * found by searching for the null.  The pointer returned points to
 * the last character copied into dest, or to dest if nothing was copied.
 * This routine does not insure a null on the end of the resultant string.
 * This routine is similar to scopy.  Passing a zero as the final argument
 * to sbcopy is like passing -1 as the final argument of scopy (there will
 * be no limit on how much can be copied onto dest).
 */

char   *sbcopy (source, sourcelow, dest, destlow)
char   *source,
       *sourcelow,
       *dest,
       *destlow;
{
    register char  *s1,
                   *d1,
                   *d2;

    s1 = source;
    d1 = dest;
    d2 = destlow;
    if (s1 == 0)
	for (s1 = sourcelow; *s1; s1++);
			     /* find end of source string */
    while ((s1 > sourcelow) && (d1 > d2))
			     /* copy backwards */
	*--d1 = *--s1;
    return (d1);
}
