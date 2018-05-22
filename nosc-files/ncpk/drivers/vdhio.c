#
/* Very Distant Host Specific code */

#include "param.h"
#include "user.h"
#include "file.h"
#include "buf.h"
#include "proc.h"
#include "net_net.h"
#include "net_netbuf.h"
#include "net_ncp.h"
#include "net_contab.h"
#include "net_hosthost.h"
#include "net_vdhstat.h"
#include "net_vdh_imp.h"
#include "net_vdh.h"
/*name:
	hhsize[]

function:
	To give the lengths of the host-host commands.  It is indexed by the
	op code of the command to find the length.

algorithm:
	A static table.

called by:
	hh1

comments:
	This table must be coordinated with the declaration in hosthost.h
	which allocates the table but does not define the values.
*/
char hhsize[14]
{
	1,		/* nop */
	10,		/* rts */
	10,		/* str */
	9,		/* cls */
	8,		/* all */
	4,		/* gvb */
	8,		/* ret */
	2,		/* inr */
	2,		/* ins */
	2,		/* eco */
	2,		/* erp */
	12,		/* err */
	1,		/* rst */
	1		/* rrp */
};
/*
Name:
	impopen

Function:
	to initialize the imp and to serve as a basis for
	the main input driving loop for data from the network

Algorithm:
	set fixed imput addresses
	reset the imp interface
	enable interrupts
	build fixed parameters in the imp input leader
	start up a kernel process to handle data from the net
	build and send 3 nops to the imp

Parameters:
	none

Returns:
	nothing

Globals:
	imp.nrcv=

Calls:
	impstrat
	sleep(sys)
	newproc(sys)
	impldrd
	imp_input
	corezero

Called by:
	ncpopen

History:
	initial coding 1/7/75 by S. F. Holmgren
	rewritten for VDH 29-Jul-75 by Tovar
	rewritten somewhat 22-Oct-75 by Tovar
*/
impopen()
{
	register int *fp, *p;

	/* set fixed input addresses */
	/* set parameters on input leader */
	imp.nrcv = ncp_rcv;		/* perm receive comm here */
	/* start up kernel input process to handle input */
	if( newproc() )
	{
		/* release any hold we may have on parents files */
		for ( fp = &u.u_ofile[0]; fp < & u.u_ofile[NOFILE]; fp++)
			if ((p = *fp) != NULL ) {
				p->f_count--;
				*fp = 0;
			}
		imp_init();	/* initalize */
		/* loop around handling input from imp */
		for(;;)
		{
			spl_imp();
			if ( ! (vstatebits & VDHILAST) )  /*while instead?*/
			{ 
				sleep( &imp,NETPRI);	/* wait for action */
			}
			if (ncpopnstate == 0)
				break;	/* we down? */
			if (vstatebits & VDHILAST)
				imp_input();	/* got some handle it */
			if (vstatebits & VDHINEED)
			{
				getkb();	/*interrupt needs buffer*/
				vstatebits =& ~ VDHINEED;
			}
			if(needinit) {
				needinit = 0;
				log_to_ncp("VDH line down");
				imp_init();
			}
		}
		imp_dwn ();		/* return buffers to kernel */
		exit();	/* kill child; parent was unaware of it anyway */
	}
}
/*
Name:
	imp_input

Function:
	Started in response to a wakeup from the input interrupt
	initiates and switches all transfers received from the imp.

	The interrupt level code has taken care of putting message together
	for a VDH, so all that is left to do is to bless the leader and
	send it off for processing.

algorithm:
	if message type is not 0
		 means imp to host msg received
			call ih and let him deal with that type
	else must be regular message
	is imp pad byte != 0
		yes error do stats and flush message
	else
		legal leader
		if hh protocol msg (imp.link == 0)
			set flag
		else
			if host & link number are in conntab
				then set ptr to associated inode
			else
				error flush message 
	call hh to handle it

parameters:
	none

Returns:
	nothing

globals:
	struct imp	struct for accessing various imp leader fields
	impi_sockt=	set to addr of user inode

Calls:
	hh		to handle hh protocol msgs
	ih		to handle imp to host protocol msgs
	imphostlink	to look host link number up in conn tab return sktp ptr

called by:
	impopen

history:

	initial coding 1/7/75 by Steve Holmgren
	rewritten for VDH 29-Jul-75 by Tovar
*/
imp_input()  /* imp input handler */
{
	int index;

	if (imp.type =& 0137)    /* Regular message? */
	    ih();			/* No, must be from IMP */
	else
	if (imp.pad1 != 0)	  /* legal leader? */
	{
		vs.badld++;		/* no */
		iherr();
	}
	else
	if (imp.link == 0)	  /* hh protocol? */
	{
		impi_sockt = 0;
		hh();
	}
	else
	if ((impi_sockt=imphostlink(imp.link&0177)) == 0)
	{
			/* no link -- give to daemon to log */
		to_ncp(&imp.nrcv, 9/*?*/, impi_msg);
		impi_msg = 0;
		iherr();
	}
	else hh();	/* Give it to host-host processor */
}


