#

/*	skt_oper.c	*/

/*	globals declared in this file:
		skt_oper

	functions declared in this file:
		so_ignr
		so_glchk
		so_fseq
		so_match
		glsn_q
		lsint_q
		rfc_util
		rfc_rfcw
		rfc_slsn
		rfc_glsn
		cls_rfcw
		cls_q
		cls_cacw
		cls_clsw
		cls_open
		clo_lsn
		clo_rfop
		clo_cacw
		clo_c2cw
		so_ut1
		so_ut2
		so_ut3
		tmo_q
		stimeout
*/

#include	"hstlnk.h"
#include	"files.h"
#include	"socket.h"
#include	"kwrite.h"
#include	"globvar.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_skt_oper[] "~|^`skt_oper.c\tV3.9E0\tJan78\n";


/*name:
	so_ignr

function:
	"place holder" for null intersections in socket operation matrix.

algorithm:
	does nothing but return0 to indicate no match.

parameters:
	skt_p	pointer to socket struct with which it does nothing.

returns:
	zero.

globals:
	none.

calls:
	nothing.

called by:
	get_skt	thru skt_oper.

history:
	initial coding 1/7/75 by G. R. Grossman.

*/

int	so_ignr(skt_p)
struct socket	*skt_p;
{
	return(0);
}

/*name:
	so_glchk

function:
	given pointers to a socket struct with possible "wild" fields and
	a normal (tame) socket struct, check for match of host and foreign
	socket fields taking anything as a match for a wild field.
	if a match is made, copy tame fields into wild socket.

algorithm:
	if host field of wild socket is 0 (i.e., wild)
	or the host fields are equal:
		if foreign socket field of wild socket is 0 (i.e., wild)
		or foreign socket fields match via so_fseq:
			copy host and foreign socket fields from tame to
			wild socket.
			return 1.
	if any conditions are not met:
		return 0.

parameters:
	w_skt_p		pointer to socket struct with possible "wild" fields.
	t_skt_p		pointer to normal (tame) socket struct.

returns:
	1	iff a match was made.
	0	otherwise.

globals:
	s_*
	host=

calls:
	so_fseq

called by:
	glsn_q
	rfc_glsn

history:
	initial coding to replace so_cphfs 6/10/75 by G. R. Grossman.
	Bug 1/6/76 S. F. Holmgren:
	global host assigned from queued rfc so protocol resulting
	from the match will go to the right host.

*/

int	so_glchk ( w_skt_p, t_skt_p )
struct socket	*w_skt_p, *t_skt_p;
{
	register char	*tp,	/* points to "tame" struct for compares */
				/* and its foreign socket field for copy */
			*wp;	/* same for "wild" */

	register int	n;	/* counter for same */

	wp = w_skt_p;		/* copy pointers to registers */
	tp = t_skt_p;
	if (    ( wp->s_hstlnk.hl_host == 0 )	/* host wild? */
	     || ( wp->s_hstlnk.hl_host == tp->s_hstlnk.hl_host ) )
			/* or hosts =? */
	{
		if (    ( ( wp->s_fskt[0].word | w_skt_p->s_fskt[2].word )
			  == 0 )	/* foreign socket wild? */
		     || so_fseq ( wp, tp ) )	/* or foreign sockets
							   equal? */
		{
			wp->s_hstlnk.hl_host = tp->s_hstlnk.hl_host;
			host = (tp->s_hstlnk.hl_host & 0377);
				/* copy host from tame to wild */
			tp = &tp->s_fskt[0];	/* set source for copy */
			wp = &wp->s_fskt[0];	/* ditto for dest */
			for ( n = 4 ; n > 0 ; n-- )	/* copy four bytes */
				*wp++ = *tp++;
			return ( 1 );	/* indicate success */
		}
	}
	return ( 0 );	/* indicate failure */
}

/*name:
	so_fseq

function:
	compares foreign socket fields of two socket structs.

algorithm:
	establish char pointers to the two fields to be compared.
	step thru the fields a byte at a time:
		if bytes are not equal:
			return 0.
	if fall out of loop, fields are equal:
		return 1.

parameters:
	lsp	pointer to one of the socket structs whose foreign socket
		fields are to be comared.
	rsp	pointer to the other such socket struct.

returns:
	1	iff the two foreign sockets are byte-for-byte equal.
	0	otherwise.

globals:
	s_*

calls:
	nothing.

called by:
	so_glchk
	so_match

history:
	initial coding 6/10/75 by G. R. Grossman

*/

