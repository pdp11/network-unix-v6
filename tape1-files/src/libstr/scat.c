#include "string.h"
extern struct sctrl _ScB;
/*
 * str = scat(s1, s2);
 * Returns a new auto string which is the concatenation of its two args.
 */
char *
scat(arg1, arg2)
    char *arg1;
    char *arg2;
{
    char *dest;
    register char *d;
    register char *src;
    int len;
    extern int slength();

    len = slength(arg1) + slength(arg2) + 1;

    get(len, dest);

    d = dest;

    src = arg1;
    while (*d++ = *src++)
	continue;
    d--;
    src = arg2;
    while (*d++ = *src++)
	continue;

    _ScB.c_sp = d;
    return(dest);
}
