#

/*	ir_proc		*/

/*	globals declared in this file:
		ir_proc

	functions declared in this file:
		ir_reglr
		ir_erldr
		ir_igd
		ir_nop
		ir_rfnm
		ir_hdeds
		ir_re_x
		h_dead
		ir_reset
		ir_iltyp
*/

#include	"hstlnk.h"
#include	"files.h"
#include	"socket.h"
#include	"probuf.h"
#include	"kread.h"
#include	"globvar.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_ir_proc[] "~|^`ir_proc.c\tV3.9E1\t10Mar78\n";

/*name:
	ir_reglr

function:
	performs initial decoding and checking of type zero message from imp.

algorithm:
	if link = 0:
		call hr_dcode to parse host-host commands.
	otherwise if link number is illegal:
		log error message via log_bin.
	otherwise:
		call hr_data to handle the data

parameters:
	none.

returns:
	nothing.

globals:
	host
	kr_buf.krr_host
	kr_buf.krr_link

calls:
	hr_dcode
	hr_data
	log_bin

called by:
	kr_rcv	thru ir_proc

history:
	initial coding 12/16/74 by G. R. Grossman

*/

ir_reglr()
{
	register char	link_num;	/* will hold link number for testing */

	link_num = kr_buf.krr_link;	/* get link number */
	if ( link_num == 0 )	/* link zero (host-host control link)? */
		hr_dcode();	/* go decode protocol message */
	else	/* not link zero */
		if ( link_num == 1 || link_num > 71 )	/* illeagal link? */
			log_bin("ir_reglr: bad data link",&kr_buf.lo_byte,
				krr_ovhd+kr_bytes);	/* log error */
		else	/* legal link */
			hr_data();	/* go process data */
}

/*name:
	ir_erldr

function:
	processes "error in leader" message from imp.

algorithm:
	just log it.

parameters:
	none.

returns:
	nothing.

globals:
	kr_buf

calls:
	log_bin

called by:
	kr_rcv thru ir_proc

history:
	initial coding 12/18/74 by G. R. Grossman

*/

ir_erldr()
{
	log_bin("ir_erldr: error in leader",&kr_buf.krr_type,4);
		/* log with contents of imp message */
}

/*name:
	ir_igd

function:
	processes the "imp going down message" from the imp.

algorithm:
	write the information provided in the last two bytes of the
	leader into the first two bytes of the host status file via hs_update.
	log the contents of the imp message.

parameters:
	none.

returns:
	nothing

globals:
	kr_buf

calls:
	hs_update
	log_bin

called by:
	kr_rcv	thru ir_proc

history:
	initial coding 12/16/74 by G. R. Grossman
	use of hs_update for status added 5/28/75 by G. R. Grossman.

*/

ir_igd()
{
	hs_update(0,&kr_buf.krr_link);		/* log as host 0 status */
	log_bin("ir_igd - imp going down",&kr_buf.krr_type,4);
		/* log with contents of imp message */
}

/*name:
	ir_nop

function:
	processes the imp "nop" message.

algorithm:
	does nothing.

parameters:
	none.

returns:
	nothing.

globals:
	none.

calls:
	nothing.

called by:
	kr_rcv	thru ir_proc

history:
	initial coding 12/16/74 by G. R. Grossman

*/

ir_nop()
{
}

/*name:
	ir_rfnm

function:
	processes the "request for next message" message from the imp.

algorithm:
	if the link number field in the leader is non-zero:
		if a matching socket is found via hl_find:
			pass to file level via fi_rfnm.
		otherwise:
			log the occurrance via log_bin.
		return.
	set host's retry count to zero.
	reset the host's rfnm bit.
	delink the number of probufs indicated by the host's h_pb_sent
	counter from the host's h_pb_q and enter them in pb_fr_q.
	if there are more probufs in the host's h_pb_q:
		set pro2send.
	if this is the host currently being reset during initialization:
		go do the next one via rst_all.

parameters:
	none.

returns:
	nothing

globals:
	host
	h_pb_rtry=
	rfnm_bm=
	h_pb_sent=
	h_pb_q=
	pb_fr_q=
	pro2send=
	init_host
	kr_buf.*

calls:
	reset_bit
	q_dlink
	q_enter
	rst_all
	hl_find
	fi_rfnm
	log_bin

called by:
	kr_rcv	thru ir_proc

history:
	initial coding 12/16/74 by G. R. Grossman
	revision for reset mechanism 1/17/75 by G. R. Grossman.
	revision for server icp 7/8/75 by G. R. Grossman.

*/

