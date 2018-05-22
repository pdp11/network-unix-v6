#

/*	so_unm.c	*/

/*	globals declared in this file:
		so_unm

	functions declared in this file:
		so_ulsn
		so_uinit
		so_urfc
		so_ucls
*/


#include	"hstlnk.h"
#include	"files.h"
#include	"socket.h"
#include	"globvar.h"
#include	"kwrite.h"
#include	"measure.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_so_unm[] "~|^`so_unm.c\tV3.9E1\t12Mar78\n";

/*name:
	so_ulsn

function:
	directly handles unmatched general and specific listens.
	indirectly handles unmatched inits for so_uinit.

algorithm:
	if a socket is created (via so_urfc):
		attach socket to file via att_skt.
		write setup to kernel via kw_sumod.
		return pointer to socket struct.
	otherwise, failure:
		return 0.

parameters:
	none.

returns:
	pointer to newly created socket if successful.
	0 otherwise.

globals:
	kwi_stup

calls:
	so_urfc
	att_skt
	kw_sumod

called by:
	get_skt	through so_unm.
	so_uinit

history:
	initial coding 1/7/75 by G. R. Grossman

*/

struct socket	*so_ulsn()
{
	extern struct socket *so_urfc();
	register struct socket	*s_p;	/* will point to socket struct, if any
					   we get from so_urfc */

	if ( (s_p = so_urfc()) != 0 )	/* was so_urfc successful? */
	{
		att_skt(s_p);		/* attach socket to file */
		kw_sumod(kwi_stup,s_p,0,0,0,0);	/* write setup to kernel:
						   status = allocation = 0 */
		return(s_p);		/* return pointer to socket */
	}
	else		/* failure */
		return(0);
}

/*name:
	so_uinit

function:
	handles unmatched inits.

algorithm:
	if a socket is created, attached, and set up (via so_ulsn):
		send rfc via hw_rfc.

parameters:
	none.

returns:
	nothing.

globals:
	none.

calls:
	so_ulsn
	hw_rfc

called by:
	get_skt	through so_unm.

history:
	initial coding 1/7/75 by G. R. Grossman

*/

so_uinit()
{
	register struct socket	*s_p;		/* will point to socket struct,
						   if any, we get from so_ulsn 
						*/

	if ( (s_p = so_ulsn()) != 0 )	/* was so_ulsn successful? */
		hw_rfc(s_p);		/* send rfc */
}

/*name:
	so_urfc

function:
	directly handles unmatched rfc's.
	indirectly handles unmatched listens and inits for so_ulsn.

algorithm:
	loop thru all socket structs:
		if socket state is null:
			copy skt_req to socket.
			if the request was actually an rfc:
				decrement socket struct allocation counter
				(1/10/75 GRG).
			return pointer to socket struct.
	if fall out of loop:
		log error.
		return 0.

parameters:
	none.

returns:
	pointer to socket struct if it is successful in allocating one.
	0 otherwise.

globals:
	sockets
	ss_null
	skt_req

calls:
	nothing.

called by:
	get_skt	through so_unm.

history:
	initial coding 1/7/75 by G. R. Grossman
	decrement of n_s_left added 1/10/75 by G. R. Grossman

*/

struct socket	*so_urfc()
{
	register struct socket	*s_p;	/* will point to socket structs
					   during search */
	register char	*sbp,	/* source byte pointer for struct copy */
			*dbp;	/* ditto for dest */

	for ( s_p = &sockets[0] ; s_p < &sockets[nsockets] ; s_p++ )
		/* loop thru all socket structs */
	{
		if ( s_p->s_state == ss_null )	/* this one free? */
		{
			dbp = &s_p->lo_byte;	/* set dest for copy */
			for ( sbp = &skt_req.lo_byte;	/* copy loop */
			      sbp <= &skt_req.s_sinx;
			      *dbp++ = *sbp++ );
			if ( skt_req.s_state == si_rfc )	/* real rfc? */
				n_s_left--;		/* decrement socket
							   allocation counter */
			return(s_p);
		}
	}
	/* error if here */
	log_bin("so_urfc: out of sockets. request is",&skt_req.lo_byte,
		skt_size);	/* log the error */
	return(0);
}

/*name:
	so_ucls

function:
	handles unmatched cls's.

algorithm:
	simply take statistics and log.

parameters:
	none.

returns:
	nothing.

globals:
	measure.m_ucls
	host
	skt_req

calls:
	statist
	log_bin

called by:
	get_skt	through so_unm.

history:
	initial coding 1/7/75 by G. R. Grossman

*/

so_ucls()
{
	statist(&measure.m_ucls);		/* keep statistics */
	log_bin("so_ucls: unmatched cls from host",&host,1);
					/* log host */
	log_bin(".... sockets were",&skt_req.s_lskt[0],6);   /* and sockets */
}

int	(*so_unm[])()
{
	&so_ulsn,	/* 0	glsn */
	&so_ulsn,	/* 1	slsn */
	&so_uinit,	/* 2	init */
	&so_urfc,	/* 3	rfc  */
	&so_ucls,	/* 4	cls  */
};
