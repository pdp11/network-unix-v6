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

#define	BUFS	30
#define	IDLE	0
#define	START	1
#define	RUN	2
#define	WAIT	3

struct
{
	char	*ep;
	char	*xp;
	char	buf[BUFS];
	char	state;
	char	cks;
	char	time;
	char	ackf;
	char	buff;
} mpxm, mprc;

int	mptimf;

mptime()
{

	mptimf = 0;
	if(mprc.time) {
		mprc.time--;
		if(mprc.time == 0)
			mprc.state = IDLE;
	}
	if(mpxm.time) {
		mpxm.time--;
		if(mpxm.time == 0) {
			mpxm.state = START;
			mpxintr();
		}
	}
	if(mpxm.time || mprc.time)
		mptimer();
}

mptimer()
{

	if(mptimf == 0) {
		mptimf++;
		timeout(mptime, 0, 60);
	}
}

mpxintr()
{
	register char *cp;
	register d;

	if((ADDR->xcsr & DONE) == 0)
		return;
	switch(mpxm.state) {

	case IDLE:
		d = mpsx();
		if(d < 0 && mpxm.ackf == 0)
			return;
		mpxm.buff =^ 0300;
		cp = mpxm.buf+1;
		while(d >= 0) {
			*cp++ = d;
			if(cp >= mpxm.buf+BUFS-1)
				break;
			d = mpsx();
		}
		mpxm.buf[0] = cp-mpxm.buf+1;
		if(mpxm.buff & 0200)
			mpxm.buf[0] =| 040;
		mpxm.ep = cp;

	case START:
		mpxm.xp = mpxm.buf;
		mpxm.cks = 0;
		mpxm.buf[0] =| mpxm.ackf;
		mpxm.ackf = 0;
		mpxm.state = RUN;

	case RUN:
		mpxm.time = 3;
		mptimer();
		if(mpxm.xp < mpxm.ep) {
			d = *mpxm.xp++;
			mpxm.cks =- d;
			ADDR->xbuf = d;
			return;
		}
		ADDR->xbuf = mpxm.cks;
		mpxm.state = WAIT;
	}
}

mprintr()
{
	register char *cp;
	register d;

	d = ADDR->rcsr;
	if(d < 0) {
		d = ADDR->rbuf;
		return;
	}
	if((d&DONE) == 0)
		return;
	/*
	 * recieve the character
	 * and put it into rec buf
	 */
	d = ADDR->rbuf & 0377;
	if(mprc.state == IDLE) {
		mprc.cks = 0;
		mprc.ep = mprc.buf;
		mprc.state = RUN;
	}
	mprc.time = 2;
	mptimer();
	if(mprc.ep < mprc.buf+BUFS) {
		*mprc.ep++ = d;
		mprc.cks =+ d;
	}
	d = mprc.buf[0] & 037;
	if(mprc.ep < mprc.buf+d)
		return;
	/*
	 * at end of data,
	 * if bad checksum, pretend like
	 * it never happened.
	 */
	mprc.time = 0;
	mprc.state = IDLE;
	if(mprc.cks)
		return;
	/*
	 * process data if buffer hasnt
	 * been seen before.
	 */
	cp = mprc.buf;
	d = *cp++;
	if((d&040) != mprc.buff) {
		mprc.buff = d&040;
		mprc.ep--;
		while(cp < mprc.ep)
			mpsr(*cp++ & 0377);
	}
	mpxm.ackf = 0100;
	if(d & 040)
		mpxm.ackf = 0200;
	if(d & mpxm.buff) {
		mpxm.time = 0;
		mpxm.state = IDLE;
	}
	mpxintr();
}

mpinit()
{

	mpxm.buff = 0100;
	mpxm.ackf = 0;
	mpxm.time = 0;
	mpxm.state = IDLE;

	mprc.buff = -1;
	mprc.time = 0;
	mprc.state = IDLE;

	ADDR->rcsr = 0131;
	ADDR->xcsr = 0531;
}
