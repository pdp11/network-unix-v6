#define BBNNET
#include "netlib.h"
#include "net.h"
#include "con.h"
#include <sys/types.h>
#include <sys/timeb.h>
#include <signal.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0
extern int errno;
extern char *progname;
struct con con;
struct netstate outstat;
struct timeb stime, ttime;
int timer;
int fr, fw, finis, loop, match, compare;
short wport, rport, host, imp, eol;
char *dir;
char outbuf[1024];
char inbuf[1024];
long bytes, ibytes;
float totals, itotals;

main(argc, argv)
int argc;
char *argv[];
{
	register char last;
	register count, i, j, rdcnt;
	int bufsiz, pid;
	int fd;
	int t_write, t_read, printit;
	int testint(), testurg(), urgent(), report(), die(), timerpt();

	progname = argv[0];

	last = 0xff;
	count = -1;
	itotals = 0.;
	totals = 0.;
	ibytes = 0;
	bytes = 0;
	bufsiz = 512;
	printit = 0;
	fd = -1;

	con.c_mode = CONTCP | CONACT;
	con.c_sbufs = 0;
	con.c_rbufs = 0;
	con.c_timeo = 0;
	con.c_fcon = thishost(thisnet());
	mkanyhost(con.c_lcon);
	con.c_fport = 0;
	con.c_lport = 0;
	wport = 0x1234;
	rport = 0x5678;
	t_write = 1;
	t_read = 1;
	match = 0;
	finis = 0;
	loop = 0;
	eol = 0;
	compare = 0;

	while (argc > 1 && argv[1][0] == '-') {
		switch(argv[1][1]){

		case 'r':               /* read test */
			t_write--;
			t_read++;
			break;

		case 'w':               /* write test */
			t_read--;
			t_write++;
			break;

		case 'd':               /* debug connection */
			con.c_mode |= CONDEBUG;
			break;

		case 'a':               /* await mode */
			con.c_mode &= ~CONACT;
			break;

		case 'h':               /* set foreign host/imp */
			con.c_fcon = gethost(argv[2]);
			argv++; argc--;
			break;

		case 'g':
			con.c_lcon = gethost(argv[2]);
			argv++; argc--;
			break;

		case 'p':               /* set foreign port */
			sscanf(argv[2], "%d", &wport);
			con.c_fport = wport;
			argv++; argc--;
			break;

		case 'q':               /* set local port */
			sscanf(argv[2], "%d", &rport);
			con.c_lport = rport;
			argv++; argc--;
			break;

		case 'x':               /* print after each read/write */
			printit++;
			break;

		case 'b':               /* read/write buffer size */
			sscanf(argv[2], "%D", &bufsiz);
			if (bufsiz > 1024)
				ecmderr(0, "buffer size > 1024\n");
			argv++; argc--;
			break;

		case 'c':               /* send/rcv buffer allocation */
			sscanf(argv[2], "%d", &host);
			con.c_sbufs = host;
			con.c_rbufs = host;
			argv++; argc--;
			break;

		case 't':               /* open timeout */
			sscanf(argv[2], "%d", &host);
			con.c_timeo = host;
			argv++; argc--;
			break;

		case 'l':               /* looping test */
			loop++;
			break;

		case 'm':               /* match read/write size mismatch */
			match++;
			break;

		case 'e':               /* send eols */
			eol++;
			break;

		case 'z':               /* comparison test */
			compare++;
			break;

		case 'i':		/* report every n seconds */
			timer = 30;
			if (argc > 1) {
				timer = atoi(argv[2]);
				argv++; argc--;
			}
			break;

		case 'f':               /* output file */
			if (argc > 1) {
				if ((fd = creat(argv[2], 0666)) < 0)
					ecmderr(errno, "Cannot create %s.\n",
						argv[2]);
				argv++; argc--;
			} else
				printf("No output file name given.\n");
			break;

		case '?':
			goto doc;

		default:
			printf("usage: %s [-rwadelmxz?] [-g <netaddr>] [-h <netaddr>] [-p <fport>] [-q <lport>]\n", 
				argv[0]);
			printf("\t\t[-b <bufsiz>] [-c <conbufs>] [-t <timeout>] [<count>] [-i <intvl>]\n");
			return;
		}
		argv++; argc--;
	}

	if (argc > 1) {
		if (argv[1][0] == '?') {
doc:			printf("TCPTEST OPTIONS:\nopt\tdescription\t\tdefault\n");
			printf("a\tawait mode\t\toff\n");
			printf("b\tset buffer size\t\t512\n");
			printf("c\tset buffer alloc\t1\n");
			printf("d\tdebug mode\t\toff\n");
			printf("e\teol mode\t\toff\n");
			printf("f\tcreate output file\tnull\n");
			printf("g\tset local host\t\tlocal\n");
			printf("h\tset foreign host\tlocal\n");
			printf("i\treport every n secs\t30\n");
			printf("l\tloopback mode\t\toff\n");
			printf("m\trept r/w len mismatch\tno\n");
			printf("p\tset foreign port\t0\n");
			printf("q\tset local port\t\t0\n");
			printf("r\tread test\t\ton\n");
			printf("t\tset open timeout\t30\n");
			printf("w\twrite test\t\ton\n");
			printf("x\tprint after r/w\t\toff\n");
			printf("z\tcompare buffers\t\toff\n");
			return;
		}
		count = atoi(argv[1]);
	}

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, testint);       
	signal(SIGQUIT, report);           
	signal(SIGURG, urgent);
	signal(SIGTERM, testurg);
	signal(SIGHUP, die);
	signal(SIGALRM, timerpt);

	for (i=0, j=0; i < bufsiz; i++)
		outbuf[i] = j++;

	if (t_read && !t_write)
		goto readit;

	if (t_read && t_write)
		switch (pid = fork()) {

		case -1:
			ecmderr(errno, "cannot fork.\n");

		default:
			break;

readit:		case 0:
			dir = "read";
			if (loop) {
               		       con.c_lport = rport;
               		       con.c_fport = wport;
				con.c_mode &= ~CONACT;
			}
        		if ((fr = open("/dev/net/anyhost", &con)) < 0) 
        			ecmderr(errno, "read open error.\n");
			if (timer > 0)
				alarm(timer);

			if (count >= 0)
        			rdcnt = count * bufsiz;
			else
				rdcnt = 1;

			while (rdcnt > 0 && !finis) {

				if (fd >= 0)
        				for (i=0; i < bufsiz; i++)
        					inbuf[i] = 0;

				ftime(&stime);
				if ((i = read(fr, inbuf, bufsiz)) < 0) {
					if (errno == 35) {
        					ioctl(fr, NETGETS, &outstat);
						printf("read status: %o\n",
							outstat.n_state);
					} else if (errno != EINTR) {
						cmderr(errno, "read error.\n");
        					finis++;
					}

				} else if (i > 0) {
        				ftime(&ttime);
					if (i != bufsiz && match) 
					        printf("read %d: length mismatch -- asked %d got %d\n",
						 count, bufsiz, i);
					else
					        if (printit)
						        printf("read %d: %d\n",
							  count,i);

					if (compare) 
						for (j=0; j < i; j++)
							if (inbuf[j] != ++last) {
								printf("read %d: comparison test failed at position %d char %x last %x\n",
								  count, j, inbuf[j], last);
								last = inbuf[j];
								break;
							}

					if (fd >= 0)
						if (write(fd, inbuf, i) < 0)
							cmderr(errno, "Error writing output file.\n");
					totals += (float)(ttime.time - stime.time) +
						  (float)(ttime.millitm - stime.millitm) / 1000.;
					itotals += (float)(ttime.time - stime.time) +
						  (float)(ttime.millitm - stime.millitm) / 1000.;
					ibytes += i;
        				bytes += i;
					if (count-- >= 0)
        					rdcnt -= i;
				} else {
					printf("read: got EOF\n");
					finis++;
				}

			}
			close(fr);
			dorept(dir, totals, bytes, FALSE);
		        exit(0);
		}

	if (t_write) {

		dir = "write";
		if (loop) {
        		con.c_lport = wport;
        		con.c_fport = rport;
			sleep(1);
		}
		if ((fw = open("/dev/net/anyhost", &con)) < 0)  
			ecmderr(errno, "write open error.\n");
		if (timer > 0)
			alarm(timer);

		if (eol)
			ioctl(fw, NETSETE, 0);

		while (!finis && count != 0) {

			ftime(&stime);
			if ((i = write(fw, outbuf, bufsiz)) < 0) {
				if (errno == 35) {
					ioctl(fw, NETGETS, &outstat);
					printf("write status: %o\n",
						outstat.n_state);
					continue;
				} else if (errno != EINTR) {
					cmderr(errno, "write error.\n");
					finis++;
				}

			} else {
        			ftime(&ttime);
				if (i != bufsiz && match)
				        printf("write %d: length mismatch -- tried %d wrote %d\n",
     					     count, bufsiz, i);
        			else  
        				if (printit)
        					printf("write %d: %d\n", count, i);
				totals += (float)(ttime.time - stime.time) +
					  (float)(ttime.millitm - stime.millitm) / 1000.;
				itotals += (float)(ttime.time - stime.time) +
					  (float)(ttime.millitm - stime.millitm) / 1000.;
        			ibytes += i;
        			bytes += i;
			}

			if (count > 0 && --count <= 0)
				finis++;

		}
		close(fw);
		printf("write done.\n");

		if (t_read)
			wait(0);
		dorept(dir, totals, bytes, FALSE);
	}
}

testint()
{
	printf("Interrupt...\n");
	finis++;
}

testurg()
{
	signal(SIGTERM, testurg);
	printf("Urgent test...\n");
	ioctl(fw, NETSETU, 0);
}

urgent()
{
	signal(SIGURG, urgent);
	printf("Received urgent data\n.");
}

report()
{
	signal(SIGQUIT, report);
	dorept(dir, totals, bytes, FALSE);
}

dorept(title, totals, bytes, brief)
char *title;
float totals;	
int bytes, brief;
{
	long thru;

	printf("%s: ", title);
	if (!brief)
		printf("%D bytes in %6.3f seconds", bytes, totals); 
	if (totals == 0.0)
		putchar('\n');
	else {
		thru = (bytes/totals)*8;
		printf(" %D bits/sec\n", thru);
	}
}

timerpt()
{
	static char title[24];

	signal(SIGALRM, timerpt);
	if (timer > 0)
		alarm(timer);
	sprintf(title, "int %s", dir);
	dorept(title, itotals, ibytes, TRUE);
	sprintf(title, "avg %s", dir);
	dorept(title, totals, bytes, TRUE);
	itotals = 0;
	ibytes = 0;
}

die()
{
	finis++;
}
