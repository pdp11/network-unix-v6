#include "string.h"
extern struct sctrl _ScB;
/*
 * Ends (releases) a string storage stack frame. If the previous
 * stack frame ptr was NULL, then this frame is a continuation from
 * a previous area; the current area is freed and the previous one
 * made current.
 *
 * The free routine is handed both the address of the area and its
 * size, providing a hook for use by future memory management routines.
 */
send()
{
    struct sframe *p;
    struct sbase *sbp;
    char * area_base;
    int area_size;

    if (_ScB.c_sfp == NULL)
	_serror("Invalid stack base ptr.\n");

    p = (struct sframe *)_ScB.c_sfp;
    _ScB.c_sfp = p->prev_sfp;
    if (p->prev_sfp != NULL)
    {
	_ScB.c_sp = (char *)p;
	return;
    }

    for (sbp = (struct sbase *)p;
		sbp->b_marker == NULL; sbp = (struct sbase *)_ScB.c_sfp)
    {
	if (sbp->b_sfp == NULL)  /* End of area chain */
	    _serror("send() called without matching sbegin().\n");

	area_base = (char *)sbp;
	area_size = _ScB.c_top - area_base;

	_ScB.c_top = sbp->b_top;
	_ScB.c_sfp = sbp->b_sfp;
	_ScB.c_sp = sbp->b_sp;

	free(sbp, area_size);		      /* Hand it back */
    }
}
