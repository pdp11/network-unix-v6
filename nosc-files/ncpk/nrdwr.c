#
#include "param.h"
#include "net_net.h"
#include "inode.h"
#include "buf.h"
#include "user.h"
#include "net_hosthost.h"
#include "file.h"
#include "reg.h"

#ifdef SCCSID
/* SCCS PROGRAM IDENTIFICATION STRING */
char id_nrdwr[] "~|^`nrdwr.c\tV3.9E2\t09Mar78\n";
#endif

/* if CLOSE0 is defined you get zero bytes read on end of netfile.
If CLOSE0 is not defined, you get -1 returned on end of netfile.
Write on end of netfile still returns error.
*/
#define CLOSE0 X
/*name:
	netread

function:
	to handle user reads on a file previously opened with netopen

algorithm:
	if there is a socket
		while there is no data and the connection is open
		sleep
	
		if the connection is open
			transfer the min of the num in the queue and the num
			the user asked for into his space
			send a reallocation for the amount of data read
			into user space
			tell user that that much has been transfered
			make the amount in the queue correct

parameters:
	aip	-	a pointer to a socket( i_addr part of an inode )
			that is type read.

returns:
	nothing

globals:
	none

calls:
	min
	sleep
	bytesout
	sendalloc
	spl_imp

called by:
	rdwr of ken/sys2.c

history:
	7Jan75	S.F.Holmgreg initial coding
	15Mar77	S.M.Abraham to set and clear n_rcvwt flag bit
	14Jun77	J.S.Kravitz to return if u.u_count == 0
	14Jun77	J.S.Kravitz to not reallocate on closed
		connection
_ifdef SFHBYTE
	added multiple of 8 bytesizes 01/27/78 S. F. Holmgren
_endif

*/

netread( aip )		/* handles user reads -- called from rdwr of */
struct inode *aip;	   /*net/sys/sys2.c  */
{

register amt;
register struct rdskt *sktp;
register tries;
extern int lbolt;


	sktp = aip;	/* for speed */
	/* is this a valid socket pointer */
	if( sktp == 0 )
	{
		err:
		u.u_error = EBADF;
		return;
	}
	if ( ! u.u_count)
		return;

	tries = 0;
	sktp->r_rdproc = u.u_procp;
	retry:
	spl_imp();	/* so i get to sleep before anyone can wake me */
	while (	( (amt=min(sktp->r_qtotal,u.u_count)) == 0)
	&&	(sktp->r_flags&n_open)
	) {
		sktp -> r_flags =| n_rcvwt ; /* mark skt as waiting for data */
		sleep( sktp,NETPRI );
		sktp -> r_flags =& ~n_rcvwt; /*skt not waiting for data */
	}
	spl0();

	/* wait for data still trickling in even though conn closed */
	if( amt == 0 && tries == 0 )
	{
		/* so we dont do this again */
		tries++;
		/* sleep for 4 seconds */
		sleep( &lbolt,NETPRI );
		/* there still no data? */
		if( sktp->r_qtotal == 0 )
#ifndef CLOSE0
			goto err;		/* yep then say done */
#endif not CLOSE0
#ifdef CLOSE0
			return;		/* yep then say done */
#endif CLOSE0
		else
			goto retry;		/* ahh some data then get it */
	}
	if( amt > 0 )
	{
#ifdef SFHBYTE
		amt =* sktp->r_bsize/8;	/* go to 8 bit bytes */
#endif
		amt =- bytesout( &sktp->r_msgq,u.u_base,amt,0 );	/* give user data */
#ifdef SFHBYTE
		amt = (amt*8)/sktp->r_bsize;	/* go to byte size bytes */
#endif
		u.u_count =- amt;
		if (sktp->r_flags & n_open)
			sendalloc( sktp,amt );
		sktp->r_qtotal =- amt;		/* decrement queued total */
	}
#ifndef CLOSE0
	else
		goto err;

#endif not CLOSE0
}

/*name:
	netwrite

function:
	to handle user writes on a file opened with netopen

algorithm:
	is there a socket and has the user specified a non zero count
		while there is no allocation and the connection is open
			sleep
		if the connection is open
			send as many bytes as the allocation will permit
			wait for the rfnm to return( this is probably not the 
			place to wait for a rfnm )
			if there was an error then resend the data
			update the address and count of data still to be
			sent

			decrement the amount of allocation available from
			the foreign host.

parameters:
	aip	-	a pointer to a socket of type write

returns:
	nothing	( well really the fact that data was sent to the net )

globals:
	none

calls:
	annyalloc
	spl_imp
	min
	sleep
	sndnetbytes
	netsleep
	dpsub

called by:
	rdwr of ken/sys2.c

history:
	initial coding 1/7/75 by S. F. Holmgren
	added bse, cnt, and flg params for server telnet.
	8/26/76 by S. F. Holmgren
	modiefied 3/15/77 by SMA to set and reset n_sendwt,
	n_allwt, and n_prevmerr
	12Jun77 server telnet test code deleted from ncp. Specifically,
		.. bse,cnt,flg are now obtained from the U again, instead
		of as parameter. J.S.Kravitz
	12Jun77 Comment on spl_imp changed J.S.Kravitz
_ifdef SFHBYTE
	modified to add multiple of 8 bit bytesize 01/27/78 S. F. Holmgren
_endif

*/