int	so_fseq ( lsp, rsp )
struct socket	*lsp, *rsp;
{
	register char	*lbp,	/* one of two pointers used for compare */
			*rbp;	/* other such pointer */
	register int	n;	/* counter for compare */

	lbp = &lsp->s_fskt[0];	/* set pointers */
	rbp = &rsp->s_fskt[0];
	for ( n = 4 ; n > 0 ; n-- )	/* compare four bytes */
		if ( *lbp++ != *rbp++ )		/* bytes not equal? */
			return ( 0 );		/* failure return */
	return ( 1 );		/* success return if fall out of loop */
}

/*name:
	so_match

function:
	determines whether host and foreign socket of skt_req and specified
	socket are equal.

algorithm:
	if hosts do not match:
		return 0.
	return result of match of foreign sockets via so_fseq.

parameters:
	skt_p	pointer to socket struct which is to be matched with
		skt_req.

returns:
	1	if host and foreign sockets match.
	0	if they do not.

globals:
	skt_req
	s_*

calls:
	so_fseq

called by:
	lsint_q
	rfc_util
	cls_rfcw
	cls_q
	cls_cacw
	cls_clsw
	cls_open

history:
	initial coding 1/7/75 by G. R. Grossman
	revised to use so_fseq 6/10/75 by G. R. Grossman

*/

int	so_match(skt_p)
struct socket	*skt_p;
{
	if ( skt_req.s_hstlnk.hl_host != skt_p->s_hstlnk.hl_host )
			/* hosts not equal? */
		return(0);
	return ( so_fseq ( &skt_req.lo_byte, skt_p ) );	/* return result of 
						   compare of foreign sockets */
}

/*name:
	glsn_q

function:
	handles the case in which a general listen matches a queued rfc.

algorithm:
	if a "wild" match for host and foreign socket is made via so_glchk:
		return result returned by lsint_q.
	otherwise:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if some failure was detected by lsint_q.
	1	if lsint_q was successful.

globals:
	none.

calls:
	so_glchk
	lsint_q

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	revised to use so_glchk 6/10/75 by G. R. Grossman.

*/

int	glsn_q(skt_p)
struct socket	*skt_p;
{
	if ( so_glchk ( &skt_req.lo_byte, skt_p ) )	/* match? */
		return(lsint_q(skt_p));	/* let lsint_q do the work */
	return ( 0 );		/* otherwise failure return */
}

/*name:
	lsint_q

function:
	directly handles the cases in which an attempt is being made to
	match a specific listen or init with a queued rfc.
	indirectly handles the general listen - queued rfc case for
	glsn_q.

algorithm:
	if host and foreign sockets match (via so_match):
		if receive socket:
			copy link number from skt_req.
		otherwise:
			copy byte size from skt_req.
		copy file pointer and socket type from skt_req.
		attach socket to file via att_skt.
		set socket state to open.
		increment socket struct allocation counter to take into
		account the fact that it was decremented in open to
		reserve a socket struct and also in so_urfc when the
		struct was actually allocated to the queued rfc. (1/10/75 GRG)
		write setup to kernel (state = open, allocations = 0)
		via kw_sumod.
		send answering rfc via hw_rfc.
		pass open to file level via fi_sopn.
		return 1.
	otherwise, no match:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	skt_req
	ss_open
	kwi_stup
	ksb_open

calls:
	so_match
	att_skt
	kw_sumod
	hw_rfc
	fi_sopn

called by:
	get_skt	through skt_oper.
	glsn_q

history:
	initial coding 1/7/74 by G. R. Grossman
	increment of n_s_left added 1/10/75 by G. R. Grossman

*/

int	lsint_q(skt_p)
struct socket	*skt_p;
{
	register struct socket	*s_p;	/* will point to socket struct */

