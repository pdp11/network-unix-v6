#include "string.h"
extern struct sctrl _ScB;
/*
 * sinit(addr, len, bsize);
 * Initializing call for storage allocation. addr (must be even) and len
 * (number of bytes, must be at least sizeof sbase) describe the initial
 * memory to be used for string allocation; when that runs out, the alloc
 * (III) library routine will be called for additional blocks of at least
 * bsize bytes. If bsize is 0, alloc will never be called.
 */
sinit(asbp, len, bsize)
    struct sbase *asbp;
    int len;
    int bsize;
{
    register struct sbase *sbp;

    sbp = asbp;
    if (len < sizeof(*sbp))
	_serror("sinit: area size too small.\n");
    if (bsize == 0 || bsize > sizeof(*sbp))
	_ScB.c_bsize = bsize;
    else
	_serror("sinit: bad allocation blocksize.\n");
    sbp->b_marker = NULL;
    sbp->b_sp = NULL;
    sbp->b_sfp = NULL;
    sbp->b_top = NULL;
    _ScB.c_sfp = (char *)&sbp->b_marker;
    _ScB.c_sp = (char *)(sbp + 1);	/* Point at first avail byte */
    len -= sizeof(*sbp);
    _ScB.c_top = _ScB.c_sp + len;  /* Point at first unavail byte */
    _ScB.c_top = (char *)aligndown((int)_ScB.c_top);
}
