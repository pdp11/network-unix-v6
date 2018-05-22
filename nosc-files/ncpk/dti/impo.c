#
#include "../h/param.h"
#include "../h/user.h"
#include "../h/buf.h"
#include "../h/reg.h"
#include "../h/net.h"
#include "../h/netbuf.h"
#include "../h/imp.h"
#include "../h/file.h"
#include "../h/ncp.h"
#include "../h/contab.h"


/* array that holds the rfnm wait bits */
char host_map[32];

/*name:
	impopen

function:
	to initialize the imp and to serve as a basis for
	the main input driving loop for data from the network

algorithm:
	initialize the imp (imp_init)
	start up a kernel process to handle data from the net

parameters:
	none

returns:
	nothing

globals:
	ncpopnstate

calls:
	imp_init
	sleep(sys)
	newproc(sys)
	impldrd
	imp_input

called by:
	ncpopen

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 10/5/75 chopped initialize into imp_init
	and started checking imp_error bit and inited if
	bit was on S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/


impopen()
{

	register int *fp;
	register int *p;
	/* start up kernel input process to handle input */
	if( newproc() )
	{
		/* release any hold we may have on parents files */
		exit0 ();

		imp_proc = u.u_procp;
		HMR_interval = 10;
		set_HMR();
		/* loop around handling input from imp */
		spl_imp();			/* so imp wont interrupt */
		/* initialize the imp */
		imp_init();
		for( ;; )
		{
			spl_imp();
			sleep( &imp,-25 );	/* wait for something */
/*
			if( ncpopnstate == 0 ) break;	/* we down? */
			imp_input();		/* handle it */
		}
		imp_dwn();		/* clean up kernel data */
		u.u_error = EBADF;	/* so return to user will get -1 */
	}
}

/*name:

	imp_init
function:
	to initialize the imp interface hardware, and various software
	parameters.

algorithm:
	set fixed input addresses
	set fixed output addresses
	reset the imp interface
	start strobing the host master ready bit
	enable interrupts
	clear any extended memory address bits
	build and send 3 nops to the imp
	built fixed parameters in the imp input leader

parameters:
	none

returns:
	an initialized imp

globals:
	impncp=
	impistart=
	impostart=
	imp.nrcv=

calls:
	impstrat
	impldrd

called by:
	impopen

history:
	initial coding 10/5/75 S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/

imp_init()
{
	int i;
	register int saver0;
	
	/* set fixed input addresses */
	impncp = &imp.nrcv;	/* addr of data to hand to_ncp for daemon comm. */
	impistart = &imp.type;	/* start addr to give imp interface for leader reads */

	/* set fixed output addresses */
	impostart = &oimp;	/* start addr of output leader */

	/* reset the imp interface */
	do
	{
		imp_reset();		/* PBJ */

		saver0 = u.u_ar0[R0];
		u.u_ar0[R0] = 2;	/* time to sleep */

		sslep ();		/* sleep for 2 secs */

		u.u_ar0[R0] = saver0;	/* restore */
	}
	while( IMPADDR->istat&imasrdy ); /* until imp says is up PBJ */

	/* enable interrupts and unlock input control reg PBJ */
	IMPADDR->istat = IMPADDR->ostat = iienab;

	/* build and send 3 host imp nops */
	oimp.o_type = ih_nop;	/* set opcode to h_i_nop */
	impobuf.b_addr = impostart;	/* set addr for output */
	impobuf.b_wcount = 4;	/* send the four imp leader bytes */
	/* send the three nops */
	for ( i = 0; i < 3; i++ )
	{
		/* strobe host master ready */
		IMPADDR->istat =| hmasrdy;	/* PBJ */
		impstrat( &impobuf );	/* send one */
		iowait( &impobuf );			/* wait for one to fin */
	}
	/* set parameters on input leader */
	imp.nrcv = ncp_rcv;		/* perm receive comm here */
	impldrd();			/* set up input side of imp interface */
}
/*name:
	imp_dwn

function:
	clean up ncp data structures so that an ncpdaemon restart will
	work correctly.

algorithm:
	reset the imp interface
	quit setting host master ready
	free any input message
	clean up input variables
	clear out all messages queued for the output side
	reset any rfnm bits
	let the kernel buffer code clean up its own

parameters:
	none

returns:
	nothing

globals:
	HMR_interval =
	impmsg =
	implen =
	impiptr =
	impflushing =
	impistart =
	impotab (forward and backward links)
	host_map[-] = 

calls:
	freemsg
	ncp_bfrdwn

called by:
	imp_open

history:
	initial coding 6/22/76 by S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/
imp_dwn()
{
	register char *p;

	/* reset the imp interface */
	imp_reset();		/* PBJ */
	/* quit setting host master ready */
	HMR_interval = 0;

	/* cleanup the input side */
	freemsg( impmsg );
	impmsg = implen = impiptr = impflushing = impistart =  0;

	/* clean up the output side */
	impotab.d_active = 0;
	/* get rid of messages waiting to be output */
	p = impotab.d_actf;
	while ( p )
	{
		if( p->b_flags & B_SPEC )	/* this a net message */
			freemsg( p->b_dev );	/* free it */
		else
			iodone( p );		/* sys buffer say done */
	}

	impotab.d_actf = impotab.d_actl = 0;

	/* clear out any waiting rfnm bits */
	for( p = &host_map[0]; p < &host_map[32]; p++ )
		*p = 0;

	/* let the net message software clean up */
	ncp_bfrdwn();
}

