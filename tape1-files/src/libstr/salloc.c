/*
 * Return a correctly aligned pointer for
 * use with datatypes other than strings.
 * Size is literal, not off-by-one.
 * Otherwise, just like sauto.
 */

#include "string.h"
extern struct sctrl _ScB;

char *
salloc(size)
    unsigned size;
{
    char *result;
    char *nresult;

    get(size, result);
    if (unaligned((int)result))
    {
	nresult = (char *)alignup((int)result);
	size += nresult - result;
	get(size, result);
    }
    _ScB.c_sp = result + size;
    return(nresult);
}
