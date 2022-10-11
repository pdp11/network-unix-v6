/*
 * stokeq (string, separators, token)
 * This routine returns true if the string token is the substring
 * of the string string before the first instance of a character in
 * the string separators.   It assumes there are no separators in token.
 */

stokeq (str, ccl, token)
char   *str;
char   *ccl;
char   *token;
{
    register char  *p,
                   *q,
                   *r;

    for (p = str, r = token; *r;)
	if (*p++ != *r++)
	    return (0);
    if (*p == 0)
	return (1);
    for (q = ccl; *q;)
	if (*p == *q++)
	    return (1);
    return (0);
}