/*name:
	impstrat

function:
	Used to link output buffers into the imp output queue
	for transmission to the network.

algorithm:
	if queue empty put buffer at front. 
	else
	link buffer to end
	if imp output is not active start it up

parameters:
	abp	-	pointer to the buffer to be sent

returns:
	nothing

globals:
	impotab.d_actf=
	impotab.d_actl=
	bp->b_forw=

calls:
	imp_output

called by:
	sndnetbytes
	impopen

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/

impstrat( abp )
struct buf *abp;
{

	register struct buf *bp;

	bp = abp;
	

	bp->b_forw = 0;		/* null forward pointer this buffer */
	spl_imp();
		if( impotab.d_actf == 0 )	/* anything in list ?? */
			impotab.d_actf = bp;	/* no make this the first */
		else
			impotab.d_actl->b_forw = bp;	/* make this new last */
		/* set last to this */
		impotab.d_actl = bp;

	if (impotab.d_active == 0)		/* do we need to start the imp? */
		imp_output();		/* yep kick it over */
	

}

/*name:
	imp_output

function:
	Takes buffers( see buf.h ) linked into the impotab active
	queue and applies them to the imp interface.

algorithm:
	check to see if the queue is empty if so return
	set the active bit
	enable output interrupts and the endmsg bit( imp interface manual)
	calculate then end address and perform the mod 2 function
	on the calculated address( again see imp interface manual)
	load the start output address register
	load the end output address register which is also an implied
	go

parameters:
	none

returns:
	nothing

globals:
	impotab.d_actf
	(buffer)

calls:
	nothing

called by:
	impstrat
	imp_oint

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/

imp_output()
{
	register struct buf *bp;

	
	if((bp = impotab.d_actf) == 0)
		return;			/* return nothing to do */

	impotab.d_active++;		/* something to do set myself active */

	/* set or reset disable endmsg according to user wishes */
	IMPADDR -> ostat = iienab;	/* PBJ */
	if( bp->b_blkno )	/* PBJ */
		IMPADDR->ostat =& ~oendmsg;
	else
		IMPADDR->ostat =| oendmsg;


	IMPADDR->spo = bp->b_addr;	/* load start addr */
	IMPADDR->owcnt = -(bp->b_wcount >> 1);		/* load endaddr and go PBJ */
	IMPADDR->ostat =| igo;		/* crank it up PBJ */
	
}

/*name:
	impldrd

function:
	Called when input portion specifically wants to transfer
	the first bytes of an imp-to-host and host-to-host leader
	into fixed low core.

algorithm:
	make sure starting a new input message to be buffered
	say doing leader read
	load imp input start register from fixed address created
	by impopen
	load imp input end register from fixed address  created
	by impopen

parameters:
	none

returns:
	nothing

globals:
	impmsg=
	impleader=
	impistart

calls:
	nothing

called by:
	impopen
	hh
	ih

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/

impldrd()			/* start another leader into imp */
{

	spl_imp();			/* make sure im not interrupted */
	
	impmsg = 0;		/* start a new message */
	impleader++;		/* say doing leader read */
	IMPADDR->spi = impistart;	/* read into "imp" structure */
	IMPADDR->iwcnt = -IMPLDLEN;
	IMPADDR->istat =| (igo | iwrtenble);		/* make it go fast! PBJ */
	

}


/*name:
	impread

function:
	to apply an input area to the input side of the imp interface

algorithm:
	say not doing leader read
	load start input register with address passed
	load end input register with address+size passed

parameters:
	addr - start address in which to store imp input data
	size - available number of bytes in which to store said data

returns:
	nothing

globals:
	impleader=

calls:
	nothing

called by:
	flushimp
	ihbget

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/

impread( addr,size )		/* start a read from imp into data area */
{

	spl_imp();			/* make sure im not interrupted */
	
	impleader = 0;		/* say not doing leader read */
	IMPADDR->spi = addr;
	IMPADDR->iwcnt = -(size >> 1);		/* PBJ */
	IMPADDR->istat =| (igo | iwrtenble);			/* start it up PBJ */
	

}

