#include "string.h"
extern struct sctrl _ScB;
/*
 * sbegin();
 * Starts off a string storage stack frame. All it does is remember
 * the auto string stack level.
 */
sbegin()
{
    register struct sframe *p;

    _ScB.c_sp = (char *)alignup((int)_ScB.c_sp); /* Round up to word boundary */
    get(2, p);			/* Assure room for backchain */
    p->prev_sfp = _ScB.c_sfp;	/* Push current stack frame ptr */
    _ScB.c_sfp = (char *)p;	/* Point at it */
    _ScB.c_sp = (char *)(p + 1); /* Point sp at first avail word */
}
