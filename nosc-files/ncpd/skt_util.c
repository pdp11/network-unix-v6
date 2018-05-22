#

/*	skt_util.c	*/

/*	globals declared in this file:
		n_s_left

	functions declared in this file:
		sm_dux
		sm_splx
		free_skt
		att_skt
		get_skt
		kill_skt
		init_skts
		clean_skt
*/


#include	"files.h"
#include	"hstlnk.h"
#include	"socket.h"
#include	"globvar.h"
#include	"kwrite.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char whatid_skt_util[] "~|^`skt_util.c\tV3.9E0\tJan78\n";

int	n_s_left	nsockets;	/* counter for allocating sockets */

/*name:
	sm_dux

function:
	given local and foreign base sockets, and a socket operation
	code ( init or listen), performs the operation on the pair of
	connections so defined, associating local with foreign+1
	and local+1 with foreign.

algorithm:
	compute local socket +1 via skt_off.
	compute foreign +1 via skt_off.
	do the operation to create a receive data socket using local and
	foreign+1 via sm_splx.
	do the operation to create a send data socket using local+1
	and foreign via sm_splx.

parameters:
	lbs_addr	address of local base socket.
	fbs_addr	address of foreign base socket.
	op		socket operation code (must be si_init or si_glsn
			{in which case fbs_addr must be a legal address
			but the content is irrelevant} or si_slsn).

returns:
	nothing.

globals:
	ksx_rcv
	ksx_xmit

calls:
	skt_off
	sm_splx

called by:
	fi_sktnm
	kr_odrct
	fi_rfnm

history:
	initial coding 1/6/75 by G. R. Grossman

*/

sm_dux(lbs_addr,fbs_addr,op)
char	*lbs_addr,*fbs_addr;
int	op;
{
	char	lskt_1[2],	/* holds local socket +1 */
		fskt_1[4];	/* holds foreign socket + 1 */

	skt_off(&lbs_addr[2],&lskt_1[2],1,2);	/* compute local socket +1
						   and store in lskt_1 */

		/* perform operation creating send data socket */
	sm_splx(&lskt_1[0],fbs_addr,ksx_xmit,op);

	skt_off(&fbs_addr[4],&fskt_1[4],1,4);	/* compute foreign socket +1
						   and store in fskt_1 */
		/* perform operation creating receive data socket */
	sm_splx(lbs_addr,&fskt_1[0],ksx_rcv,op);
}

/*name:
	sm_splx

function:
	creates a data socket given local and foreign sockets, socket type,
	and operation.

algorithm:
	set fields of skt_req:
		file pointer from global fp.
		socket type from parameter sinx.
		operation from parameter op.
		byte size 8.
		host from global host.
		if receive:
			link number via asn_lnkn.
		otherwise (send):
			link number to "null" (0377)	(1/10/75 GRG).
		local socket by copying thru parameter ls_addr.
		foreign socket by copying thru parameter fs_addr.
	create/match socket via get_skt.
parameters:
	ls_addr		pointer to local socket number.
	fs_addr		pointer to foreign socket number.
	sinx		socket type.
	op		socket operation code (see remarks under parameters
			for sm_dux).

returns:
	nothing.

globals:
	fp
	host
	skt_req=

calls:
	asn_lnkn
	get_skt

called by:
	sm_dux
	kr_odrct

history:
	initial coding 1/6/75 by G. R. Grossman
	null link # for send socket 1/10/75 by G. R. Grossman
_ifdef SFHBYTE
	added variable bytesize 01/27/78 by S. F. Holmgren
_endif

*/

sm_splx(ls_addr,fs_addr,sinx,op)
char	*ls_addr,*fs_addr;
int	sinx,op;
{
	register char	*sbp,		/* source pointer for copying
					   socket numbers */
			*dbp;		/* dest pointer for same */
	register int	n;		/* counter for same */

	skt_req.s_filep = fp;		/* set file pointer */
	skt_req.s_sinx = sinx;		/* socket type */
	skt_req.s_state = op;		/* socket operation */
#ifndef SFHBYTE
	skt_req.s_bysz = 8;		/* byte size */
#endif
#ifdef SFHBYTE
	skt_req.s_bysz = (fp->f_bysz == 0) ? 8 : fp->f_bysz;	/* byte size */
#endif
	skt_req.s_hstlnk.hl_host = host;/* host number */
	if ( (ls_addr[1] & 1) == 0 )	/* receive socket? */
		skt_req.s_hstlnk.hl_link = asn_lnkn();
			/* assign link number */
	else				/* send socket */
		skt_req.s_hstlnk.hl_link = 0377;	/* "null link #" */
	dbp = &skt_req.s_lskt[0];	/* set dest for socket number copy */
	sbp = ls_addr;			/* ditto for source */
	*dbp++ = *sbp++;		/* 1st byte of local socket */
	*dbp++ = *sbp++;		/* 2nd  "   "    "      "   */
	sbp = fs_addr;			/* set source for foreign socket */
	for (n = 4 ; n ; --n )		/* copy loop */
		*dbp++ = *sbp++;
	get_skt();			/* go do it to the socket */
}

/*name:
	free_skt

function:
	detaches a socket from its file and de-allocates it.

algorithm:
	if the socket is attached to a file:
		detach via fi_sgone.
	increment socket allocation counter.
	set socket state to null ( this makes it available).

parameters:
	skt_p	pointer to socket struct to be detached and deallocated.

returns:
	nothing.

globals:
	n_s_left=

calls:
	fi_sgone

called by:
	cls_q
	cls_clsw
	clo_lsn
	clo_c2cw

history:
	initial coding 1/6/75 by G. R. Grossman
	modified by greep to clear timeout

*/

