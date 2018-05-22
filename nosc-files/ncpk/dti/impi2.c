#
#include "../h/param.h"
#include "../h/user.h"
#include "../h/buf.h"
#include "../h/net.h"
#include "../h/netbuf.h"
#include "../h/imp.h"
#include "../h/file.h"
#include "../h/ncp.h"
#include "../h/contab.h"
#include "../h/proc.h"
#include "../h/src.h"



/*
	defines for dealing with host to host protocol		*/

#define hhnop	0	/* nop */
#define hhall	4	/* allocate */
#define hhinr	7	/* interrupt by receiver */
#define hhins	8	/* interrupt by sender */


char hhsize[14];
char host_map[32];
/*name:
	flushimp

function:
	increment number of imp flushes and reload imp interface with
	buffer to dump data into

algorithm:
	set impflushing flag
	handle statistics
	reload imp interface with black hole

parameters:
	none

returns:
	nothing

globals:
	impflushing=
	imp_stat.i_flushes=

calls:
	emerbuf			to get address of black hole
	impread			to load imp interface with address of hole

called by:
	imp_input
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
*/

flushimp()
{		/* repeatedly called when we want imp interface cleaned out */
	impflushing = 1;
	imp_stat.i_flushes++;
	impread( emerbuf(),net_b_size );
}



/*name:
	ihbget

function:
	To reload the imp interface with a buffer to store data into

algorithm:
	If a buffer is available set impmsg to address
		call impread to reload imp interface registers
	else
		couldnt get a buffer start flushing data

parameters:
	none

returns:
	nothing

globals:
	impmsg=

calls:
	appendb			to add another buffer to impmsg
	impread			to load imp interface registers
	flushimp		to flush imp data if couldnt get buffer

called by:
	imp_input
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
*/

ihbget()	/* stands for imp host buffer getter */
{
	register struct netbuf *bp;
	/* called when there is data to buffer from the imp */
	/* appendb returns 0 if cant get buffer */
	
	if( bp=appendb( impmsg ) )
	{
		impmsg = bp;
		impread( bp->b_data,net_b_size );
	}
	else
	{
		printf("flush bfr\n");
		flushimp();
	}
	
}

/*


*/
struct salloc		/* structure to lay over an allocate command */
{
	char a_op;
	char a_link;
	int  a_msgs;
	int  a_bitshi;
	int  a_bitslo;
};


/*name:
	hh1

function:
	To search through host to host protocol messages strip out any
	that apply to the user ( allocates for now )

algorithm:
	loops thru looking for hh commands in IMPMSG

		it copies a command at a time to HHPROTO, process the
		protocol if it is an allocate, inr, or ins.  If it is
		none of the above, it is stuck in an output message.

	once all of the input has been messed with, if there is an
	output message it is sent to the ncpdaemon with our blessing.

parameters:
	none

returns:
	nothing

globals:
	impmsg
	implen
	hhsize[]
	impncp

calls:
	allocate		to deal with allocate commands rcvd from net
	to_ncp			to send uninteresting protocol to ncpdaemon
	ipc_trans		to send ins or inr
	psignal			if old style i/o to send ins or inr 

called by:
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
	modified to send events when ins or inr arrives 12/8/76 by David Healy
	re-written (the pile before had mucho bugs) Feb 1977 S. F. Holmgren
*/

