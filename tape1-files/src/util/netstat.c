#include <stdio.h>
#include <pwd.h>
#include "netlib.h"
#define BBNNET
#include <sys/param.h>
#include "net.h"
#include "ucb.h"
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include "tcp.h"
#include <nlist.h>

struct nlist nl[] = {
	{ "_contab" },
	{ "_netcb" },
	{ "_netstat" },
	0,
};

struct ucb ucb;
struct tcb tcb;
struct proc proc;
struct net netcb;
struct net_stat stats;
int dump = 0;

extern int errno;

main(argc, argv)
int argc;
char *argv[];
{
	register i, j, mf, tcp;
	char opts, pstats, mstats, cstats, iaddrs;
	char *sname, *cname;
	char *hp;
	struct passwd *pp;
	long conptr;
	struct passwd *getpwuid();

	opts = pstats = mstats = cstats = 0;
	sname = "/vmunix";
	cname = "/dev/kmem";

  	while (argc > 1 && argv[1][0] == '-') {
		switch(argv[1][1]){

		case 'f':
			if (argc > 2) {
				sname = argv[2];
				argv++; argc--;
				break;
			} else
				goto bad;

		case 'd':
			if (argc > 2) {
				dump++;
				cname = argv[2];
				argv++; argc--;
				break;
			} else
				goto bad;

		case 's':
			pstats++;
			opts++;
			break;

		case 'm':
			mstats++;
			opts++;
			break;

		case 'c':
			cstats++;
			break;

		case 'a':
			pstats++; mstats++; cstats++;
			break;

		case 'h':
			iaddrs++;
			break;

		default:
		bad:
			printf("usage: %s [-achms] [-f <sysname>] [-d <corename>]\n", argv[0]);
			return;
		}
		argv++; argc--;
	}

	nlist(sname, nl);
	if (nl[0].n_type == 0) {
		fprintf(stderr, "no %s namelist", sname);
		exit(1);
	}
	if ((mf = open(cname, 0)) < 0) {
		fprintf(stderr, "cannot open ");
		perror(cname);
		exit(1);
	}
	if (!opts || cstats) {
        	printf("USER       PID MODE  STAT FOREIGN HOST   FPRT LPRT TCB    SB RB SA RA  HI  LO TS\n");
        
		if (!iaddrs)
			loadnetmap();
		nseek(mf, (long)nl[0].n_value, 0);
		read(mf, &nl[0].n_value, sizeof(long));

        	for (j=0; j < NCON; j++) {
        
        		nseek(mf, (long)nl[0].n_value, 0);
        		read(mf, &ucb, sizeof ucb);
        		if (ucb.uc_proc != 0) {
        			nseek(mf, ucb.uc_proc, 0);
        			read(mf, &proc, sizeof proc);
                		if (ucb.uc_tcb != 0) {
                			nseek(mf, ucb.uc_tcb, 0);
                			read(mf, &tcb, sizeof tcb);
                		}
        
				if ((pp = getpwuid(proc.p_uid)) != NULL)
					printf("%-8.8s ", pp->pw_name);
				else
					printf("               ");

				printf("%5u ", proc.p_pid);

                		i = 5;
        			tcp = 0;
                		if (ucb.uc_flags & UTCP) {
        				tcp++;
                			putchar('T');
        			} else if (ucb.uc_flags & UIP)
                			putchar('I');
                		else if (ucb.uc_flags & URAW)
                			putchar('R');
				else if (ucb.uc_flags & UCTL)
					putchar('C');
                		else
                			putchar('?');
                		if (ucb.uc_flags & UEOL) {
                			putchar('P');
                			i--;
                		}
                		if (ucb.uc_flags & UURG) {
                			putchar('U');
                			i--;
                		}
                		if (ucb.uc_flags & RAWERR) {
                			putchar('E');
                			i--;
                		}
                		if (ucb.uc_flags & RAWASIS) {
                			putchar('A');
                			i--;
                		} else if (ucb.uc_flags & RAWVER) {
                			putchar('V');
                			i--;
                		} else if (ucb.uc_flags & RAWCOMP) {
                			putchar('S');
                			i--;
                		}
                		if (ucb.uc_flags & UDEBUG) {
                			putchar('D');
                			i--;
                		}
                		if (ucb.uc_flags & ULISTEN) {
                			putchar('L');
                			i--;
                		}
                		while (i-- > 0)
                			putchar(' ');
                
				printf("%04o ", ucb.uc_state);

				if (!iaddrs) {
					hp = hostname(ucb.uc_host);
					if (seq(hp, "nohost"))
						hp = hostfmt(ucb.uc_host, 1);
					printf("%-14.14s ", hp);
				} else
					printf("%08X       ", ucb.uc_host);

                		printf("%04x %04x %06X %2d %2d %2d %2d %3d %3d %2d\n",
                			(tcp ? tcb.t_fport : 0),
                			(tcp ? tcb.t_lport : 0),
                			(long)ucb.uc_tcb & 0x7fffffff,
                			ucb.uc_ssize,
                			ucb.uc_rsize,
                			ucb.uc_snd,
                			ucb.uc_rcv,
                			ucb.uc_lo,
                			ucb.uc_hi,
                			(tcp ? tcb.t_state : 0));
        		}
        
         		nl[0].n_value += sizeof ucb;
        	}
		if (!iaddrs)
			freenetmap();
	}

	if (mstats) {

		nseek(mf, (long)nl[1].n_value, 0);
		read(mf, &netcb, sizeof netcb);
		printf("pages= %d bufs= %d free= %d hiwat= %d lowat= %d\n",
			netcb.n_pages,
			(netcb.n_pages << 3),
			netcb.n_bufs,
			netcb.n_hiwat,
			netcb.n_lowat);
	}

	if (pstats) {

		nseek(mf, (long)nl[2].n_value, 0);
		read(mf, &stats, sizeof stats);
		printf("imp resets= %d imp flushes= %d imp drops= %d ",
			stats.imp_resets, stats.imp_flushes, stats.imp_drops);
		printf("mem drops= %d\nip badsums= %d tcp badsums= %d tcp rejects= %d tcp unacked= %d\n",
			stats.m_drops, stats.ip_badsum, stats.t_badsum,
			stats.t_badsegs, stats.t_unack);
		printf("icmp drops= %d icmp badsums= %d src quenches= %d redirects= %d\n",
			stats.ic_drops, stats.ic_badsum, stats.ic_quenches, stats.ic_redirects);
		printf("icmp echoes= %d time exceeded= %d\n",
			stats.ic_echoes, stats.ic_timex);
	}
}

nseek(fd, off, how)
int fd, how;
long off;
{
	if (dump)
		off &= ~0x80000000;
	return(lseek(fd, off, how));
}