free_skt(skt_p)
struct socket	*skt_p;
{
	register struct socket	*s_p;

	s_p = skt_p;
	s_p->s_timeo = 0;               /* clear timeout value */
	if ( s_p->s_filep != 0 )	/* attached to file? */
		fi_sgone(s_p);		/* detach */
	n_s_left =+ 1;			/* increment socket allocation
					   counter */
	s_p->s_state = ss_null;		/* set struct to null state, thus
					   deallocating it */
}

/*name:
	att_skt

function:
	attaches a socket to its file.

algorithm:
	the file pointer is obtained from the socket struct.
	the file's appropriate f_skt_x byte is set according to the
	socket's s_sinx field and the socket struct's index.
	the file's attached socket counter is incremented.

parameters:
	skt_p	pointer to socket struct to be attached to its file.

returns:
	nothing.

globals:
	sockets

calls:
	nothing.

called by:
	lsint_q
	so_ulsn

history:
	initial coding 1/6/75 by G. R. Grossman

*/

att_skt(skt_p)
struct socket	*skt_p;
{
	register struct socket	*s_p;	/*points to socket struct */
	register struct file	*f_p;	/* points to associated file struct */

	s_p = skt_p;
	f_p = s_p->s_filep;		/* get pointer to file */
	f_p->f_skt_x[s_p->s_sinx] = s_p - &sockets[0];
		/* set appropriate socket index in file; we use C's
		   conversion facilities to compute index */
	f_p->f_nsatt++;			/* increment file's attached socket
					   count */
}

/*name:
	get_skt

function:
	attempts to fulfill the request in skt_req by finding a socket
	struct in sockets whose local socket matches that is skt_req
	and calling the appropriate procedure thrugh skt_oper.
	if the above fails, calls the appropriate procedure through
	so_unm.

algorithm:
	loop through all socket structs:
		if the socket state is not null and the local sockets match:
			if the procedure called through skt_oper indexed
			by the states of skt_req and the current socket
			returns a non-zero value:
				return.
	if all socket structs are exhausted without terminating:
		call "unmatch" procedure through so_unm indexed by
		state of skt_req.

parameters:
	none.

returns:
	nothing.

globals:
	sockets
	skt_oper
	skt_req
	so_unm

calls:
	those procedures pointed to by:
		skt_oper
		so_unm

called by:
	hr_rfc
	hr_cls
	kr_ouicp
	sm_splx
	kr_osicp

history:
	initial coding 1/6/75 by G. R. Grossman

*/

get_skt()
{
	register struct socket	*p;	/* will point to socket structs
					   as we examine them */

	for ( p = &sockets[0] ; p < &sockets[nsockets] ; p++ )	/* loop thru 
								   all skts */
		if ( (p->s_state != ss_null)		/* socket in use? */
		     && (p->s_lskt->word == skt_req.s_lskt->word) )
							/* and locals match? */
			if ( (*skt_oper[skt_req.s_state][p->s_state])(p) )
				/* did op procedure return non-zero? */
				return;		/* all done if so */
	(*so_unm[skt_req.s_state])(p);	/* if fell out of loop, no match,
					   call appropriate unmatched proc */
}

/*name:
	kill_skt

function:
	performs the "kill" operation on a socket. this operation is
	used when the ncp daemon spontaneously decides to close a connection
	without prior close action by the user program or the foreign host.

algorithm:
	the appropriate procedure is called by indexing skt_oper with
	the kill op code and the state of the socket.

parameters:
	skt_p	pointer to the socket struct to be killed.

returns:
	nothing.

globals:
	skt_oper
	si_kill

calls:
	"kill" procedures in skt_oper.

called by:
	fi_sktnm
	fi_sgone
	fi_all
	fi_rfnm
	ir_re_x

history:
	initial coding 1/6/75 by G. R. Grossman

*/

kill_skt(skt_p)
struct socket	*skt_p;
{
	(*skt_oper[si_kill][skt_p->s_state])(skt_p);
}

/*name:
	init_skts

function:
	initializes socket structs at program initialization.

algorithm:
	loop thru all socket structs:
		set socket state to null.

parameters:
	none.

returns:
	nothing.

globals:
	sockets
	nsockets
	ss_null

calls:
	nothing.

called by:
	main

history:
	initial coding 1/8/75 by G. R. Grossman

*/

init_skts()
{
	register struct socket	*s_p;	/* will point to sockets as we
					   initialize them */

	for ( s_p = &sockets[0] ; s_p < &sockets[nsockets] ; s_p++ )
			/* loop thru all socket structs */
		s_p->s_state = ss_null;		/* set state to null */
}

/*name:
	clean_skt

function:
	clean up the local socket number
	clean up the foreign socket number
	clcean up the host and link numbers

algorithm:
	if state is ss_clow
		zero things of awhile

parameters:
	skt_p	-	address of a socket to be cleaned

returns:
	nothing

globals:
	none

calls:
	nothing

called by:
	cls_rfcw
	cls_cacw
	so_ut1
	so_ut2

history:
	initial coding 7/7/76 by S. F. Holmgren

*/
clean_skt( skt_p )
struct socket *skt_p;
{
	register char *sp;

	if( skt_p->s_state == ss_clow )		/* close wait? */
		for( sp = skt_p; sp < &skt_p->s_state; *sp++ = 0 );
}

