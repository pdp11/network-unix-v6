#
/*
 * DC11 implementation
 * of character handler.
 */
struct
{
	int	rcsr;
	int	rbuf;
	int	xcsr;
	int	xbuf;
};
#define	ADDR	0174140
#define	DONE	0200

mpxintr()
{
	register d;

	if((ADDR->xcsr & DONE) == 0)
		return;
	d = mpx();
	if(d < 0)
		return;
	ADDR->xbuf = d;
}

mprintr()
{
	register d;

	d = ADDR->rcsr;
	if(d < 0)
		printf("mpxer %o\n", d);
	if((d&DONE) == 0)
		return;
	d = ADDR->rbuf & 0377;
	mpr(d);
}

mpinit()
{

	ADDR->rcsr = 0131;
	ADDR->xcsr = 0131;
}