	s_p = skt_p;
	if ( so_match(s_p) )	/* host and foreign socket match? */
	{
		if ( (skt_req.s_lskt[1] & 1) == 0 )	/* receive socket? */
			s_p->s_hstlnk.hl_link = skt_req.s_hstlnk.hl_link;
				/* copy link number */
		else					/* send socket */
			s_p->s_bysz = skt_req.s_bysz;	/* copy byte size */
		s_p->s_filep = skt_req.s_filep;		/* copy file ptr */
		s_p->s_sinx = skt_req.s_sinx;		/* socket type */
		att_skt(s_p);		/* attach socket to file */
		s_p->s_state = ss_open;	/* set socket state to open */
		n_s_left++;		/* this socket struct was accounted
					   for during the original
					   queuing operation in so_urfc and
					   also during the open. this increment
					   resolves the conflict. */
		kw_sumod(kwi_stup,s_p,ksb_open,0,0,0);	/* kernel setup:
							   status = open,
							   0 allocation */
		hw_rfc(s_p);		/* send answering rfc */
		fi_sopn(s_p);		/* tell file level about open */
		return(1);		/* all done successfully */
	}
	else	/* no match */
		return(0);
}

/*name:
	rfc_util

function:
	indirectly handles the case in which an attempt is being made to
	match an incoming rfc with a socket in the rrc wait state.
	also indirectly handles the case of matching incoming rfc's with
	listens for rfc_slsn.

algorithm:
	if hosts and foreign sockets match ( via so_match):
		if receive socket:
			copy byte size from skt_req.
		otherwise:
			copy link number from skt_req.
		set socket state to open.
		return 1.
	otherwise, no match:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	skt_req
	ss_open

calls:
	so_match

called by:
	rfc_rfcw
	rfc_slsn

history:
	initial coding 1/7/74 by G. R. Grossman, as rfc_rfcw
	revised as rfc_util 6/11/75 by G. R. Grossman.

*/

int	rfc_util(skt_p)
struct socket	*skt_p;
{
	register struct socket	*s_p;	/* will point to socket struct */

	s_p = skt_p;
	if ( so_match(s_p) )	/* host and foreign socket match? */
	{
		if ( (skt_req.s_lskt[1] & 1) == 0 )	/* receive socket? */
			s_p->s_bysz = skt_req.s_bysz;	/* copy byte size */
		else					/* send socket */
			s_p->s_hstlnk.hl_link = skt_req.s_hstlnk.hl_link;
				/* copy link number */
		s_p->s_state = ss_open;	/* set socket state to open */
		return(1);		/* all done successfully */
	}
	else	/* no match */
		return(0);
}

/*name:
	rfc_rfcw

function:
	handles the case in which an attempt is being made to match
	an incoming rfc with a socket in the rfc wait state.

algorithm:
	if call on rfc_util is successful:
		pass to file level via fi_sopn.
		return 1.
	otherwise:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	none.

calls:
	rfc_util
	fi_sopn

called by:
	get_skt thru skt_oper.

history:
	initial coding 1/7/75 by G. R. Grossman
	completely recoded 6/11/75 by G. R. Grossman.


*/

int	rfc_rfcw ( skt_p )
struct socket	*skt_p;
{
	if ( rfc_util ( skt_p ) )	/* match found via rfc_util? */
	{
		fi_sopn ( skt_p );	/* pass to file level */
		return ( 1 );		/* successful return */
	}
	return ( 0 );			/* failure return */
}

/*name:
	rfc_slsn

function:
	directly handles the case in which an attempt is being made to
	match an incoming rfc with a specific listen.
	indirectly handles the case in which an incoming rfc is matched
	to a general listen for rfc_glsn.

algorithm:
	if a call on rfc_util is successful:
		send answering rfc via hw_rfc.
		pass to file level via fi_sopn.
		return 1.
	otherwise:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	none.

calls:
	rfc_util
	fi_sopn
	hw_rfc

called by:
	get_skt	through skt_oper.
	rfc_glsn

history:
	initial coding 1/7/74 by G. R. Grossman
	revised to use rfc_util and call fi_sopn 6/11/75 by G. R. Grossman.

*/

int	rfc_slsn(skt_p)
struct socket	*skt_p;
{
	if ( rfc_util(skt_p) )	/* match a success? */
	{
		hw_rfc(skt_p);	/* send answering rfc */
		fi_sopn ( skt_p );	/* pass to file level */
		return(1);
	}
	else
		return(0);
}