iherr()
{
	impi_sockt = 0;
	freemsg(impi_msg);
	impldrd();		/* Start next message */
}
/*
Name:
	hh

Function:
	If data received was not an imp leader, message is
	switched either to user or passed to hh1 for further decoding
	and possible switching.

algorithm:
	data is either for ncp daemon or user
	in either case first data byte is actually the last
	pad byte of host host leader. Since imp interface
	reads only even amts of bytes impleader read consists
	of first 8 bytes of imp to host and host to host leader
	leaving one byte of host host leader remaining padding
	to be discarded, so call bytesout to discard it.
	remove standard imp padding and from that set the length
	of the data in the msg.
	if msg is for user
		if msg is user type but for ncpdaemon
			send ih leader and msg to daemon
		else
			decrement users msg allocation by one
			decrement users byte allocation by msg size
			link msg to user msg queue
			increment users msg queue byte total
			let user run
		msg is a host to host msg call hh1 for further decoding
	start another leader read

parameters:
	none

Returns:
	nothing

globals:
	imp.nrcv
	impi_msg->b_len=
	sktp->r_flags=
	sktp->r_bytes=
	sktp->r_msgs=
	sktp->r_msgq
	sktp->r_qtotal=

Calls:
	freemsg			to destroy buffer data
	wakeup (sys)		to allow user to run
	bytesout		to destroy first data byte
	rmovepad		to remove imp padding and set length
	to_ncp			to pass data to ncpdaemon
	hh1			for further decoding and switching of data
	impldrd			to start another leader read
called by:
	imp_input

history:
	initial coding 1/7/75 by Steve Holmgren
	rewritten for VDH 29-Jul-75 by Tovar
	modified to call tsrvinput 9/19/76 by S. F. Holmgren
*/
hh()		/*  looks at host to host protocol msgs  */
{
  	register struct rdskt *sktp;
	register struct netbuf *bufp;

	int dummy;			/* for discarding pad bytes */

	sktp = impi_sockt;
	impi_msg->b_len = net_b_size; /* by definition the buffer is either full
				or it is the last. If the last rmovepad will
				set the length correctly. */
	bytesout(&impi_msg,&dummy,1,1);	/* destroy first data byte
					it's part of the leader   */
	rmovepad();			/* remove padding and set length */
	if (sktp)			/* user or hh msg? */
		/* it's part of the leader */
		if( sktp->r_flags&n_toncp)	/* data to ncp? */
			to_ncp(&imp.nrcv,5,impi_msg);
		else			/* no, give to user and wakeup */
		{
			/* dec messages allocated */
			sktp->r_msgs--;

			/* if this is server socket feed input to it */
/*
			if( sktp->r_flags & n_srvrnode )
				tsrvinput( impi_msg,sktp->r_msgq );
			else
			{
*/
				/* link message to user read queue */
				sktp->r_msgq = catmsg( sktp->r_msgq,impi_msg );

				/* update users queued total */
				sktp->r_qtotal =+ impi_mlen;
/*
			}
*/
			wakeup( sktp );	/* let him know */
		}
	else	/* nope hh msg */
		hh1();			/* let him handle this */
	impldrd();			/* start another leader read */
}
/*name:
	ih

function:
	To look at all imp to host messages and handle rfnms, incomplete
	transmissions, and nops

algorithm:
	if imp.type is request for next message (rfnm)
		tell user rfnm arrived
	if imp.type is nop
		start another leader read
	if imp.type is error_in_data or incomplete_transmission
		tell user last data he sent is in error or imp couldnt handle
	if imp.type is non of the above
		send imp leader to ncpdaemon for processing

	in any case start another leader read

parameters:
	none

returns:
	nothing

globals:
	imp.type
	imp.nrcv
calls:
	siguser			to let user see rfnms, incomptrans or err data
	to_ncp			to ship leaders to ncpdaemon
	impldrd			to start another leader read

calls:
called by:
	imp_input

history:

	initial coding 1/7/75 by Steve Holmgren
*/

