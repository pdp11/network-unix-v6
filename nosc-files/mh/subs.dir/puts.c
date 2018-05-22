#include "/rnd/borden/h/iobuf.h"

puts(buf, iob)
{
	register char *cp;
	register struct iobuf *ip;
	extern int errno;

	cp = buf;
	ip = iob;

	errno = 0;
	if(ip->b_fildes)
		while(*cp) {
			putc(*cp++, ip);
			if(errno) { perror("Write error"); exit(-1); }
		}
}
