#include "/rnd/borden/h/iobuf.h"

peekc(iob)
struct iobuf *iob;
{
	register c;
	register struct iobuf *p;

	p = iob;
	if((c = getc(p)) >= 0) {
		p->b_nleft++;
		p->b_nextp--;
	}
	return(c);
}
