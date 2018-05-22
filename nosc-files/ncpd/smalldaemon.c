/*name:
	smalldaemon

installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc smalldaemon.c
	exit
: newer
	if ! { newer smalldaemon.c /usr/net/etc/smalldaemon } exit
	echo ncpd/smalldaemon.c:
: finale
	cc -O -s smalldaemon.c
	if ! -r a.out exit
	if ! -r /usr/sys/ncpd/smalldaemon.c goto same
	if { cmp -s smalldaemon.c /usr/sys/ncpd/smalldaemon.c } goto same
	su cp smalldaemon.c /usr/sys/ncpd/smalldaemon.c
	su chmod 444 /usr/sys/ncpd/smalldaemon.c
	rm -f smalldaemon.c
: same
	if -r /usr/net/etc/smalldaemon su rm -f /usr/net/etc/smalldaemon
	su cp a.out /usr/net/etc/smalldaemon
	rm -f a.out
	su chmod 544 /usr/net/etc/smalldaemon
	su chown daemon /usr/net/etc/smalldaemon
	su chgrp system /usr/net/etc/smalldaemon

function:
	to open the daemon communication file with a small program
	so that when the call on impopen in ncpopen (ncpio.c) forks
	the program size is tiny.  Hence every input from the imp
	doesnt have to swap in the large daemon program, just this
	little fella

algorithm:
	set software priority up so we get some decent 
	service.  Note owner must be root, and set uid
	bit must be on for this to work.
	did open of /dev/ncpkernel come off
		if id is not zero
			make it zero
		execute bigdaemon

parameters:
	any found are passed through to the large daemon.

returns:
	never

globals:
	none

calls:
	open (sys)
	close (sys)
	dup  (sys)
	execl (sys)

called by:
	when the ncpdaemon wants to be started.

history:
	initial coding 10/20/75 by S. F. Holmgren.
	Center for Advanced Computation, Univ of Illinois
	Patch to insert sleep(10) before exec'ing was 
	empirically arrived at by people at DoD. We add 
	it here because it seems not to do any harm.
	K. Kelley, UofI CSO, Dec 12, 1977.
	JSKravitz, UI/CSO, 25Jan78, took out sbrk (0), because it
	was wrong
	Code added to ignore ordinary hang-up,interupt,and quit signals,
	March 17, 1978. K. Kelley at suggestion of J.S. Kravitz
	Setuid of daemon to DAEMON (1) for prevention of ksys damage.
	K. Kelley, 31 March '78.
	Greg Noel, NOSC, 10July78, added code to pass parameters through to
	largedaemon invocation -- maybe someday it will do something with them.

*/
/* SCCS PROGRAM IDENTIFICATION STRING */
char id_smalldaemon[] "~|^`smalldaemon.c\tV4.0+\tFriMar3116:24:31EST1978\n";

main(argc, argv)
int argc;
char **argv;
{

	int  k_file;	/* file descriptor of opened kernel/daemon file */

	/* set me up so things hop a little when I call */
	nice( -15 );
	setuid(1);	/*so this guy is owned by daemon, to prevent
			"somebody"'s ksys from killing us prematurely...KCK*/
	/* can I open kernel file */
	switch ( k_file = open ("/dev/ncpkernel", 2) )
	{
	default:
		close( 0 );	/* if descriptor wasnt zero */
		dup( k_file );	/* make it zero */
	case 0:
		/* set priority down to normal for big boy */
		nice(0);
		/* rev up the big boy */
		sleep(10);
		/*
		 *      In media res,...
		 *
		 *      Computers are speedy mutha's
		 *      and hence on PDP-11/70's and other such
		 *      we don't have enough time for the imp to reset
		 *      and send out its stuff before the large daemon
		 *      tries to talk.  This race condition seems to
		 *      cause catononic shock to the IMP I/O.
		 *      Netstat will indicate
		 *              In: Master Ready err
		 *              In: Master non DMA
		 *      The console will type
		 *              IMP: Reset
		 *      But the expected
		 *              IMP: Init
		 *      will not occur.
		 *
		 *      Thus it seems that our hero John Codd found out
		 *      and therefore the reason for the sleep(10) on
		 *      the previous page.
		 *
		 *      Dennis L. Mumaugh/Laura Williams/Pat Altomari
		 *      22 Sept 1977
		 */
		/*The smalldaemon should never receive signals 1, 2, or 3.
		  However, it has been known to happen by accident and 
		  hang the system. The following will cause those signals 
		  to be ignored: (J. S. Kravitz/K. Kelley) 
		*/
		signal(1,1);
		signal(2,1);
		signal(3,1);
		argv[0] = "-Largedaemon";  argv[argc] = 0;
		execv( "/usr/net/etc/Largedaemon",argv );
		write(1, "Exec of large daemon failed\n", 28);
		exit (-1);

	case -1:
		write(1, "Unable to open ncp-kernel file\n", 31);
		exit (-1);
	}
}
