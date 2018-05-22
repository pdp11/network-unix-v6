#include "/rnd/borden/h/iobuf.h"

cputc(chr, iob)
{
	register struct iobuf *ip;
	extern int errno;

	ip = iob;

	errno = 0;
	if(ip->b_fildes) {
			putc(chr, ip);
			if(errno) { perror("Write error"); exit(-1); }
	}
}
