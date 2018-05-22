#

/*	1main.c		*/

#include	"param.h"	/*so we can use errs in user.h*/
#define NO_U	X
#include	"user.h"	/*to get stnd error names*/
#include	"files.h"
#include	"globvar.h"
#include	"probuf.h"
#include	"kread.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_1main[] "~|^`1main.c\tV3.9E1\t09Mar78\n";

/*name:
	main

function:
	initialization and main loop of ARPANet ncp daemon.

algorithm:
	expects daemon communication to be in file descriptor zero
		catch quit signals and direct them to emer_dwn
		catch interrupt signals and direct them to clean_dwn
		catch toggle signals and direct them to toggle_debug
		initialize the log via log_init
		initializes host status file via hs_init
		writes kernel reset(0) which resets kernel for all hosts
		via kw_reset.
		enters main system loop:
			read from kernel.
			initialize flags.
			decode kernel instruction.
			if protocol is to be sent, send it via send_pro.
			check for any server files timing out
		if we fall out of loop, log error and die

parameters:
	none.

returns:
	nothing.

globals:
	k_file=
	kr_bytes=
	pro2send=

calls:
	kw_reset
	kr_dcode
	log_bin
	log_asc
	send_pro
	read	(sys)
	hs_init
	log_init
	kw_timeo

called by:
	started by "smalldaemon" after ncpkernel file has be opened 

history:
	initial coding 12/09/74 by G. R. Grossman.
	modification for new rst initialization 1/17/75 by G. R. Grossman.
	initializing host status file added 5/28/75 by G. R. Grossman
	modification to expect ncpkernel communication in file descriptor
	zero	10/20/75  by  S. F. Holmgren
	modified to call fi_timeo 6/15/76 by S. F. Holmgren
	modified to call kw_timo instead of fi_timeo by greep
	modified to send quit signals to emer_dwn and catch interrupts
		and send them to clean_dwn 1/28/77 by S. M. Abraham
	modified to call log_init 3/2/77, S.M. Abraham
	moved rst logic to when IMP interface logic is recieved
		8/31/77 by Greg Noel
	modified to toggle debugging switches when a signal is recieved
		10/5/77 by Greg Noel
	cosmetic mods to log messages 01Feb78 J.S.Goldberg

*/

main(argc, argv)
int argc;
char **argv;
{
	extern errno;		/* interface to system error codes */
/*#	define EINTR 4		/* error code for interrupted system call */
/*the line above is made unnecessary by including error names from user.h*/

	extern emer_dwn();
	extern clean_dwn();
	extern toggle_debug();

	/* set up to call emer_dwn on quit signals */
	signal (1,1);
	signal (2,1);
	signal (3,1);
	signal (10, &emer_dwn );	/* changed from 4 - flt emt */
	/* set up to call clean_dwn on interrupt signals */
	signal (5, &clean_dwn);
	/* set up to call toggle_debug on signal 11 (segmentation err) */
	signal (11, &toggle_debug);

	/* if debugging requested, turn it on */
	if(argc > 1) toggle_debug();
	close (2);
	dup (1);	/*so perror comes out on re-directed output*/

	log_init() ; /* init the log */

	k_file = 0;	/* make sure k_file looks in zero */
	hs_init();	/* init host status file */
	kw_reset(0);	/* reset all hosts in kernel */
	init_skts();	/* initialize socket structs */

		/* main loop of the ncp daemon */

		/* read until eof or error */
		for(;;) if( (kr_bytes =read(k_file,&kr_buf,1024)) > 0 )
		{
			pro2send = 0;	/* reset protocol transmission flag */
			kr_dcode();	/* go decode and execute
					   instruction from kernel */
			if (pro2send)	/* host-host protocol commands
					   queued up to send? */
				send_pro();	/* send them if so */
			if( ( stimeo || ftimeo ) && (timeo == 0) )
				kw_timo ();	/* set timer */
		} else /* potential eof or error */
			/* exit loop if not interrupted system call -- i.e.,
			   continue if read was interrupted by an external
			   kill that elected not to quit */
			if(kr_bytes != -1 || errno != EINTR) break;
		/* end of main loop */

		/* the kernel file has closed unexpectedly, so dump  */
		die("main: Kernel file closed unexpectedly");
}

