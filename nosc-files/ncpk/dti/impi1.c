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
#include "../h/src.h"



/*
	defines for dealing with host to host protocol		*/

#define hhnop	0	/* nop */
#define hhall	4	/* allocate */

/* this array is indexed by the above op codes to give the length of the command */

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

/* this array holds the rfnm wait bits for sending allocations to hosts */
char host_map[32];


/*
name:
	imp_iint

function:
	Simply wakes up the input handling process to tell something
	has arrived from the network

algorithm:
	calls wakeup

parameters:

	none


returns:
	i hope so

globals:
	imp_proc

calls:
	wakeup		to start input handling process running

called by:
	hardware

history:
	coded by Steve Holmgren 04/28/75


*/

imp_iint()
{
	wakeup( &imp );
}
/*name:
	imp_input

function:
	Started in response to a wakeup from the input interrupt
	initiates and switches all transfers received from the imp.

algorithm:
	There are two logical states -
	1. The driver will setup an 8 byte read from the imp when
	   it is in the leader state evidenced by impleader != 0
	   even if there is more data to be received from the imp
	   the interface will interrupt and come here for decoding.

	2. After a leader has been received and the endmsg bit of 
	   interface is not sent, the driver falls into a data
	   buffering mode reloaded by ihbget until the endmsg
	   flag is raised.


	if buffering data
		call hh let him handle it
	else
		if leaderread and endmsg means imp to host msg received
			call ih and let him deal with that type
		else
			must be a normal leader with further data waiting in imp
			is imp pad byte != 0
				yes error do stats and flush imp
			else
				legal leader
				if hh protocol msg (imp.link == 0)
					set flag
					continue buffering data
				else
					if host & link number are in conntab
						then set ptr to associated inode
						and continue reading data
					else
						error flush imp 

parameters:
	none

returns:
	nothing

globals:
	impleader=		toggle as to whether doing leader reads
	struct imp		struct for accessing various imp leader fields
	impiptr=		set to addr of user inode

calls:
	hh			to handle hh protocol msgs
	ih			to handle imp to host protocol msgs
	flushimp		to keep reading data until endmsg is on
	ihbget			to reload imp with fresh imp data buffer
	imphostlink		to look host link number up in conn tab return sktp ptr

called by:

history:

	initial coding 1/7/75 by Steve Holmgren
*/


imp_input()	/* imp input handler */
{
	
	/* check of imp error bit */
	if (IMPADDR->istat < 0 && IMPADDR -> iwcnt != 0) /* imp incomplete sets error also */
	{
		printf("IMP error -- resetting\n");
		while( IMPADDR->istat < 0 )
			imp_init();
		return;
	}
	if (impleader == 0)	/* leader read ? */
		hh();		/* no data is being buffered */
	else
	{
		if( IMPADDR->istat & iendmsg ) /* ih msg? */
			ih();	/* yes let him deal with it */
		else
		{		/* host to host leader */
			/* legal leader */
			if( ((imp.type&07) != 0) || (imp.pad1 != 0) )
			{
			iherr:
				impiptr = 0;
				imp_stat.i_leaderr++;
				flushimp();
				return;
			}
			else
			{   /* yes legal leader */
				if (imp.link == 0)	/* hh protocol? */
				{
					impiptr = 0;
				nxtbuf:
					ihbget();
					return;
				}
				/* data read -- we know this guy?? */
				if( impiptr = imphostlink( imp.link & 0177 ))
					goto nxtbuf;	/* got iptr cont reading */
				else
				{
					goto iherr;	/* whos he err */
				}
			}
		}
	}
	
}



/*name:
	hh

function:
	If data received was not an imp leader to continue buffering
	until endmsg is raised on imp interface, once raised, msg is
	switched either to user or passed to hh1 for further decoding
	and possible switching.

algorithm:
	if endmsg raised
		if flushing imp
			discard data 
			reset flushing switch
			if data for user
				indicate error 
				allow him to run
		else
		not flushing so data is either for ncp daemon or user
				decrement users byte allocation by msg size
				link msg to user msg queue
				increment users msg queue byte total
				let user run
			msg is a host to host msg call hh1 for further decoding
		start another leader read
	else
		endmsg not set so either keep buffering or flushing
		if flushing
			keep flushing
		else
			keep reading

parameters:
	none

returns:
	nothing

globals:
	impmsg->b_len=
	impflushing=
	sktp->r_flags=
	sktp->r_bytes=
	sktp->r_msgs=
	sktp->r_msgq
	sktp->r_qtotal=

calls:
	freemsg			to destroy buffer data
	wakeup (sys)		to allow user to run
	bytesout		to destroy first data byte
	rmovepad		to remove imp padding and set length
	to_ncp			to pass data to ncpdaemon
	hh1			for further decoding and switching of data
	impldrd			to start another leader read
	flushimp		to continue flushing data from imp
	ihbget			continue bufferin data
called by:
	imp_input

history:

	initial coding 1/7/75 by Steve Holmgren
	check for something in impmsg 8/16/76 S. Holmgren
	modified to do send events 12/8/76 by David Healy
*/

