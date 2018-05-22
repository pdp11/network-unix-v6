#define	SAND	7
#define	OOPEN	01
#define	XLOWAT	10
#define	XWAIT	04

struct clist
{
	int	c_cc;
	int	c_cf;
	int	c_cl;
};

struct
{
	char	flag;
	char	chano;
	struct	clist	iq;
	struct	clist	oq;
};

struct
{
	char	rstate;
	char	rchan;
	char	xstate;
	char	xchan;
	char	xcount;
} mps;

mpsr(d)
{
	register c;
	register *p;

	if(mps.rstate == 0) {
		if(d >= 128) {
			mpr(d);
			return;
		}
		mps.rchan = d;
		mps.rstate = 1;
		return;
	}
	c = mps.rchan;
	if(mps.rstate == 1) {
		if(c == 0 || c == 64) {
			mpr(c);
			mpr(d);
			mps.rstate = 0;
			return;
		}
		mps.rstate = -d;
		return;
	}
	mps.rstate++;
	if(mps.rstate == 0) {
		mpr(c);
		mpr(d);
		return;
	}
	p = cptr(c);
	if(p == 0)
		goto err;
	if(p->flag&OOPEN)
		putc(d, &p->iq);
	return;

err:
	mps.rstate = 0;
}

mpsx()
{
	register *p;
	register d;

	if(mps.xstate == 0) {
		d = mpx();
		if(d < 0 || d >= 128)
			return(d);
		if(d == 0 || d == 64) {
			mps.xcount = 0;
			mps.xstate = 2;
			return(d);
		}
		mps.xchan = d;
		mps.xstate = 1;
		return(d);
	}
	if(mps.xstate == 1) {
		p = cptr(invert(mps.xchan));
		if(p == 0)
			goto err;
		d = p->oq.c_cc + 1;
		if(d > SAND)
			d = SAND;
		mps.xcount = 1-d;
		mps.xstate = 2;
		return(d);
	}
	if(mps.xstate == 2) {
		d = mpx();
		mps.xstate = mps.xcount;
		return(d);
	}
	p = cptr(invert(mps.xchan));
	if(p == 0)
		goto err;
	d = getc(&p->oq);
	if((p->flag & XWAIT) && p->oq.c_cc < XLOWAT) {
		p->flag =& ~XWAIT;
		wakeup(&p->oq);
	}
	mps.xstate++;
	return(d);

err:
	mps.xstate = 0;
	return(mps.rchan);
}