ih()		/*  handles all imp to host messages */
{
	
	switch( imp.type )
	{
		case 15: iherr(); return;	/* debug -- drop long-format leaders */
		case ih_rfnm:	siguser( n_rfnmwt );	/* tell user rfnm arrived  */
		case ih_nop:	break;		/* ignore nops */

		case ih_edata:
		case ih_ictrans:siguser( n_xmterr );	/* tell user about his problem   */
		default:	to_ncp( &imp.nrcv,5,0 );	/* send leader to ncp */

	}
	impldrd();		/* start another leader read */
	
}
/*name:
	siguser

function:
	ostensibly to or the parameter into the users flag field and
	let him run. If the socket is closed send a close command
	to the ncp daemon he was waiting for a msg to clear the imp sys
	If the ncp daemon is monitoring all data going to a user send
	the rfnm to him
	if this is a host to host protocol rfnm let the ncp daemon
	know he can now send further protocol msgs to that host.


algorithm:
	If imp.link!=0 andif the host link entry is in the conn tab
		If conn closed
			send close to ncpdaemon
		if data for ncpdaemon
			send leader to ncpdaemon
		else
			or param into flag field
			allow user to run
	else
		host to host protocol return response so
		allow ncpdaemon to process 

parameters:
	sigflg		bits to be ored into the users flag field

returns:
	nothing

globals:
	imp.link
	imp.nrcv
	conentry->w_flags=

calls:
	imphostlink		to relate host link to socket entry
	wakeup (sys)		to allow user to run
	to_ncp			to send data to ncp daemon

called by:
	ih

history:

	initial coding 1/7/75 by Steve Holmgren
	modified to release rfnm waits on xmit errors also
	9/19/76 S. Holmgren
*/

siguser( sigflg )		/* called when user needs waking up. the
				   passed flag is ored into his flag field
				   and he is awakened */
{
	register conentry;
	
	if( imp.link && (conentry = imphostlink( imp.link | 0200 )) )
	{
			if( conentry->w_flags & n_toncp )
				to_ncp( &imp.nrcv,5,0 );	/* give to ncp */
			else
			{
				conentry->w_flags =| sigflg;	/* set flags */
				wakeup( conentry );
			}
	}
	else
	{
		/* user waiting for control link rfnm? */
		if( bit_on( host_map,imp.host ) != 0 )
		{
			/* say rfnm here */
			reset_bit( host_map,imp.host );
			/* let any users battle it out */
			wakeup( host_map+imp.host );
		}
		else
			/* no users waiting ship to daemon */
			to_ncp( &imp.nrcv,5,0 );
	
	}
}
/*name:
	hh1

function:
	To search through host to host protocol messages strip out any
	that apply to the user ( allocates for now )

algorithm:
	Looks through each buffer of the message on host to host
	protocol boundaries for and allocate protocol msg. When
	one is found, copies the msg into hhmsg and overwrites
	with host to host nops. Calls allocate with the allocate
	protocol command with deals with it at the user level.
	once all the msg has been passed through, it as well as
	the imp to host leader is passed to the ncpdaemon for further
	processing.

parameters:
	none

returns:
	nothing

globals:
	impi_msg
	hhsize[]
	imp.nrcv

calls:
	allocate		to deal with allocate commands rcvd from net
	to_ncp			to send uninteresting protocol to ncpdaemon
	prt_dbg			to note all flush calls if in debug mode
	bytesout
	catmsg
	imphostlink
	vectomsg

called by:
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
	modified 1/1/77 by Steve Holmgren to simplify and correct bug
	related to hh protocal crossing buffer boundary
	Modified 6/26/78 by Greg Noel to remove use of impi_mlen.
*/
hh1()
{

	register char *sktp;
	register int hhcom, cnt;
	char *daemsg, hhproto [15];

	daemsg = 0;
	while( impi_msg ) {	/* while things in msg */
#ifndef UCBUFMOD
		hhcom = (impi_msg->b_qlink)->b_data[0] & 0377;
#endif UCBUFMOD
#ifdef UCBUFMOD
		hhcom = fbbyte(impi_msg->b_qlink->b_loc, 0);
#endif UCBUFMOD
		if( hhcom > NUMHHOPS ) {
			daemsg = catmsg( daemsg,impi_msg );
			goto fordaemon;
		}

		cnt = hhsize[ hhcom ];	/* get bytes in this command */

		if( bytesout( &impi_msg,&hhproto,cnt,1 ));	/* msg not long enough */
		else
		if( hhcom == hhall && (sktp=imphostlink( hhproto[1]|0200 )) ) {
			allocate( sktp,&hhproto );
			continue;
		}
		else
		if( hhcom == hhins && (sktp=imphostlink( hhproto[1]&0177 )) ) {
			if( (sktp -> r_rdproc) -> p_stat ) {	/* there ? */
				psignal( sktp->r_rdproc,SIGINR );
			}
			continue;
		}
		else
		if( hhcom == hhinr && (sktp=imphostlink( hhproto[1]|0200 )) ) {
			if( (sktp -> r_rdproc) -> p_stat ) {
				psignal (sktp -> w_wrtproc, SIGINR);
			}
			continue;
		}
		else
		if( hhcom == hhnop )
			continue;

		vectomsg( &hhproto,cnt,&daemsg,1 );	/* got here then give it to daemon */
	}

	if( daemsg != 0 )		/* something in msg */
fordaemon:
			to_ncp (&imp.nrcv, 5, daemsg);	/* send to daemon */
}