hh1()
{

	register char hhcom;
	register cnt;
	register char *sktp;
	static char *daemsg;
	static char hhproto[96];

	daemsg = 0;
	while( implen > 0 )		/* while things in msg */
	{
		hhcom = (impmsg->b_qlink)->b_data[0] & 0377;
		if( hhcom > NUMHHOPS )
		{
			daemsg = catmsg( daemsg,impmsg );
			goto fordaemon;
		}

		cnt = hhsize[ hhcom ];	/* get bytes in this command */

		implen =- cnt;		/* decrement implen */

		if( bytesout( &impmsg,&hhproto,cnt,1 ))	/* msg not long enough */
		{
			printf("imp bad protocol msg\n");
			implen = 0;			/* force msg empty */
			continue;
		}
		else
		if( hhcom == hhall && (sktp=imphostlink( hhproto[1]|0200 )) )
		{
			allocate( sktp,&hhproto );
			continue;
		}
		else
		if( hhcom == hhins && (sktp=imphostlink( hhproto[1]&0177 )) )
		{
			if( &proc[sktp->r_pid]->p_stat )	/* there ? */
			{
				if( sktp->r_flags&n_nbio )
					ipc_trans( SRC_INS | ( 1 << 8 ), sktp->r_pid,sktp->r_ufid,0 );
				else
					psignal( &proc[sktp->r_pid],SIGINR );
			}
			continue;
		}
		else
		if( hhcom == hhinr && (sktp=imphostlink( hhproto[1]|0200 )) )
		{
			if( &proc[sktp->r_pid]->p_stat )
			{
				if( sktp->w_flags&n_nbio )
					ipc_trans( SRC_INR | ( 1 << 8 ),sktp->w_pid,sktp->w_ufid,0 );
				else
					psignal( &proc[sktp->w_pid],SIGINR );
			}
			continue;
		}
		else
		if( hhcom == hhnop )
			continue;

		vectomsg( &hhproto,cnt,&daemsg,1 );		/* got here then give it to daemon */
	}

	if( daemsg != 0 )		/* something in msg */
	fordaemon:
			to_ncp( impncp,5,daemsg );	/* send to daemon */
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
	impncp
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
	modified to implement nbio 12/8/76 by Dave Healy
*/

allocate( skt_ptr,allocp )
struct wrtskt *skt_ptr;
{

	/* called from hh1 when a hh allocate is received */
	register char *ap;
	register struct wrtskt *sktp;
	int	nbytes;		/* allocation */
	struct netbuf *msgp;

	
	sktp = skt_ptr;
	ap = allocp;
	msgp = 0;
		if (sktp->w_flags & n_toncp)
		{
			vectomsg( allocp,8,&msgp,1 );
			to_ncp( impncp,5,msgp );
		}
		else
		{
			sktp->w_msgs =+ swab( ap->a_msgs );
			dpadd(sktp->w_falloc,swab(ap->a_bitslo));
			sktp->w_falloc[0] =+ swab( ap->a_bitshi );
			sktp->w_flags =| n_allocwt;
			if ((sktp -> w_flags & n_waiting) == 0)
			{
				if( (sktp->w_flags&n_open) == 0 )
					return;
				if ((sktp -> w_flags & n_event) && ((nbytes = anyalloc (sktp)) != 0))
				{
					sktp -> w_flags =& ~n_event;
					ipc_trans (SRC_WRITE | (1 << 8),
					sktp -> w_pid, sktp -> w_ufid, nbytes);
				}
				else
					wakeup( sktp );
			}
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
	set impmsg to the new last buffer.

parameters:
	none

returns:
	nothing

globals:
	implen=
	impmsg=

calls:
	swab (sys)		to switch top and bottom bytes
	freebuf			to release the last buffer from the message

called by:
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
*/

int last_nbytes;
rmovepad()
{
	/*  calculates number of bytes from impleader then runs through impsg buffers
	    adding up counts until number calculated or end of msg is reached.
	    sets impcnt tothe min of amt in msg and calculated and discards
	    any excess bytes  */

	register struct netbuf *bfrp;
	register cnt;

	
	impmsg = bfrp = impmsg->b_qlink;	/* point at first bfr */
	implen = swab( imp.bcnt );		/* get num bytes */
	cnt = imp.bsize * implen;		/* calc number of bits */
	cnt = ( cnt+7 ) / 8;
	while( (cnt =- bfrp->b_len) > 0 )
	{
		if( bfrp->b_qlink == impmsg )	/* wrapped around? */
		{
			printf("imp msg wrap\n");
			break;
		}
		bfrp = bfrp->b_qlink;
	}
	if( cnt < 0 )
		bfrp->b_len = (bfrp->b_len & 0377) + cnt;

	while( bfrp->b_qlink != impmsg )
		freebuf( bfrp );

	impmsg = bfrp;
	if( cnt > 0 )			/* didnt eat them all? */
	{
		printf("IMP: missing %d data bytes\n",cnt);
		return(1);
	}
	return(0);
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
	zero 			if not in conn tab or socket not open
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
	if( sktp = incontab( ((imp.host<<8) | (link & 0377)),0 ))
	{
		sktp = sktp->c_siptr;		/* contab returns ptr to entry
						   must get skt pointer 
						*/
			return( sktp );
	}
	return( 0 );
}
 