#include "string.h"
extern struct sctrl _ScB;
/*
 * str = sauto(maxlen);
 * Allocates a string of maximum length maxlen on the string stack.
 */
char *sauto(maxlen)
    int maxlen;
{
    register char *p;

    get(maxlen + 1, p);
    *p = '\0';	    /* current len = 0 */
    _ScB.c_sp = p + maxlen + 1;
    return(p);
}