netwrite( aip )
struct wrtskt *aip;
{

register struct wrtskt *sktp;
register n;


sktp = aip;		/* for speed */
if ( sktp == 0 ) return;
while( u.u_count > 0 )
{
	sktp->w_wrtproc = u.u_procp;
	/*
	 * in order to assure that the allocation doesn't arrive before
	 * we get to sleep, we shut imp interface interrupts off here
	 * for a moment
	 */
	spl_imp();
	while(	(n=min( anyalloc(sktp),u.u_count) ) == 0
	&&	(sktp->w_flags & n_open))
	{
		sktp -> w_flags =| n_allwt ; /* mark skt as waiting for ALL */
		sleep( sktp,NETPRI );
		sktp -> w_flags =& ~n_allwt ; /* skt no longer waiting */
	}
	spl0();

	if (sktp->w_flags & n_open)	/* connection still open */
	{
		retry:
			/* send as much as allocation will permit */
			sndnetbytes ( u.u_base
				,n
				,sktp
				,(sktp->w_hostlink & ~0200)
				,0
				);

			/* wait for rfnm to return */
			if( netsleep( sktp,n_rfnmwt ) & n_xmterr )
				/* if got trans err send again */
			{
				sktp -> w_flags =| n_prevmerr ; /* note error */
				goto retry;
			}
			else /* no error, so send operation complete */
				sktp -> w_flags =& ~(n_prevmerr | n_sendwt) ;

#ifndef SFHBYTE
			u.u_base =+ n;
#endif
#ifdef SFHBYTE
			u.u_base =+ n*(sktp->w_bsize/8);
#endif
			u.u_count =- n;

			/* update allocation from foreign host */
			sktp->w_msgs =- 1;
#ifndef SFHBYTE
			dpsub( sktp->w_falloc, (n<<3) );
#endif
#ifdef SFHBYTE
			dpsub( sktp->w_falloc, (n*sktp->w_bsize) );
#endif
	}
	else
	{
		u.u_error = EBADF;
		return;
	}
}
}
/*name:
	anyalloc

function:
	returns = 	0 if there is no allocation from the foreign host
	        =	1000 if >= 1000 bytes are allocated( max net msg size )
	        =	number of bytes allocated if none of the above

algorithm:
	if number of messages allocated is zero return zero
	if the high word of the two word allocate is non zero
	or
	the low word of the allocation is greater or equal to 8000
	then return 1000 
	otherwise
		return the number of bytes allocated

parameters:
	pointer to a write socket( i_addr part of an inode )

returns:
	see function

globals:
	none

calls:
	nothing

called by:
	netwrite

history:
	initial coding 1/7/75 by S. F. Holmgren
_ifdef SFHBYTE
	added multiple of 8 bit bytesizes 01/27/78 S. F. Holmgren
_endif

*/

anyalloc( wsp )
struct wrtskt *wsp;
{
				/*  called from netwrite  */

	register struct wrtskt *sktp;
	sktp = wsp;
	if (sktp->w_msgs == 0)	/* any msg allocation ? */
		return( 0 );	/* no return zero allocation */
	if ( sktp->w_falloc[0] != 0  ||  sktp->w_falloc[1] >= 8000)
#ifndef SFHBYTE
		return( 1000 );	/* there are at least 1000 bytes so return that  */
	return( (sktp->w_falloc[1]>>3) & 017777); /* return num of bytes alloced */
#endif
#ifdef SFHBYTE
		return( 8000/sktp->w_bsize );
	return( sktp->w_falloc[1]/sktp->w_bsize );	/* return num bytes alloced */
#endif

}



/*name:
	netsleep

function:
	will sleep on the socket( asp ) until one of n_xmterr or n_eof
	or flg get set

algorithm:
	while none of the above mentioned bits are set
	sleep
	determine which bit has been set and return it

parameters:
	asp	-	pointer to a socket
	flg	-	a bit or set of bits to wait for

returns:
	the bit set by the other potions of the system

globals:
	none

calls:
	sleep
	spl_imp

called by:
	netwrite

history:
	initial coding 1/7/75 by S. F. Holmgren
	update of procp value added (from abraham's copy 9may77)

*/

netsleep( asp,flg )
struct wrtskt *asp;
{

	register struct wrtskt *sktp;
	register result;

	sktp = asp;
	sktp -> w_wrtproc = u.u_procp;
	
	spl_imp();	/* see comments on spl_imp's in netread and netwrite */

	while (( sktp->w_flags&(n_xmterr | n_eof | flg)) == 0)
		sleep( sktp,NETPRI );
	spl0();