/*name:
	die

function:
	fault termination of the ncp daemon. logs a message and
	terminates with a bus error to force dump.

algorithm:
	log message via log_asc.
	force bus error by storing into location 1.

parameters:
	s	pointer to ascii string to be logged as cause of
		termination.

returns:
	never.

globals:
	none.

calls:
	hs_update
	log_asc

called by:
	many routines.

history:
	initial coding 1/7/75 by G. R. Grossman

*/

die(s)
char	*s;
{
	register int	*odd;	/* will contain odd address to force
				   bus error */

	hs_update(0,host_dead) ; /* mark us dead */
	log_asc(s);	/* log the cause of termination */
	odd = 1;	/* point at odd address */
	*odd = 0;	/* force bus error */
}

/*name:
	emer_dwn

function:
	take the network down in an emergency and produce a core image

algorithm:
	call daemon_dwn to to cleanly take down the network,
	and call "die" to log the occurence and produce a core dump

parameters:
	none

returns:
	since it terminates the ncpdaemon, it doesn't return

globals:
	none

calls:
	daemon_dwn	to take the network down
	die		to log the occurence and produce a core dump

called by:
	quit signal sent to the largedaemon

history:
	initial coding 1/28/77 by S. M. Abraham
*/

emer_dwn()
{
	daemon_dwn(); /* take the network down */
	die("emer_dwn: Received a quit signal");
}
/*name:
	clean_dwn

function:
	cleanly takes down the network upon interception of interrupt signal

algorithm:
	take the network down by calling daemon_dwn
	log reason why net was taken down
	exit (thereby terminating the largedaemon)

parameters:
	none

returns:
	0	as status to the exit system call

globals:
	none

calls:
	daemon_dwn	to take the network down
	log_asc		to log reason
	exit (sys)	to terminate the largedaemon process

called by:
	interrupt signal sent to the largedaemon

history:
	initial coding 1/28/77 by S. M. Abraham
*/

clean_dwn()
{
	daemon_dwn(); /* take down the network */
	log_asc("Received an interrupt signal") ; /* log reason */
	exit(0); /* terminate the largedaemon */
}
/*name:
	daemon_dwn

function:
	tell the imp we are going down and sleep for awhile
		and 
	clean up any kernel connections

algorithm:
	mark us dead externally (in the host status file)
	send a host going down message to the imp and sleep for awhile
	send a reset (host=0) to the kernel to clean things up
	for all files not in the null state
		tell the kernel to inform any waiting users that the file has
		been closed, and then tell kernel to release the file
	close the kernel file

parameters:
	none

returns:
	nothing

globals:
	k_file

calls:
	hs_update
	hw_dwn
	sleep (sys)
	kw_reset
	kw_frlse
	log_asc
	close (sys)

called by:
	emer_dwn (quit signal to the largedaemon)
	clean_dwn (interrupt signal to the largedaemon)


history:
	initial coding 6/23/76 by S. F. Holmgren.
	modified by Abraham (JSK) to correct order of release/ready
	modified not to log reason for occurence, and to close the
	kernel file  1/28/77, S. M. Abraham
	modified to call hs_update, 3/1/77, S.M. Abraham
	modified to do kw_rdy before kw_frlse, 3/31/77, S.M. Abraham

*/

daemon_dwn()
{
	register struct file *f_p;

	/* mark us dead in the host_status file */
	hs_update(0,host_dead) ;

	/* send a host going down message to the imp */
	hw_dwn();

	sleep( 5 );		/* let simmer 5 mins med. heat */

	/* send a reset all hosts command to the kernel */
	kw_reset( 0 );

	/* tell anyone waiting on any files to go away */
	for( f_p = &files[0]; f_p < &files[nfiles]; f_p++ )
	if( f_p->f_state != fs_null )
	{
		fp = f_p;
		kw_rdy( f_p->f_id,EDDWN );
		kw_frlse();
	}

	close(k_file) ; /* close the kernel file */
}
/*name:
	toggle_debug

function:
	To set and reset the debugging toggles.

algorithm:
	Complement the two toggles.
	Reset the interrrupt

parameters:
	none

returns:
	nothing

globals:
	k_rdebug
	k_wdebug

calls:
	signal  (sys)

called by:
	main
	system in response to interrupt

history:
	Initial coding by Greg Noel on 10/5/77
*/
toggle_debug()
{
	k_rdebug = 1 - k_rdebug;
	k_wdebug = 1 - k_wdebug;
	signal(11, &toggle_debug);
}