ir_rfnm()
{
	register int	h;		/* will hold host number */
	register struct socket	*s_p;	/* will hold socket sstruct pointer
					   if rfnm is on a data link */

	h = host;
	if ( kr_buf.krr_link != 0 )	/* on data as opposed to control link?
					*/
	{
		if ( ( s_p = hl_find ( h | ( ( kr_buf.krr_link | 0200 ) << 8 ) )
		     ) != 0 )		/* find a socket w/ matching host
					   & link ? */
		{
			fi_rfnm ( s_p );	/* pass to file level */
		}
		else	/* no match */
			log_bin ( "ir_rfnm:unmatched rfnm", &kr_buf.krr_type, 4 );
				/* log it */
		return;			/* all done w/ non-zero link */
	}
	h_pb_rtry[h] = 0;		/* set retry count to zero */
	reset_bit(&rfnm_bm[0],h);	/* reset host's rfnm bit */
	while ( h_pb_sent[h] )		/* loop while probufs sent != 0 */
	{
		q_enter(&pb_fr_q,q_dlink(&h_pb_q[h]));
			/* transfer a probuf to host's q to free q */
		h_pb_sent[h]--;		/* decrement count of probufs sent */
	}
	if ( h_pb_q[h] != 0 )		/* still have prbufs to send? */
		pro2send = 1;		/* set flag */
	if ( h == init_host )		/* is this the host we are currently
					   resetting during initialization? */
		rst_all();		/* try the next one */
}

/*name:
	ir_hdeds

function:
	processes "dead host status" message from the imp.

algorithm:
	write last two bytes of leader in apppropriate place (byte number
	= 2*{host number}) in host status file via hs_update.
	marks host dead in h_up_bm

parameters:
	none.

returns:
	nothing

globals:
	host
	kr_buf
	h_up_bm

calls:
	hs_update
	reset_bit

called by:
	kr_rcv	thru	ir_proc

history:
	initial coding 12/16/74 by G. R. Grossman.
	changed to use hs_update 5/28/75 by G. R. Grossman.
	resets internal host up  6/07/76 by S. F. Holmgren

*/

ir_hdeds()
{
	hs_update(host,&kr_buf.krr_link);	/* log host status */
	reset_bit( &h_up_bm[0],host );		/* say host is dead */
}

/*name:
	ir_re_x

function:
	processes "destination host or imp dead",
		  "error in data",
		  "incomplete transmission"
	messages from the imp.

algorithm:
	if the link number field in the imp leader is non-zero:
		if a matching socket is found via hl_find:
			initiate close via kill_skt.
		otherwise:
			log the occurrance via log_bin.
		return.
	reset host's rfnm bit.
	decrement host's retry count; if <= 0:
		calls h_dead to process dead host
		if this is the host currently being reset during 
		initialization:
			go do the next one via rst_all.
	otherwise:
		sets pro2send to cause retransmission.

parameters:
	none.

returns:
	nothing.

globals:
	rfnm_bm=
	h_pb_rtry=
	host
	pro2send=
	init_host
	kr_buf.*

calls:
	reset_bit
	h_dead
	rst_all
	hl_find
	kill_skt

called by:
	kr_rcv thru ir_proc

history:
	initial coding 12/16/74 by G. R. Grossman
	revision for reset mechanism 1/17/75 by G. R. Grossman.
	revision for non-zero link #'s 7/8/75 by G. R. Grossman.

*/

ir_re_x()
{
	register int	h;		/* will hold host number */
	register struct socket	*s_p;	/* will point to socket struct if link
					   non-zero */

	h = host;
	if ( kr_buf.krr_link != 0 )	/* on data as opposed to control link?
					*/
	{
		if ( ( s_p = hl_find ( h | ( ( kr_buf.krr_link | 0200 ) << 8 ) )
		     ) != 0 )		/* find a socket w/ matching host
					   & link ? */
		{
			kill_skt ( s_p );	/* initiate close */
		}
		else	/* no match */
			log_bin ( "ir_re_x:unmatched host-imp err", &kr_buf.krr_type, 4 );
				/* log it */
		return;			/* all done w/ non-zero link */
	}
	reset_bit(&rfnm_bm[0],h);	/* reset host's rfnm bit */
	if ( --h_pb_rtry[h] <= 0)	/* retries exhausted? */
	{
		h_dead();		/* process dead host */
		if ( h == init_host )	/* current initialization reset host? */
			rst_all();	/* go do next one */
	}
	else				/* more retries */
		pro2send = 1;		/* set flag for retransmission */
}

