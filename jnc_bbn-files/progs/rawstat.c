#
#include "../h/param.h"
#include "../h/proc.h"
#include "../h/rawnet.h"
#include "../h/net.h"     /* for n_open */

/*
 *      ncc -sO rawstat.c -ln -lj
 *      This command tells what hostmessageidrange combinations are open
 *      in the rawmsg facility at the moment.  Analogous to netstat.
 */

#define MEM "/dev/mem"          /* to read rawskt's from */
int mfd;                        /* file descriptor for MEM */
struct rawskt rawskt;           /* to read rawskt's into */
				/* every time */
long stolhost();
char *hostname(), *puthost();
#define DOPROC 1        /* define this to get uid and pid */
#ifdef DOPROC
#define UNIXNL "/unix"  /* to get proc address if old gprocs */
struct {        /* namelist structure for digging addresses out of NL */
	char    name[8];
	int     type;
	int     value;
} nl[] {
	"_proc", 0, 0,
	"", 0, 0,
};
char *procadd;          /* the address of proc in the kernel */
#define PAS 80
char parr[PAS][10];     /* log names array for filling by pwread */
int parrsize PAS;
#endif DOPROC

int fout;
main(argc, argv)
char *argv[];
{
	int time;

	fout = dup(1);          /* buffer the output */

	printf("direction\thost\t\t  lomsg himsg ");    /* give header */
		printf("bytes  msgq  qtotal ");
#ifdef DOPROC
		printf("pid uid");
#endif DOPROC
	printf("\n");
	flush();

#ifndef RMI
	err (0,"no RMI in the kernel","");
#endif  RMI

	time = (argc > 1) ? atoi(argv[1]) : 0;

	if (sizeof(r_rawtab) != table(9, 0))
		err (1, "rawtab size mismatch.", "");
	if ((mfd = open (MEM, 0)) < 0) err (0, "can't open", MEM);

#ifdef DOPROC
	if ((procadd = gprocs(1)) == -1) {
	 nlist (UNIXNL, nl); /* find address of the proc array in the kernel*/
	    if (nl[0].type == 0) err (0, "no namelist from", UNIXNL);
	    procadd = nl[0].value;  /* save it in a convenient place */
	}
	if (gprocs(0) != NPROC) err (1, "NPROC mismatch.","");
	pwread();
#endif DOPROC

	do {
		table (9, &r_rawtab[0]);
		table (10, &w_rawtab[0]);
#ifdef DOPROC
		gprocs(proc);   /* get the proc structure */
#endif DOPROC
		printf("\n");
		output(&r_rawtab[0], "read");   /* give info on the read */
		output(&w_rawtab[0], "write");  /* and write connection sides */
		if (time) sleep (time);
	} while (time);
	exit(0);
}

output(rawtab, dir)
struct rawentry *rawtab;
char *dir;
{
	register struct rawentry *rp;
	register int i;
	register char *pp;
	char *z;
	long j;

	for (rp = &rawtab[0]; rp < &rawtab[RAWTSIZE]; rp++) {
		if (rp -> y_rawskt) {
			printf("%1.1s ", dir);
			j = rp -> y_host;
			j = stolhost(j);
			printf("\t%12.12s ", puthost(j, 0));
			printf("%-13.13s ", hostname(j));
			printf("%3.3d %3.3d ",
				rp -> y_lomsg,
				rp -> y_himsg
			);
			getskt(rp -> y_rawskt);
			printf("%06.6o %06.6o %06.6o ",
				rawskt.v_bytes,
				rawskt.v_msgq,
				rawskt.v_qtotal
			);

#ifdef DOPROC
			pp = rawskt.v_proc;
			if (pp >= procadd) {
				pp = (pp - procadd) + (z = &proc[0]);
				printf("%2.3d ",
					pp -> p_pid
				);
				if (parr[i = pp -> p_uid][0] == 0) {
					printf("%3.3d ", i);
				} else {
					printf("%-8.8s", parr[i]);
				}
			} else {
				printf(" no proc");
			}
#endif DOPROC
/*
 *                      printf("\nflags::: %06.6o", rawskt.v_flags);
 */
			if (!(rawskt.v_flags & n_open)) printf("C");
			printf("%s\n",
			    ((!rp -> y_host) &&
			      (rp -> y_lomsg == rp -> y_himsg) &&
				(rp -> y_lomsg == ELSEMSG) &&
				  (rawtab == r_rawtab))
					? "E" : ""
			);
			flush();
		}
	}
	return;
}

err(code, s1, s2)
char *s1, *s2;
{
	printf("%s %s %s\n", s1, s2, code?"Recompile rawstat.":"");
	flush();
	exit(1);
}

getskt(sktp)
char *sktp;
{
	seek(mfd, sktp, 0);
	read(mfd, &rawskt, sizeof(rawskt));
}
