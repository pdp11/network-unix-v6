#
/*
 * ncc -n -O -s file_use.c
 *
 * file_use wait num
 *
 * If wait is given and non-zero, repeat and sleep wait seconds between.
 * If num is also given, stop after num repeats.
 * Summary is printed after num repeats or on signals 1, 2, 3, or 5.
 * Program continues after signal 5, halts after others.
 * jsq BBN 9-5-78
 */

#include "../h/param.h"
#include "../h/file.h"
#include "../h/inode.h"

#define NL      "/unix"
#define MEM     "/dev/kmem"

struct {        /* namelist structure for digging addresses out of NL */
	char    name[8];
	int     type;
	int     value;
} nl[] {
	"_file",
	0,
	0,
	"_inode",
	0,
	0,
	"",
	0,
	0,
};

int fout;
int cur[4], max[4]; char *min[4]{ -1, -1, -1, -1};
int maxt[4][2], mint[4][2]; float mean[4];
char *ctime();  int finish(), howzit();
int times, wait;
char *name;  int stime[2];

main(argc, argv)
char **argv;
{
	register struct file *f;
	register struct inode *i;
	register count;
	int num, used, tvec[2];

	fout = dup(1); close(0); close(1); close(2);

	nlist (NL, nl); /* find addresses of file and inode */
	if ((nl[0].type == -1) || (nl[1].type == -1))
		err ("no namelist from", NL);
	if(open(MEM, 0) != 0) err("can't open", MEM);

	wait = 0;       /* get arguments: seconds to wait between checks */
	num = -1;       /* and number of checks (-1 means forever) */
	if (argc > 1) wait = atoi (argv[1]);
	if (argc == 3) num = atoi (argv[2]);

	printf("NFILE:\t%d\tNINODE:\t%d\n", NFILE, NINODE);
	printf("used\tcount\tused\tcount\tdate\n");
	flush();        /* because are buffering output, must do this to print */
	time(stime);    /* save time we started */
	times = 0;      /* number of checks done is zero */
	name = argv[0]; /* save name under which program was called */
	signal(1, &finish);     /* on all the obvious stops print summary */
	signal(2, &finish);     /* and exit */
	signal(3, &finish);
	signal(5, &howzit);     /* same on trace, but continue after */
	do {
		seek(0, nl[0].value, 0);        /* get file struct array */
		read(0, file, sizeof(file));
		used = count = 0;
		for(f = &file[0]; f < &file[NFILE]; f++) {
			if (f -> f_count) {
				used++;
				count =+ f -> f_count;
			}
		}
		cur[0] = used; cur[1] = count;

		seek(0, nl[1].value, 0);        /* and inode */
		read(0, inode, sizeof(inode));
		used = count = 0;
		for(i = &inode[0]; i < &inode[NINODE]; i++) {
			if (i -> i_count) {
				used++;
				count =+ i -> i_count;
			}
		}
		cur[2] = used; cur[3] = count;

		time(tvec);     /* save and print check info */
		for(count = 0; count < 4; count++) {
			mean[count] =+ cur[count];
			if (cur[count] < min[count]) {
				min[count] = cur[count];
				mint[count][0] = tvec[0];
				mint[count][1] = tvec[1];
			}
			if (cur[count] > max[count]) {
				max[count] = cur[count];
				maxt[count][0] = tvec[0];
				maxt[count][1] = tvec[1];
			}
			printf("%d\t", cur[count]);
		}
		printf("%.15s\n", ctime(tvec) + 4);
		flush();

		if (wait == 0) finish();        /* if no wait, no repeat */
		times++;                        /* done one more time */
		sleep (wait);
	} while (--num);        /* do no more than num argument times */
	finish();
}

finish() {      /* come here when stopped, if possible */
	if (times == 0) exit(0);/* if no checks, no summary, else summary */
	howzit();
	exit(0);
}

howzit() {      /* howzit goan? */
	register i;
	int tvec[2];

	time(tvec);
	printf("\n%s started at \t%.15s\n", name, ctime(stime) + 4);
	printf("time now is\t\t%.15s\n", ctime(tvec) +4);
	printf("means of %d checks at %d second intervals\n", times, wait);
	printf("NFILE:\t%d\tNINODE:\t%d\n", NFILE, NINODE);
	printf("used\tcount\tused\tcount\n");
	for(i = 0; i < 4; i++) printf("%6.2f\t", mean[i]/times);
	printf("\n\nmaximums:\n");
	for(i = 0; i < 4; i++) printf("%d\t%.15s\n", max[i], 4 + ctime(&maxt[i][0]));
	printf("\nminimums:\n");
	for(i = 0; i < 4; i++) printf("%d\t%.15s\n", min[i], 4 + ctime(&mint[i][0]));
	flush();
	return;
}

err(s1, s2)
char *s1, *s2;
{
	printf("%s %s\n", s1, s2);
	flush();
	exit(1);
}
