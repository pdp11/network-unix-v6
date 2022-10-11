#define BBNNET
#define TCPDEBUG
#define TRPT
#include "net.h"
#include "fsm.h"
#include "fsmdef.h"

#define MAXSTATE 13
#define MAXINP   10
#define MAXTIM   5

struct t_debug tcpbuf;
char dname[] = "/etc/net/tcpdebug";
char savetim[30] = "";
extern int errno;
extern char *progname;
int notime = 0;

/*****************************************************************************
*                                                                            *
*      trpt [-ensw] [-f <filename>] [<connection id>]                        *
*                                                                            *
*      trpt prints the contents of the tcp debugging log in readable         *
*      format.  The log consists of entries for each transition of the       *
*      tcp state machine for all enabled connections.  The information       *
*      recorded includes the old and new states of the transition, the       *
*      type of the input that caused it, an identifier for the connection,   *
*      and for network input and output, the tcp sequence and acknowledge-   *
*      ment numbers and packet flags, and a timestamp.                       *
*                                                                            *
*      Options:                                                              *
*              -e -- exit if an invalid log entry is found                   *
*              -w -- suppress invalid log entry diagnostic                   *
*              -s -- print only tcp state info.                              *
*              -f -- use the filename that follows as the log file.          *
*              -n -- print only net input and output                         *
*              -t -- do not print timestamps
*              <connection id> -- print info only for this connection.       *
*      Defaults:                                                             *
*              -f "/etc/net/tcpdebug", print info for all connections.       *
*                                                                            *
*****************************************************************************/

main(argc, argv)
int argc;
char *argv[];
{
	register fd, flags;
        int comp, nowarn, errexit, nomsg, nostate;
	char *fname, *timep;
	char *ctime();
	unsigned long taddr;
	unsigned long ntohl();
	unsigned short ntohs();

	progname = argv[0];
	nostate = comp = nowarn = errexit = nomsg = 0;
	fname = &dname[0];

	while (argc > 1 && argv[1][0] == '-') {
		switch(argv[1][1]){

		case 't':               /* don't print timestamps */
			notime++;
			break;

		case 'f':               /* use next arg as log file */
			fname = argv[2];
			argv++;
			argc--;
			break;

		case 'w':               /* don't print bad format msg */
			nowarn++;
			break;

		case 'e':               /* exit if bad format entry found */
			errexit++;
			break;

		case 's':               /* print only state info */
			nomsg++;
			break;

		case 'n':               /* print only net input and output */
			nostate++;
			break;

		default:
			printf("usage: %s [-enstw] [-f <filename>] [<tcb addr>]\n",
				argv[0]);
			return;
		}
		argv++; argc--;
	}

	if (argc > 1) {
		sscanf(argv[1], "%X", &taddr);
		taddr |= 0x80000000;
		comp++;
	}

	if ((fd = open(fname, 0)) < 0) 
		ecmderr(errno, "Cannot open %s.\n", fname);

	while (read(fd, &tcpbuf, sizeof(tcpbuf)) == sizeof(tcpbuf)) {

		if (comp && (taddr != (unsigned long)tcpbuf.t_tcb))
			continue;

		if (tcpbuf.t_old < 0 || tcpbuf.t_old > MAXSTATE ||
		    tcpbuf.t_inp < 0 || tcpbuf.t_inp > MAXINP ||
		    (tcpbuf.t_inp == ISTIMER && 
			(tcpbuf.t_tim < 0 || tcpbuf.t_tim > MAXTIM)) ||
		    tcpbuf.t_new > MAXSTATE) {
			if (!nowarn)
				printf("Bad format data.\n");
			if (errexit)
				exit(1);
			continue;
		}

		timep = ctime(&tcpbuf.t_tod);
		if (strcmpn(&savetim[0], timep, 13) != 0) {
			printf("\n%s\n", timep);
			strcpy(&savetim[0], timep);
		}
		timep += 11;

		if (tcpbuf.t_new == -1 && tcpbuf.t_inp == INRECV) {
			if (!nomsg) {
				prtime(tcpbuf.t_tcb, timep);
				printf(" OUT: ");
				if (tcpbuf.t_tim != 0) {
        				tcpbuf.t_sno = ntohl(tcpbuf.t_sno);
        				tcpbuf.t_ano = ntohl(tcpbuf.t_ano);
					tcpbuf.t_wno = ntohs(tcpbuf.t_wno);
				        tcpbuf.t_lno = ntohs(tcpbuf.t_lno) - 40;
				}
        			printseg(tcpbuf.t_sno, tcpbuf.t_ano, 
					 tcpbuf.t_lno, tcpbuf.t_wno, tcpbuf.t_flg);
        			if (tcpbuf.t_tim == 0)
        				printf(" (FAILED)\n");
        			else
        				putchar('\n');
			}
			continue;
		} 

		if (!nomsg && tcpbuf.t_inp == INRECV) {
			prtime(tcpbuf.t_tcb, timep);
			printf(" IN: ");
			printseg(tcpbuf.t_sno, tcpbuf.t_ano, tcpbuf.t_lno, 
				 tcpbuf.t_wno, tcpbuf.t_flg);
			putchar('\n');
		}

		if (!nostate) {

			prtime(tcpbuf.t_tcb, timep);
        		printf(" %s X %s",  
        			tcpstates[tcpbuf.t_old], tcpinputs[tcpbuf.t_inp]);
        
        		if (tcpbuf.t_inp == ISTIMER)
        			printf("(%s)", tcptimers[tcpbuf.t_tim]);
        
        		printf(" --> %s", tcpstates[ (tcpbuf.t_new > 0) ? tcpbuf.t_new
        								: tcpbuf.t_old ]);
        
        		if (tcpbuf.t_new < 0)
        			printf(" (FAILED)\n");
        		else
        			putchar('\n');
		}
        
	}
}

printseg(seq, ack, len, win, flags)
unsigned long seq, ack;
short len;
unsigned short win, flags;
{
	printf("seq= %X ack= %X win= %d len= %d flags: ", seq, ack, win, len);
	if (flags & 001)
		putchar('F');
	if (flags & 002)
		putchar('S');
	if (flags & 004)
		putchar('R');
	if (flags & 010)
		putchar('E');
	if (flags & 020)
		putchar('A');
	if (flags & 040)
		putchar('U');
}

prtime(tcb, time)
register long tcb;
register char *time;
{
	if (!notime)
	        printf("%8.8s %X", time, tcb);
	else
		printf("%X", tcb);
}


unsigned long ntohl(lx)
register unsigned long lx;
{
	register unsigned long a;

	a = (lx & 0xff) << 24;
	a |= (lx & 0xff00) << 8;
	a |= (lx & 0xff0000) >> 8;
	a |= (lx & 0xff000000) >> 24;
	return(a);
}

unsigned short ntohs(ls)
register unsigned short ls;
{ 
	register unsigned short a;

	a = (ls & 0xff) << 8;
	a |= (ls & 0xff00) >>8;
	return(a);
}