hh()
{
	register struct rdskt *sktp;
	register struct netbuf *bufp;
	int temp;

	sktp = impiptr;
	if( impmsg )			/* if there is anything in the msg */
		impmsg->b_len = net_b_size;	/* by defination the buffer
						   is either full or the last.
						   if last rmovepad will set
						   length correctly */

	if( IMPADDR->istat & iendmsg)	/* end of msg? */
	{
		if( impflushing ) 		/* finished flushing */
		{
			freemsg( impmsg );	/* destroy any bfred data */
			impmsg = 0;		/* get rid of previous msg */
			impflushing = 0;	/* reset flushing */
			if (sktp)		/* was user proc involved */
			{
				sktp->r_flags =| n_eof;	/* set err */
				if (sktp -> w_flags & n_nbio)
					ipc_trans (SRC_READ | (1 << 8),
						sktp -> r_pid, sktp -> r_ufid, -1);
				else
					wakeup( sktp );
			}
		}
		else
		{
			if( impmsg == 0 )		/* empty msg? */
			{
				printf("zero impmsg\n");
				return;
			}
			if( bytesout( &impmsg,&temp,1,1 ) )
				printf("imp odd byte\n");
			if( rmovepad() )	/* remove padding and set len */
			{
				impldrd();
				return;
			}
			if (sktp)		/* user or hh msg? */
			{
				if( sktp->r_flags&n_toncp)	/* data to ncp? */
					to_ncp(impncp,5,impmsg);
				else	/* no give to user and wakeup */
				{
					/* dec msgs allocated */
					sktp->r_msgs--;
					sktp->r_qtotal =+ implen;
					sktp->r_msgq = catmsg( sktp->r_msgq,impmsg );
					if (sktp -> r_flags & n_nbio)
					{
						if (sktp -> r_flags & n_event)
						{
							ipc_trans (SRC_READ | (1 << 8), sktp -> r_pid,
								sktp -> r_ufid, sktp -> r_qtotal);
							sktp -> r_flags =& ~n_event;
						}
					}
					else
						wakeup( sktp );
				}
			}
			else	/* nope hh msg */
				hh1();		/* let him handle this */
		}
		impldrd();	/* start another leader read */
	}
	else
	if (IMPADDR -> iwcnt == 0) /* how about buffer full then */
	{
		/* not end msg - keep flushing or reading */
		if( impflushing )	/* flushing? */
			flushimp();	/* yes - keep flushing */
		else
			ihbget();	/* no cont reading data */
	
	}
	else
		printf("IMP interface spurious interrupt\n");
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

ih()
{
	
	switch( imp.type )
	{
		case ih_rfnm:	siguser( n_rfnm );	/* tell user rfnm arrived  */
		case ih_nop:	break;		/* ignore nops */

		case ih_edata:
		case ih_ictrans:siguser( n_xmterr );	/* tell user about his problem   */
				break;
		default:	to_ncp( impncp,5,0 );	/* send leader to ncp */

	}
	impldrd();		/* start another leader read */
	
}


/*name:
	siguser

function:
	ostensibly to or the parameter into the users flag field and
	let him run. If the socket is closed send a close command
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
			if got a rfnm
				update msg and bit allocations
			if non blocking i/o
				if event wanted
					send rfnm or xmiterr event
			else
				wakeup user
			turn of waiting and n_event bits
	else
		host to host protocol return response so
		allow ncpdaemon to process 

parameters:
	sigflg		bits to be ored into the users flag field

returns:
	nothing

globals:
	imp.link
	impncp
	conentry->w_flags=

calls:
	imphostlink		to relate host link to socket entry
	wakeup (sys)		to allow user to run
	to_ncp			to send data to ncp daemon

called by:
	ih

history:

	initial coding 1/7/75 by Steve Holmgren
	removed check for rfnm so incomplete transmissions 
	will also reset the host rfnm bit 8/20/76 S. Holmgren
	modified to send completion events 12/8/76 by David Healy
*/

siguser( sigflg )		/* called when user needs waking up. the
				   passed flag is ored into his flag field
				   and he is awakened */
{
	register conentry;
	register src;			/* for sending events */
	register int cnt;		/* for sending events */
	
	if( imp.link != 0 )
	{
		if( (conentry = imphostlink( imp.link | 0200 )) == 0 )
		{
/* 			printf("rfnm on bad link %o %o\n",imp.host,imp.link); /* */
			to_ncp( impncp,5,0 );
			return;
		}
		if( (conentry->w_flags&n_open) == 0 )
			return;
		if( conentry->w_flags & n_toncp )
			to_ncp( impncp,5,0 );	/* give to ncp */
		else
		{
			conentry -> w_flags =& ~n_waiting;
			conentry->w_flags =| (sigflg);	/* set flags */
			if (sigflg & n_rfnm)		/* update allocation? */
			{
				conentry -> w_msgs =- 1;	/* dec msg cnt */
				dpsub (conentry -> w_falloc, conentry -> w_xfalloc); /* and bits */
				conentry->w_xfalloc = 0;
			}
			if (conentry -> w_flags & n_nbio)
			{
				conentry -> w_flags =| n_event;
				src = (sigflg & n_rfnm) ? SRC_WRITE : SRC_NAK;
				if ((cnt = anyalloc (conentry)) || src == SRC_NAK)
				{
					conentry -> w_flags =& ~n_event;
					ipc_trans (src | (1 << 8), conentry -> w_pid, conentry -> w_ufid,cnt);
				}
			}
			else
				wakeup( conentry );
		}
	}
	else
	{
		/* user waiting for control link rfnm? */
		if( bit_on( host_map,imp.host & 0377) != 0 )
		{
			/* say rfnm here */
			reset_bit( host_map,imp.host & 0377);
			/* let any users battle it out */
			wakeup( host_map+(imp.host&0377) );
		}
		else
			/* no users waiting ship to daemon */
			to_ncp( impncp,5,0 );
	
	}
}

 