/*name:
	allocate

function:
	To look over host to host allocate protocol messages .
	determine whether they are going to ncpdaemon or user
	send off to ncpdaemon, inc appropriate user fields.

algorithm:
	if host_link in conn tab
		if socket flags say to ncpdaemon
			send to ncpdaemon with imp to host leader
		else
			update num of messages alocated
			update number of bits allocated
			tell user allocate came in
			let user run

parameters:
	allocp 		pointer to a host host allocate

returns:
	nothing

globals:
	imp.nrcv
	sktp->w_msgs=
	sktp->w_falloc=
	sktp->w_flags=

calls:
	imphostlink		to get pointer to socket inode from host link
	vectomsg		to build a msg from vec passed to send to ncp daemon
	to_ncp			to ship the allocate off to the ncp daemon
	dpadd (sys)		to add two double precision words
	wakeup (sys)		to let the user run

called by:
	hh1

history:

	initial coding 1/7/75 by Steve Holmgren
*/

allocate( skt_ptr,allocp )
struct wrtskt *skt_ptr;
{

	/* called from hh1 when a hh allocate is received */
	register char *ap;
	register struct wrtskt *sktp;
	struct netbuf *msgp;

	
	sktp = skt_ptr;
	ap = allocp;
	msgp = 0;
	if (sktp->w_flags & n_toncp)
	{
		vectomsg( ap,8,&msgp,1 );
		to_ncp( &imp.nrcv,5,msgp );
	}
	else
	{
		sktp->w_msgs =+ swab( ap->a_msgs );
		sktp->w_falloc[0] =+ swab(ap->a_bitshi);
		dpadd(sktp->w_falloc,swab(ap->a_bitslo));
		sktp->w_flags =| n_allocwt;
		wakeup( sktp );
	}
	
}
/*name:
	rmovepad

function:
	To remove the padding attached to every host host protocol msg
	and standard message by the imp

algorithm:
	given a bytesize in number of bytes in that size, calculate
	the number of 8 bit bytes.
	set the message length to that size
	run through the message a buffer at a time subtracting
	the buffer length from the calculated size. eventually
	it will go negative, since the number of bytes calculated
	is less that the actual number of bytes in the msg
	subtract the number of pad bytes from the last buffer in
	the message to setthe number of actual number of data bytes
	then free any remaining buffers.
	set impi_msg to the new last buffer.

parameters:
	none

returns:
	nothing

globals:
	impi_mlen=
	impi_msg=

calls:
	swab (sys)		to switch top and bottom bytes
	freebuf			to release the last buffer from the message

called by:
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
*/

