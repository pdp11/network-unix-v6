#define BBNNET
#include "netlib.h"
#include "net.h"
#include "con.h"

long tcbaddr;
struct con con;
extern errno;

main(argc, argv)
int argc;
char *argv[];
{
	register fr;

	if (argc <= 1)
		ecmderr(0, "usage: %s <tcb>...\n", argv[0]);

	con.c_mode = CONCTL;
	con.c_sbufs = 0;
	con.c_rbufs = 0;
	con.c_timeo = 0;
	mkanyhost(con.c_fcon);
	mkanyhost(con.c_lcon);
	con.c_fport = 0;
	con.c_lport = 0;

	if ((fr = open("/dev/net/anyhost", &con)) < 0) 
        	ecmderr(errno, "Can't access network.\n");

	while (argc > 1) {
		sscanf(argv[1], "%X", &tcbaddr);
		if (ioctl(fr, NETDEBUG, (tcbaddr | 0x80000000)) < 0)
			cmderr(errno, "%X failed.\n", tcbaddr);
		argc--; argv++;
	}
}