/*name:
	rfc_glsn

function:
	handles the case in which an incoming rfc matches a general listen.

algorithm:
	if a "wild" match is made via so_glchk:
		return result returned by calling so_slsn.
	otherwise:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	none.

calls:
	so_glchk
	rfc_slsn

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	revised to use so_glchk 6/10/75 by G. R. Grossman.

*/

int	rfc_glsn(skt_p)
struct socket	*skt_p;
{
	if ( so_glchk ( skt_p, &skt_req.lo_byte ) )	/* match? */
		return(rfc_slsn(skt_p));	/* let him do the work */
	return ( 0 );		/* failure return */
}

/*name:
	cls_rfcw

function:
	handles the case in which an attempt is being made to match an
	incoming cls with a socket in the rfc wait state.

algorithm:
	if the host and foreign socket match (via so_match):
		send clean to kernel via kw_clean.
		send cls to host via hw_cls.
		set timeout for returning cls.
		set socket to state to close wait.
		clean up local and foreign skt num and host - link fields
		return 1.
	otherwise, no match:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	ss_clow

calls:
	so_match
	kw_clean
	hw_cls
	clean_skt
	stimeout

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified to call clean_skt 7/7/76 by S. F. Holmgren
	modified by greep for timeout

*/

int	cls_rfcw(skt_p)
struct socket	*skt_p;
{
	if ( so_match(skt_p) )	/* host and foreign socket match? */
	{
		kw_clean(skt_p);	/* send clean to kernel */
		hw_cls(skt_p);		/* send cls to host */
		skt_p->s_state = ss_clow;	/* set socket state to close
						   wait */
		stimeout(skt_p);
		clean_skt( skt_p );	/* clean up part of socket info */
		return(1);	/* indicate success */
	}
	else		/* no match */
		return(0);
}

/*name:
	cls_q

function:
	handles the case in which an attempt is being made to match an
	incoming cls with a socket in the queued state.

algorithm:
	if the host and foreign socket match ( via so_match):
		send cls to host via hw_cls.
		de-allocate socket via free_skt.
		return 1.
	otherwise:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	none.

calls:
	so_match
	hw_cls
	free_skt

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman

*/

int	cls_q(skt_p)
struct socket	*skt_p;
{
	if ( so_match(skt_p) )	/* host and foreign socket match? */
	{
		hw_cls(skt_p);  /* send answering cls */
		free_skt(skt_p);	/* de-allocate socket */
		return(1);
	}
	else
		return(0);
}

/*name:
	cls_cacw

function:
	handles the case in which an attempt is being made to match
	an incoming cls with a socket in the close and cls wait state.

algorithm:
	if the host and foreign socket match ( via so_match):
		set socket state to close wait.
		clean up part of socket info
		cancel timeout
		return 1.
	else
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	ss_clow

calls:
	so_match
	clean_skt

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified to call clean_skt 7/7/76 by S. F. Holmgren

*/

int	cls_cacw(skt_p)
struct socket	*skt_p;
{
	if ( so_match(skt_p) )	/* host and foreign socket match? */
	{
		skt_p->s_state = ss_clow;	/* set state to close wait */
		clean_skt( skt_p );		/* clean up socket */
		return(1);
	}
	else
		return(0);
}

/*name:
	cls_clsw

function:
	handles the cse in which an attempt is being made to match
	an incoming cls with a socket in the cls wait state.

algorithm:
	if the host and foreign socket match ( via so_match):
		cancel the timeout
		de-allocate the socket via free_skt.
		return 1.
	else
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	none.

calls:
	so_match
	free_skt

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified by greep for timeout

*/

int	cls_clsw(skt_p)
struct socket	*skt_p;
{
	if ( so_match(skt_p) )	/* host and foreign socket match? */
	{
		free_skt(skt_p);	/* de-allocate socket */
		return(1);
	}
	else
		return(0);
}

/*name:
	cls_open

function:
	handles the case in which an attempt is being made to match
	an incoming cls with a socket in the open state.

algorithm:
	if the host and foreign socket match (via so_match):
		send clean to kernel via kw_clean.
		set socket state to close to cls wait.
		return 1.
	otherwise:
		return 0.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	0	if there was no match.
	1	if there was a match and the operation was performed.

globals:
	ss_c2cw

calls:
	so_match
	kw_clean

called by:
	get_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman

*/