	result = sktp->w_flags;
	sktp->w_flags =& ~(n_xmterr|n_eof|flg);
	
	return( result & (n_xmterr|n_eof|flg) );

}

/*name:
	sendalloc

function:
	to determine if a re-allocation is needed and if so send a host
	to host allocate in the amount of the difference between hiwat
	and bytes and msgs

algorithm:
	decrement the number of bytes read from bytes allocated
	if the number of bytes allocated is less than 1/2 the number
	in hiwat( a variable parameter ) or the number of messages
	allocated is zero
	send an allocation
		get the hostnumber
		wait for any rfnms out on link zero for that host
		format the allocate protocol
		send it off 

parameters:
	asp	-	a socket pointer(read socket)
	amtread	-	number of bytes transferred into user space

returns:
	0 if no allocation sent
	1 if allocation sent

globals:
	none

calls:
	bit_on
	set_bit
	sndnetbytes
	swab
	spl_imp

called by:
	netread
	tsrvread	(newsrvtel.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified to return allocation indication, and saves current PS
	8/27/76 by S. F. Holmgren
	modified, 3/15/77, S.M.Abraham to set and reset n_hhchanwt
	modified, 6/30/77, J.S.Kravitz to use nominal message allocation
		of NOMMSG instead of nom byte alloc for messages
		comment on wait for rfnm added
_ifdef SFHBYTE
	added multiple of 8 bit bytesize 01/27/78 S. F. Holmgren
_endif

*/

sendalloc( asp,amtread )
struct rdskt *asp;
int amtread;
{

	register struct rdskt *sktp;
	register amt;
	register int sps;
	char host;			/* for storing host num to send alloc to */

	struct
	{
		char all;
		char alink;
		int amsgs;
		int abits[2];
	} sndalloc;


	
	sktp = asp;
	sktp->r_bytes =- amtread;
	if (	(sktp->r_bytes <= (sktp->r_hiwat>>1) )
	||  	( sktp->r_msgs <= (NOMMSG/2) )	)
	/*
	 * Don't allow allocation to go below 50 % bytes or
	 * messages if user is still reading from connection
	 */
	{
		host = (sktp->r_hostlink >> 8);

		sps = PS->integ;		/* save current prio */
		/*
		 * wait for link zero to this host to be free... note
		 * that send_alloc will not resend an allocate if there
		 * is a transmission error.
		 */
		spl_imp();
			while( bit_on( host_map,host ) != 0 )
			{
				sktp -> r_flags =| n_hhchanwt;/*note the wait*/
				sleep( host_map+host,NETPRI );
				sktp -> r_flags =& ~n_hhchanwt;/*not waiting*/
			}
			/* i win quick set rfnm bit */
			set_bit( host_map,host );
		PS->integ = sps;		/* restore prev PS */

		/* we now have a rfnm back from the host send an allocate */
		sndalloc.all = hhall;
		sndalloc.alink = sktp->r_hostlink;
		sndalloc.amsgs = swab( amt = NOMMSG - sktp->r_msgs );
		sktp->r_msgs =+ amt;	/* update to amount of msgs re allocated */
		/*
		 * this allocation thing here should be recoded using
		 * long integers, since the shift could cause very large
		 * allocations to get screwed up when the bits fall off
		 * the left side of the low order word
		 */
		sndalloc.abits[0] = 0;
		sndalloc.abits[1] = 
#ifndef SFHBYTE
			swab ( (amt = sktp->r_hiwat - sktp->r_bytes) << 3 );
#endif
#ifdef SFHBYTE
			swab( (amt = sktp->r_hiwat - sktp->r_bytes) * sktp->r_bsize );
#endif
		sktp->r_bytes =+ amt;	/* update to amount of bytes reallocated */
		sndnetbytes( &sndalloc,8,sktp,0,1 );
		return( 1 );		/* say we sent an allocaiton*/
	}
	return( 0 );			/* say we didnt send an allocation*/
	
}
/*name:
	sendins

function:
	send a host to host ins command. initiated at request of user

algorithm:
	get pointer to file passed
	if null then return
	build ins
	send to the net

parameters:
file id passed down from user in r0

returns:
	nothing

globals:
	none

calls:
	getf	(sys)
	sndnetbytes

called by:
	trap thru sysent

history:
	initial coding 11/6/75 by S. F. Holmgren

*/

sendins()
{
	register int *fp;
	register int *sktp;

	struct
	{
		char ins;
		char link;
	} sndins;

	if( (fp=getf( u.u_ar0[R0] )) == NULL )
	{
		u.u_error = EBADF;
		return;
	}
	if ( ! (fp->f_flag & FNET) )
	{
		u.u_error = ENOTNET;
		return;
	}

	sktp = fp->f_netnode[ f_wrtnode ];
	sndins.ins = hhins;
	sndins.link = sktp->w_hostlink & ~0200;
	sndnetbytes( &sndins, 2, sktp, 0, 1 );
}