rmovepad()
{
	/*  calculates number of bytes from impleader then runs through impsg buffers
	    adding up counts until number calculated or end of msg is reached.
	    sets impcnt to the min of amt in msg and calculated and discards
	    any excess bytes  */

	register struct netbuf *bfrp;
	register cnt;

	
	cnt = (imp.bsize>>3)*swab(imp.bcnt);	/* given a bytesize and bytes calc num 8 bit bytes */
	impi_mlen = cnt;		/* setup imp len */
	impi_msg = bfrp = impi_msg->b_qlink;	/* pt at first bfr in msg */
	while( (cnt =- (bfrp->b_len & 0377)) > 0 )	/*get to last bfr with valid data */
		bfrp = bfrp->b_qlink;

	if (cnt < 0)			/* cnt has -(# bytes to discard) */
		bfrp->b_len = (bfrp->b_len & 0377) + cnt;	/* discard extra bytes this buffer */
	while( bfrp->b_qlink != impi_msg )	/* while not pting at frst bfr */
		freebuf(bfrp);
	impi_msg = bfrp;			/* set new handle on msg */
	

}
/*name:
	imphostlink

function:
	Checks to see if the current host and link are in the connection
	table (conn tab). Checks to see if the socket is open, if so
	returns a pointer to the socket. If not in conn tab or socket
	not open returns zero

algorithm:
	if host link in conn tab
		if skt open
			return socket ptr

	return zero

parameters:
	link			link to be checked with current host

returns:
	zero 			if not in conn tab or socket NOT open
	socket ptr		if in conn tab and socket open

globals:
	imp.host
	sktp->r_flags

calls:
	incontab		to see if host link is in conn tab

called by:
	imp_input
	siguser
	allocate

history:

	initial coding 1/7/75 by Steve Holmgren
*/

imphostlink( link )
char link;
{
	register sktp;
	if (imp.host == 0)
		v_pan ("Host Zero\n");
	if( sktp = incontab((( imp.host << 8 ) | (link & 0377) ),0))
	{
		sktp = sktp->c_siptr;		/* contab returns ptr to entry
						   must get skt pointer 
						*/
			return( sktp );
	}
	return( 0 );
}
/*name:
	imp_dwn

function:
	clean up ncp data structures so that an ncpdaemon restart will
	work correctly.

algorithm:
	reset the imp interface
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
	impi_msg =
	impi_mlen =
	impi_sockt =
	impi_flush =
	impotab (forward and backward links)
	host_map[-] = 

calls:
	freemsg
	ncp_bfrdwn
	imp_reset
	printf

called by:
	imp_open

history:
	initial coding 6/22/76 by S. F. Holmgren
	modified 4/1/77, S.M. Abraham to fix bug in loop that frees
	all msgs in output queue. It wasn't resetting the msg ptr
	to the next msg in the queue.
	printf changed to 'NCP' from 'IMP' 31AUG77 JSK

*/
imp_dwn()
{
	register char *p;

	/* reset the imp interface */
/* NOTYET
	imp_reset();		/* JSK */

	/* cleanup the input side */
	freemsg( impi_msg );
/* NOTYET */	impi_msg = impi_mlen = impi_sockt = 0;	/*
	impi_msg = impi_mlen = impi_sockt = impi_flush =  0;

	/* clean up the output side */
#ifndef UCBUFMOD
	impotab.d_active = 0;
	/* get rid of messages waiting to be output */
	p = impotab.d_actf;
	while (p)
	{
		if( p->b_flags & B_SPEC )	/* this a net message */
			freemsg( p->b_dev );	/* free it */
		else
			iodone( p );		/* sys buffer say done */
		p = p -> b_forw;		/* pt to next msg */
	}

	impotab.d_actf = impotab.d_actl = 0;
#endif UCBUFMOD
#ifdef UCBUFMOD
	freemsg(impoq); impoq = 0;	/* release messages q'ed for output */
#endif UCBUFMOD

	/* clear out any waiting rfnm bits */
	for( p = &host_map[0]; p < &host_map[256/8]; p++ )
		*p = 0;

	printf("\nNCP:Down!\n");	/*JCM & JSK*/

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

*/
#ifndef UCBUFMOD
impstrat( abp )
struct buf *abp;
{

	register int saveps;
	register struct buf *bp;

	bp = abp;
	

	bp->b_forw = 0;		/* null forward pointer this buffer */
	bp->av_back = 0;	/* cleared for VDH routine: fillchan */
	saveps = PS->integ;	/* Save PS */
	spl_imp();
		if( impotab.d_actf == 0 )	/* anything in list ?? */
			impotab.d_actf = bp;	/* no make this the first */
		else
			impotab.d_actl->b_forw = bp;	/* make this new last */
		/* set last to this */
		impotab.d_actl = bp;

	if (impotab.d_active == 0)		/* do we need to start the imp? */
		imp_output();		/* yep kick it over */
	PS->integ = saveps;		/* Restore old PS */
	

}
#endif UCBUFMOD