int	cls_open(skt_p)
struct socket	*skt_p;
{
	if ( so_match(skt_p) )	/* host and foreign socket match? */
	{
		kw_clean(skt_p);	/* send clean to kernel */
		skt_p->s_state = ss_c2cw;	/* set state to close to cls
						   wait */
		return(1);
	}
	else
		return(0);
}

/*name:
	clo_lsn

function:
	performs a close operation on a socket in the general or specific
	listen state.

algorithm:
	send clean to kernel via kw_clean.
	de-allocate socket via free_skt.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	none.

calls:
	kw_clean
	free_skt

called by:
	hr_close	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman

*/

clo_lsn(skt_p)
struct socket	*skt_p;
{
	kw_clean(skt_p);	/* send clean to kernel */
	free_skt(skt_p);	/* de-allocate socket */
}

/*name:
	clo_rfop

function:
	performs a close operation on a socket in the rfc wait or open state.

algorithm:
	send cls to host via hw_cls.
	send clean to kernel via kw_clean.
	set socket state to cls wait.
	set timeout for returning cls.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	ss_clsw

calls:
	hw_cls
	kw_clean
	stimeout

called by:
	hr_close	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified by greep for timeout

*/

clo_rfop(skt_p)
struct socket	*skt_p;
{
	hw_cls(skt_p);		/* send cls to host */
	kw_clean(skt_p);	/* send clean to kernel */
	skt_p->s_state = ss_clsw;	/* set state to cls wait */
	stimeout(skt_p);
}

/*name:
	clo_cacw

function:
	performs a close operation on a socket in the close and cls wait
	state.

algorithm:
	set socket state to cls wait.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	none.

calls:
	nothing.

called by:
	hr_close	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified by greep for timeout

*/

clo_cacw(skt_p)
struct socket	*skt_p;
{
	skt_p->s_state = ss_clsw;	/* set state to cls wait */
	stimeout(skt_p);
}

/*name:
	clo_c2cw

function:
	performs a close operation on a socket in the close to cls state.

algorithm:
	send cls to host via hw_cls.
	de-allocate socket via free_skt.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	none.

calls:
	hw_cls
	free_skt

called by:
	hr_close	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman

*/

clo_c2cw(skt_p)
struct socket	*skt_p;
{
	hw_cls(skt_p);		/* send cls to host */
	free_skt(skt_p);	/* de-allocate socket */
}

/*name:
	so_ut1

function:
	performs the following operations on sockets in the following
	states:
		operation	states
		kill		general listen
				specific listen
		timeout		general listen
				specific listen
		dead		general listen
				specific listen
				rfc wait
				open

algorithm:
	send clean to kernel via kw_clean.
	set socket state to close wait.
	clean up part of the socket info

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	ss_clow

calls:
	kw_clean
	clean_skt

called by:
	kill_skt	through skt_oper.
	h_dead		   "       "    .

history:
	initial coding 1/7/74 by G. R. Grossman
	modified to call clean_skt 7/7/76 by S. F. Holmgren

*/

so_ut1(skt_p)
struct socket	*skt_p;
{
	kw_clean(skt_p);	/* send clean to kernel */
	skt_p->s_state = ss_clow;	/* set state to close wait */
	clean_skt( skt_p );		/* clean up part of the socket */
}

/*name:
	so_ut2

function:
	performs the following operations on sockets in the following
	states:
		operation	state
		timeout		close and cls wait
		dead		close and cls wait
				close to cls wait

algorithm:
	set state to close wait.
	clean up part of the socket 

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	ss_clow

calls:
	clean_skt

called by:
	h_dead	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified to call clean_skt 7/7/76 by S. F. Holmgren

*/

so_ut2(skt_p)
struct socket	*skt_p;
{
	skt_p->s_state = ss_clow;	/* set state to close wait */
	clean_skt( skt_p );		/* clean up part of socket info */
}

/*name:
	so_ut3

function:
	performs the following operations on sockets in the following
	states:
		operation	state
		kill		rfc wait
				open
		timeout		rfc wait

algorithm:
	send cls to host via hw_cls.
	send clean to kernel via kw_clean.
	set socket state to close and cls wait.
	set timeout for returninc cls.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	ss_cacw

calls:
	hw_cls
	kw_clean

called by:
	kill_skt	through skt_oper.

history:
	initial coding 1/7/74 by G. R. Grossman

*/

