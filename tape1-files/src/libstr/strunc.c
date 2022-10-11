#include "string.h"
extern struct sctrl _ScB;
/*
 * In many circumstances, one would like to collect as much data as possible
 * into a string, then free up whatever is not used. This is the purpose of
 * this routine. After filling the last string allocated, use it to set
 * its maximum length to its current length, permitting the remainder to be
 * used for other auto strings. If it is called on a string which was
 * not the last auto string to be allocated, its action is undefined.
 * (This version does nothing if the string does not start in the current
 * stack frame; otherwise it truncates the frame back to the current length
 * of the string.)
 */
strunc(s)
    char * s;
{
    register char * se;

    se = s;
    if (se >= _ScB.c_sp || se <= _ScB.c_sfp)
	return;     /* Not in current stack frame, ignore */
    while (*se++ != '\0')
	continue;

/* Note that se points to the byte AFTER the null */

    _ScB.c_sp = se;
}
