#/*
Module Name:
	netpr.c -- network printer daemon

Installation:
	if $1x = finalx goto final
	cc -f -c netpr.c
	if ! -r netpr.o exit
	cc -f netpr.o /usr/net/hnumcvt.c -lj
	rm -f netpr.o hnumcvt.o
	exit
: final
	cc -O -f -c netpr.c
	if ! -r netpr.o exit
	cc -O -s -f netpr.o /usr/net/hnumcvt.c -lj
	rm -f netpr.o hnumcvt.o
	if ! -r a.out exit
	su cp a.out /usr/net/etc/netpr
	rm a.out

Synopsis:
	netpr [channel]

Function:
	Unspool print files from the network and turn them over to the
	system line printer spooler.

Restrictions:

Diagnostics:

Files:
	/dev/net/anyhost

See Also:

Bugs:

Modules referenced:
	/usr/net/hnumcnv.c to convert host number to name.

Compile time parameters and effects:

Module History:
*/
#define page_limit 400		/* limit before we try to break up output */

/* structure decs */

#include "/h/net/openparms.h"

struct openparams openparams;

#define o_print		03	/* direct user listen general smplex absolute */

#define temp_file	"/usr/lpd/net_print_temp"

char	buf[512];
int	fdi;
int	fdo;
float	total_bytes;		/* running total for this print file */
float	number_reads; 		/* to compute bytes per read */
int	count_0, count_1, count_2, count_3, count_4, count_5;
int	ivec[2];		/* time vector */
int	tvec[2];		/* termination time	*/


int	status;
int	sleep();
int	time();			/* time of day	*/
char	*ctime();		/* convert to ascii	*/
int	fork();
int	wait();
 
/**/

main(argc, argv)
int argc;
char **argv;
{
	register num_bytes;
	register i;
	register char *p;
	int channel;
/*KLUDGE*/int fdi2;

	extern int fout;
	fout = dup(1);

channel = (argc<2) ? 010 : atoi(argv[1]);
log ("started on channel 0%o\n", channel);

for (;;) {
	openparams.o_type = o_print;
	openparams.o_nomall = 2048;
	openparams.o_lskt   = channel;
	openparams.o_frnhost = 0;

	fdi = open ("/dev/net/anyhost",&openparams);

	if (fdi < 0) {
		log ("unable to open net connection\n");
		continue;
	}

	if(opn_out() < 0) continue;

/*KLUDGE*/fdi2 = dup(fdi); /* so opr's close of fdi won't close netfile */
	log ("start transmission from %s\n", hnumcnv(openparams.o_frnhost&0377));
/**/
	do {
		if((num_bytes = read(fdi,&buf,512)) <= 0) break;

		if(total_bytes > page_limit*512.) {	/* should we try to split printout? */
		    for(i = num_bytes; i--;)
			if(buf[i] == '\014')	/* try to split at a form feed */
			    goto split_it;
		    if(total_bytes > (page_limit+5)*512.) {	/* is it getting serious? */
			for(i = num_bytes; i--;)
			    if(buf[i] == '\n')	/* try to split at a new line */
				goto split_it;
			if(total_bytes > (page_limit+8)*512.) {	/* is it serious? */
			    i = num_bytes / 2;	/* split buffer in middle */
			    goto split_it;
			}
		    }
		}
		continue;

split_it:
		out(++i);		/* output up to split point */
		log ("splitting output stream\n");
		end_out();		/* output it */
		for(p = buf; i < num_bytes;)
			*p++ = buf[i++];	/* shift unprinted portion */
		num_bytes = p - buf;
		if(opn_out() < 0) break;	/* try to open another output file */
	} while (out(num_bytes));

	if(num_bytes < 0) perror("netpr");

	close(fdi);
/*KLUDGE*/close(fdi2);
	end_out();
   }
}
/**/
opn_out()
{
	total_bytes = 0;
	number_reads = 1.0;
	count_0 = count_1 = count_2 = count_3 = count_4 =count_5 = 0;

	fdo = creat (temp_file,0400);
	if (fdo < 0) {
		log ("unable to create an output file\n");
		close (fdi);	/* make sure netfile is closed */
	}
	time(ivec);
	return (fdo);
}
/**/
out(num_bytes)
int num_bytes;
{
	register write_bytes;

	write_bytes = write (fdo,&buf,num_bytes);
	if (write_bytes < num_bytes) {
	     log ("ran out of disk space during a write\n");
	    return (0);
        }

	total_bytes =+ write_bytes;
	number_reads =+ 1.0;
	if (write_bytes > 500) count_5++;
	else if (write_bytes > 400) count_4++;
	else if (write_bytes > 300) count_3++;
	else if (write_bytes > 200) count_2++;
	else if (write_bytes > 100) count_1++;
	else count_0++;
	return (-1);
}
/**/
end_out()
{
	register pid, baud;
float	chars_buf;		/* average chars per buffer read */

	time(tvec);
	close (fdo);

	while ((pid = fork()) < 0) sleep (1);
	if (pid == 0)  {
		execl ("/lib/lpr","-lpr","-r",temp_file,0);
		log ("for some reason the lpr didn't take\n");
		exit();
		}
	while (pid != wait (&status));

	baud = (total_bytes * 8.0) / (tvec[1] - ivec[1]);
	chars_buf = total_bytes / number_reads;
	log ("stats %d %d %d %d %d %d\n",
		count_0, count_1, count_2, count_3, count_4, count_5);
	log ("%.0f chars at %d baud (%.0f)\n",
		total_bytes, baud, chars_buf);
}
log(x,a,b,c,d,e,f)
{
	long t;			/* gets the result from time */

	time(&t);		/* get system time */
	printf ("%-16.16snetpr: ", ctime(&t)+4);	/* Mmm dd hh:mm:ss */
	printf (x,a,b,c,d,e,f);
	flush();
}