/*name:
	h_dead

function:
	takes care of cleaning up all tables relevant to a host when it
	dies or resets.

algorithm:
	loop thru all socket structs:
		if socket is not in null state and belongs to host:
			call aprrpriate dead procedure thru skt_oper.
	flush host's h_pb_q via pr_flush.
	reset host's up bit via reset_bit.
	write reset to kernel via kw_reset.
	update host status file via hs_update.

parameters:
	none.

returns:
	nothing.

globals:
	host
	sockets
	nsockets
	si_dead
	skt_oper
	ss_null
	h_up_bm=
	host_dead

calls:
	pr_flush
	reset_bit
	kw_reset
	so_ut1	thru skt_oper
	free_skt	"
	so_ut2		"
	so_ignr		"
	hs_update

called by:
	ir_re_x
	hr_rst
	kr_rst

history:
	initial coding 1/2/75 by G. R. Grossman
	update of host status file added 5/28/75 by G. R. Grossman
	modified not to reset rfnm bit by Greg Noel on 8/31/77

*/

h_dead()
{
	register struct socket	*s_p;	/* will point to sockets on trip
					   thru socket array */
	register int	h;		/* will hold host number */

	h = host;
	for ( s_p = &sockets[0] ; s_p < &sockets[nsockets] ; s_p++ )
		/* loop thru all socket structs */
	{
		if ( s_p->s_state != ss_null	/* socket state not null? */
		     &&(s_p->s_hstlnk.hl_host & 0377) == h )	/* and this host? */
			(*skt_oper[si_dead][s_p->s_state])(s_p);
				/* call appropriate dead procedure */
	}
	pr_flush();				/* flush host's h_pb_q */
/*	reset_bit(&rfnm_bm[0],h);		/* reset host's rfnm bit */
	reset_bit(&h_up_bm[0],h);		/* reset host's up   bit */
	kw_reset(h);				/* write reset to kernel */
	hs_update(h,host_dead);			/* update host status file */
}

/*name:
	ir_reset

function:
	handles the imp interface reset message from the imp.

algorithm:
	simply log the occurrance.
	and update status of "host 0".
	Forget all knowledge about currently-up hosts
	and then start resetting the hosts (Greg Noel)

parameters:
	none.

returns:
	nothing.

globals:
	rfnm_bm=	hosts for which a RFNM is outstanding
	init_host=	the host currently being reset

calls:
	log_asc
	h_dead
	rst_all

called by:
	kr_rcv	thru ir_proc

history:
	initial coding 1/8/75 by G. R. Grossman
	update of host 0 status added 5/29/75 by G. R. Grossman
	start resetting hosts via rst_all added 8/31/77 by Greg Noel
	clearing of h_up_bm and rfnm_bm added 1/8/79 by Greg Noel
*/

ir_reset()
{
	register int i;

	log_asc("ir_reset: imp interface reset");
	hs_update( 0, host_alive );	/* set "host 0" alive */
	for(host = 1; host < 256; host++) h_dead();
	for(i = 0; i < 256/8; i++) rfnm_bm[i] = 0;
	init_host = 0;
	rst_all();	/* start resetting hosts */
}

/*name:
	ir_iltyp

function:
	handles messages from the imp with illegal types.

algorithm:
	log the received message.

parameters:
	none.

returns:
	nothing.

globals:
	kr_buf
	krr_ovhd
	kr_bytes

calls:
	log_bin

called by:
	kr_rcv	thru ir_proc

history:
	initial coding 1/8/75 by G. R. Grossman

*/

ir_iltyp()
{
	log_bin("ir_iltyp: bad I-H msg",&kr_buf.lo_byte,
		krr_ovhd+kr_bytes);	/* log error and message */
}

int	(*ir_proc[])()		/* the imp read decoding vector */
{
	&ir_reglr,	/* type 0 regular message */
	&ir_erldr,	/*  "   1 error in leader */
	&ir_igd,	/*  "   2 imp going down  */
	&ir_iltyp,	/*  "   3 illegal type    */
	&ir_nop,	/*  "   4 no-op           */
	&ir_rfnm,	/*  "   5 rfnm		  */
	&ir_hdeds,	/*  "   6 host dead stat  */
	&ir_re_x,	/*  "   7 host/imp dead   */
	&ir_re_x,	/*  "   8 error in data   */
	&ir_re_x,	/*  "   9 incomplete xmit */
	&ir_reset,	/*  "  10 interface reset */
	&ir_iltyp,	/*  "  11 illegal type    */
	&ir_iltyp,	/*  "  12 illegal type    */
	&ir_iltyp,	/*  "  13 illegal type    */
	&ir_iltyp,	/*  "  14 illegal type    */
	&ir_iltyp,	/*  "  15 illegal type    */
};
