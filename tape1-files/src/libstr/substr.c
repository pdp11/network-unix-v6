#include "string.h"
extern struct sctrl _ScB;
/*
 * Returns a new auto string which is a copy of the first len chars of s1. A
 * negative len is "rounded" to 0; if len specifies a substring extending
 * beyond the end of s1, only the portion of the string within s1 is returned.
 * If s1 is zero-length, a zero-length string will be returned.
 */
char *
substr(str, len)
    char *str;
    int len;
{
    int srclen;
    register char *p1, *p2, *s;
    char *dest;
    extern int slength();

    if (len <= 0)
	return("");
    srclen = slength(str);
    if (len > srclen)
	len = srclen;

    get(len + 1, dest); /* Assure room for result */

    s = str;
    for (p1 = s, p2 = dest; len > 0; len--)
	*p2++ = *p1++;

    *p2 = '\0'; 	    /* Null byte */
    _ScB.c_sp = p2 + 1;     /* Next byte after ending null */

    return(dest);
}