/*name:
	imp_oint

function:
	This is the imp output interrupt.
	Handles both system type buffers( buf.h ) and the network msgs
	If it finds that a net message is being sent, steps to the
	next buffer in the message and transmits that.
	If all buffers of a net message or a standard system buffer
	has been encountered, it is freed and the output side is
	started once again.

algorithm:
	Check for unexpected interrupts from the imp
	Get the first buffer from the output queue
	If there was an error in output, it is indicated, and the
	buffer returned.
	If the buffer embodies a network message( B_SPEC ) and
	the last buffer of that message has not been sent,
		Get to next buffer in net message
	        Update w_count for that buffer
		Update next buffer pointer(av_forw) so next
		time come through things will be easier.
		Decide whether to set endmsg bit on not.
		Start up output side
	otherwise
		get next buffer in output queue
		if this was a net message give it back to the system
		otherwise say that the system buffer( buf.h ) is done
	if there is nothing to do clean up a little
	otherwise
	start up the output side again.

parameters:
	none

returns:
	nothing

globals:
	impotab.d_active
	buffer->b_flags=
	buffer->b_error=
	buffer->b_addr=
	buffer->b_wcount=
	buffer->av_forw=
	buffer->b_blkno=
	impotab->d_actf=

calls:
	imp_output
	freemsg
	iodone

called by:
	system

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

imp_oint()
{
	register char *buffer;
	register struct devtab *iot;
	register char *msg;

	iot = &impotab;

	
	if( iot->d_active == 0 )	/* ignore unexpected errors */
		return;

	buffer = iot->d_actf;		/* pt at first buffer in list */

	if( IMPADDR->ostat & itimout )	/* timeout errror */
	{
		buffer->b_flags =| B_ERROR;
		buffer->b_error = itimout;
	}
	else	/* if kernel msg and has multi buffers send next buffer */
	if(( buffer->b_flags & B_SPEC )
			&&
		/* this last bfr in msg */
		(buffer->b_blkno & 01))
	{
		/* no we disabled endmsg last time must send some more */
		
		msg = buffer->av_forw;
		/* set next buffer address into header */
		buffer->b_addr = msg->b_data;
		/* set length of that buffer into length field */
		buffer->b_wcount = ( msg->b_len & 0377 );
		/* if this is not the last buffer then disable endmsg */
		buffer->b_blkno = (msg == buffer->b_dev) ? 0 : 1;

		/* update next msg buffer field */
		buffer->av_forw = msg = msg->b_qlink;

		/* send to imp */
		imp_output();

		
		return;
	}
	iot->d_actf = buffer->b_forw;		/* set top to next */
	/* give msg back -- ptr put in by sndnetbytes */
	if( buffer->b_flags&B_SPEC )
		freemsg( buffer->b_dev );
	else
		iodone( buffer );	/* say done with buffer */

	if( iot->d_actf == 0 )		/* done for awhile?? */
	{
		iot->d_actl = 0;	/* remove queue links */
		iot->d_active = 0;	/* say we are done */
	}
	else
		imp_output();		/* do some more */
	
}

/*name:
	set_HMR

function:
	To repeatedly tickle the host master ready bit of the imp interface

algorithm:
	if someone hasnt set HMR_interval to zero ( should i go away?? )
		set the host master ready bit
		setup timeout so that i am called again

parameters:
	none

returns:
	nothing

globals:
	HMR_interval
	hostmaster ready bit=

calls:
	timeout(sys)

called by:
	impopen
	timeout(sys)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 28 Oct 76 for IMP-11A by P. B. Jones

*/

set_HMR()
{

	if( HMR_interval )
	{
		IMPADDR->istat =| hmasrdy;
/* DISABLE
		timeout( &set_HMR,0,HMR_interval);	/* restart */
	}
}

/*name:
	spl_imp

function:
	centralize priority setting of imp because of variations
	in interface configurations

algorithm:
	if you can't figure this out you'd better go back and
	start all over again

parameters:
	absolutely none

returns:
	yes - if it doesn't you'll know it!

globals:
	no

calls:
	spl5

called by:
	anyone who doesn't want the imp bothering him

history:
	initial coding 28 Oct 76 by P. B. Jones

*/

spl_imp()
{
	spl5();
}
/*name:
	imp_reset

function:
	tweak the correct bits in the imp interface to reset it..
	this routine for the IMP-11A is needed to make the reset easy
	the old interface needed to set one bit.. this one sets and
	resets two

algorithm:
	set the reset bits on the input and output sides of IMP11-A
	reset them according to the manual

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	nobody

called by:
	imp_dwn
	imp_init

history:
	initial coding 28 Oct 76 by P. B. Jones

*/

imp_reset()
{
	register char *impadd;

	impadd = IMPADDR;
	impadd->istat =| irst;		/* reset imp11a */
	impadd->istat =& ~irst;
	impadd->ostat =| (irst | irdyclr);	/* clear ready err and iface */
	impadd->ostat =& ~irst;		/* cleared for imp 11 a */
	impadd->istat = impadd->ostat = 0;
}
   