so_ut3(skt_p)
struct socket	*skt_p;
{
	hw_cls(skt_p);		/* send cls to host */
	kw_clean(skt_p);	/* send clean to kernel */
	skt_p->s_state = ss_cacw;	/* set to close and cls state */
}
/*name:
	tmo_q

function:
	performs the timeout operation on a socket in the queued rfc state.

algorithm:
	send cls to host via hw_cls.
	set socket state to cls wait.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	ss_clsw

calls:
	hw_cls

called by:
	currently not called. included for completeness.

history:
	initial coding 1/7/74 by G. R. Grossman
	modified by greep for timeout although this routine is not called

*/

tmo_q(skt_p)
struct socket	*skt_p;
{
	hw_cls(skt_p);		/* send cls to host */
	skt_p->s_state = ss_clsw;	/* set state to cls wait */
	stimeout(skt_p);
}

/*name:
	stimeout

function:
	sets the timeout for a socket

algorithm:
	set timeout value in socket.
	increment stimeo.

parameters:
	skt_p	pointer to socket struct on which operation is to be
		performed.

returns:
	nothing.

globals:
	stimeo

calls:
	nothing

called by:
	clo_rfop
	so_ut3
	cls_rfcw

history:
	initial coding by greep

*/
stimeout(skt_p)
struct socket	*skt_p;
{
	long int tim;

	time ( &tim );
	skt_p->s_timeo = tim + STIMEOUT;
	stimeo++;
}
/*	new action routines from BBN to solve problem with ICP from TIP.
	documentation is needed.
*/


cls_orfnmw(skt_p)
struct socket *skt_p;
{
	if ( so_match(skt_p) )
	{
		kw_clean(skt_p);
		skt_p->s_state = ss_clorfnmw;
		return(1);
	}
	else
		return(0);
}


clo_clorfnmw(skt_p)
struct socket *skt_p;
{
	hw_cls(skt_p);
	skt_p->s_state = ss_rfnmw;
}


ki_clorfnmw(skt_p)
struct socket *skt_p;
{
	skt_p->s_state = ss_c2cw;
}
/*skt_oper
*/
int	(*skt_oper[9][12])()
{
/* si_glsn */
&so_ignr,	&so_ignr,	&so_ignr,	&glsn_q,	&so_ignr,
&so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,
&so_ignr,       &so_ignr,

/* si_slsn */
&so_ignr,	&so_ignr,	&so_ignr,	&lsint_q,	&so_ignr,
&so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,
&so_ignr,       &so_ignr,

/* si_init */
&so_ignr,	&so_ignr,	&so_ignr,	&lsint_q,	&so_ignr,
&so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,
&so_ignr,       &so_ignr,

/* si_rfc */
&rfc_glsn,	&rfc_slsn,	&rfc_rfcw,	&so_ignr,	&so_ignr,
&so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,
&so_ignr,       &so_ignr,

/* si_cls */
&so_ignr,	&so_ignr,	&cls_rfcw,	&cls_q,		&cls_cacw,
&cls_clsw,      &so_ignr,       &cls_open,      &so_ignr,       &cls_orfnmw,
&so_ignr,       &so_ignr,

/* si_close */
&clo_lsn,	&clo_lsn,	&clo_rfop,	&so_ignr,	&clo_cacw,
&so_ignr,       &free_skt,      &clo_rfop,      &clo_c2cw,      &clo_rfop,
&clo_clorfnmw,  &so_ignr,

/* si_kill */
&so_ut1,	&so_ut1,	&so_ut3,	&so_ignr,	&so_ignr,
&so_ignr,       &so_ignr,       &so_ut3,        &so_ignr,       &so_ut3,
&ki_clorfnmw,   &free_skt,

/* si_timo */
&so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,
&free_skt,      &so_ignr,       &so_ignr,       &so_ignr,       &so_ignr,
&so_ignr,       &so_ignr,

/* si_dead */
&so_ut1,	&so_ut1,	&so_ut1,	&free_skt,	&so_ut2,
&free_skt,      &so_ignr,       &so_ut1,        &so_ut2,        &so_ut1,
&so_ut2,        &free_skt

}
;
