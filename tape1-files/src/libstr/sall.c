#include "string.h"

/*
 * Initial area for auto strings. It's just the base; it has
 * no room for data. It is initialized to no previous area.
 */
struct {
    struct sbase i_base;
    char i_top;     /* Dummy - used for addresses in _ScB below */
}
_Sbuf
={
    { NULL, NULL, NULL, NULL }
};

/*
 * String area control structure.
 */
struct sctrl _ScB
={
    (char *)&_Sbuf.i_top,	    /* c_sp */
    (char *)&_Sbuf.i_base.b_marker, /* c_sfp */
    (char *)&_Sbuf.i_top,	    /* c_top */
    INIT_ALLOC		    /* c_bsize */
};

/* -------------------------- _ S A L L ----------------------------- */
/*
 * _Sall(size) obtains an area big enough to hold a thing
 * of length size and initializes it.
 */
_Sall(size)
    int size;
{
    register struct sbase *sbp;
    register int len;
    extern char *malloc();

    if (_ScB.c_bsize == 0)
	_serror("Need more core!\n");
    len = size + sizeof(*sbp);	 /* Thing plus overhead */
    if (len < _ScB.c_bsize)
	len = _ScB.c_bsize;
    len = alignup(len);

    sbp = (struct sbase *)malloc(len);

    if (sbp == NULL || sbp == (struct sbase *)-1)	/* Accept either alloc */
	_serror("No more address space.\n");

    sbp->b_marker = NULL;
    sbp->b_top = _ScB.c_top;
    sbp->b_sfp = _ScB.c_sfp;

    sbp->b_sp = _ScB.c_sp;
    _ScB.c_sfp = (char *)&sbp->b_marker; /* No stack frame yet */
    _ScB.c_sp = (char *)(sbp + 1);	/* Point at first avail byte */
    _ScB.c_top = (char *)sbp;
    _ScB.c_top += len;		    /* Point at first unavail byte */
}
