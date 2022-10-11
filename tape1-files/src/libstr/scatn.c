#include "string.h"
extern struct sctrl _ScB;
/*
 * str = scatn(s1, s2..., 0);
 * Returns a new auto string which is the concatenation of all its args.
 * -1 is accepted in place of 0, although obsolete.
 */
char *
scatn(args)
    char *args;
{
    register char **argp;
    char *dest;
    register char *d;
    register char *src;
    char **lastp;
    int len;
    extern int slength();

    lastp = &args;
    len = 0;
    for (argp = lastp; *argp != (char *)-1 && *argp != 0; argp++)
	len += slength(*argp);

/* argp now points at last arg (the 0) */

    get(len + 1, dest);

    for (d = dest; lastp != argp; lastp++)
    {
	src = *lastp;
	while (*d++ = *src++)
	    continue;
	d--;
    }
    _ScB.c_sp = d + 1;
    return(dest);
}
