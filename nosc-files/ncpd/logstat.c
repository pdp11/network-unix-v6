#

/*	logstat.c	*/
/*	globals declared in this file:
		NONE.

	functions declared in this file:
		log_time
		log_asc
		log_err
		log_bin
		log_init
		statist
		wr_stat
*/

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_logstat[] "~|^`logstat.c\tV3.9E0\tJan78\n";

/*name:
	log_time

function:
	prints the date and time on the standard output.

algorithm:
	calls time to get the standard sytem time.
	converts it via ctime.
	prints the resulting string via printf.

parameters:
	none.

returns:
	nothing.

globals:
	none.

calls:
	time	(sys)
	ctime	(sys)
	printf	(sys)

called by:
	log_err
	log_bin
	log_asc

history:
	intial coding 12/27/74 by G. R. Grossman
	simplified 4Jun77 by J. S. Kravitz

*/
log_time()
{
	long t;			/* gets the result from time */

	time(&t);		/* get system time */
	printf ("%-16.16s", ctime(&t)+4);	/* Mmm dd hh:mm:ss */
}

/*name:
	log_asc

function:
	logs an ascii string.

algorithm:
	writes time in the log via log_time.
	writes the string followed by a new line on the standard output
	via printf.

parameters:
	s	pointer to a nul-terminated ascii string.

returns:
	nothing.

globals:
	none.

calls:
	log_time
	printf	(sys)

called by:
	many functions.

history:
	initial coding 12/28/74 by G. R. Grossman

*/

log_asc(s)
char	*s;
{
	log_time();		/* put time in log */
	printf("%s\n",s);	/* print string followed by a new line */
}

/*name:
	log_err

function:
	logs the current system error message.

algorithm:
	logs the time via log_time.
	prints the current system error message and a calller-supplied
	string via perror.

parameters:
	s	pointer to a nul-terminated ascii string to be passed to
		perror.

returns:
	nothing.

globals:
	errno	which is supplied by the system and used by perror.

calls:
	log_time
	perror	(sys)

called by:
	many functions.

history:
	initial coding 12/28/74 by G. R. Grossman

*/

log_err(s)
char	*s;
{
	log_time();
	perror(s);	/* pass calller's string to perror */
}

/*name:
	log_bin

function:
	logs a string and a vector of bytes.

algorithm:
	logs time via log_time.
	prints the string on the standard output via printf.
	prints the bytes of the vector in octal, 8 per line, via printf.
	duplicate lines are lined up under previous line.

parameters:
	s	pointer to a nul-terminated header string to be written in
		the log.
	v	pointer to a vector of bytes whose octal representation is
		to be written in the log.
	n	number of bytes of v whose octal representation is to be
		written in the log.

returns:
	nothing.

globals:
	none.

calls:
	log_time
	printf	(sys)

called by:
	many functions.

history:
	initial coding 12/28/74 by G. R. Grossman
	modified 4Jun77 by J. S. Kravitz to move printed octal
	numbers to the right of the :, rather than under it

*/

log_bin(s,v,n)
char	*s,
	*v;
int	n;
{
	register int	i,	/* counts bytes per line */
			nb;	/* counts total bytes printed */
	register char	*p;	/* points to bytes to be printed */

	log_time();		/* log the time */
	printf("%25.25s:",s);	/* log the string and ":" */
	p = v;			/* point at vector to be printed */
	nb = n;			/* get count of bytes to be printed */
	while ( nb > 0 )		/* loop while bytes to print */
	{
		for ( i = 0 ; nb && (i<8) ; i++ )	/* loop while bytes to
							   print and have
							   printed <16 */
		{
			printf("%3o ",(*p++ & 0377));	/* print a byte */
			--nb;			/* decrement byte count */
		}
		/*
		 * terminate with new line. if there is to be more
		 * printed, space out for next line
		 */
		printf("\n%s", nb?"\t\t\t....\t\t  ":"");
	}
}

/*name:
	log_init

function:
	intialize the log file

algorithm:
	write the process ID of the largedaemon on a line by itself
	write a log header message

parameters:
	none

globals:
	none

calls:
	log_asc		to write the header message
	log_time	to timestamp the pid line
	printf		to write the process ID
	get_pid (sys)	to get the process ID

called by:
	main

history:
	initial coding 3/2/77 by S.M. Abraham
	Log message modified 9May77 JSK
	process id put on timestamped line after init line 
		01Feb78 J.S.Goldberg
*/

log_init()
{
	/* NOTE: netstat DEPENDS on the first line of the 	*/
	/*	log file containing only the (decimal) number	*/
	/*	of the daemon's pid.  leave it like this (ecch)	*/
	/* p.s., netstat may not be the only one!		*/
	printf("%d\n", getpid() );
	log_asc("Unix NCP Initialization");
	log_time();
	printf("Daemon process number: %d\n",getpid());
}

/*name:
	statist

function:
	increments a counter in the statistics vector and updates the log
	when necessary.

algorithm:
	increments the given address; if this overflows (to zero):
		decrements the counter.
		writes the status vector out via wr_stat (which also
		clears it).

parameters:
	addr	address of a counter in the statistics structure measure.

returns:
	nothing.

globals:
	measure=

calls:
	wr_stat

called by:
	many functions.

history:
	initial coding 12/28/74 by G. R. Grossman

*/

statist(addr)
int	*addr;
{
	if ( ++(*addr) == 0 )		/* does incrementing overflow
					   the counter? */
	{
		--(*addr);		/* decrement it */
		wr_stat();		/* write and clear statistics vector */
	}
}

/*name:
	wr_stat

function:
	writes the accumulated statistics in the log and zeroes the counters.

algorithm:
	currently unimplemented.

parameters:

returns:

globals:

calls:

called by:

history:
	initial coding 1/8/75 by G. R. Grossman

*/

wr_stat()
{
}

