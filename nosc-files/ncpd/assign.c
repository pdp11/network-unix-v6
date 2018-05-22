#

/*	assign.c	*/

/*	globals declared in this file:
		skt_map
		lasktn

	functions declared in this file:
		asn_sktn
		rls_sktn
		asn_lnkn
*/

#include	"files.h"
#include	"hstlnk.h"
#include	"socket.h"
#include	"globvar.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_assign[] "~|^`assign.c\tV3.9E0\tJan78\n";

char	skt_map[skt_mpsz/8];	/* the socket number bit map */

int	lasktn	-1;	/* holds where to start search. negative initialization
			   flags that the bit map has not been initialized. */

int	laslnkn	1;	/* holds where to start search for link numbers */

/*name:
	asn_sktn

function:
	assigns a block of 8 socket numbers.

algorithm:
	if lasktn is negative, the map has not been initialized:
		set lasktn to 0.
		set all bits in skt_map to 1.
	retry:
	if attempt to find a 1 bit in skt_map via find_bit statrting at
	lasktn fails:
		if lasktn is 0, we have failed completely:
			return -1.
		set lasktn to 0.
		goto retry.
	set lasktn to the number allocated+1 for next time.
	reset the bit via reset_bit.
	return the (number*8) + skt_base with bytes swapped.

parameters:
	none.

returns:
	the byte-swapped representation of the base of the assigned
	group of 8 sockets, if successful.
	-1 if not successful.

globals:
	skt_mpsz
	skt_base
	skt_map=
	lasktn=

calls:
	find_bit
	reset_bit
	swab

called by:
	kr_ouicp
	kr_odrct
	kr_osicp

history:
	initial coding 1/7/75 by G. R. Grossman
	bug fixed (forgot to multiply by 8) 1/21/75 by G. R. Grossman.

*/

int	asn_sktn()
{
	register int	n;	/* holds number we allocated. also used as
				   loop control during initialization of
				   bit map */

	if ( lasktn < 0 )	/* first call? */
	{
		lasktn = 0;	/* initialize last socket number */
		for ( n = 0 ; n < skt_mpsz/8 ; n++ )	/* init map */
			skt_map[n] = 0377;		/* all 1's */
	}
retry:	if ( (n = find_bit(&skt_map[0],skt_mpsz,lasktn)) < 0 )
		/* attempt to find a 1 starting at lasktn failed? */
	{
		if ( lasktn == 0 )	/* total failure? */
			return(-1);	/* return failure flag */
		lasktn = 0;		/* otherwise start at bottom */
		goto retry;		/* and try again */
	}
	lasktn = n+1;		/* set new last for next time */
	reset_bit(&skt_map[0],n);	/* reset bit corresponding to 
					   number we allocate */
	return( swab((n<<3)+skt_base) );/* return (number*8) + base with bytes
					   swapped */
}

/*name:
	rls_sktn

function:
	releases a block of 8 socket numbers if the block falls within the
	scope of the socket bit map.

algorithm:
	change number from socket number form back to bit map index:
		swap bytes.
		subtract skt_base.
		divide by 8.
	if number is >=0 and < skt_mpsz:
		set corresponding bit in skt_map via set_bit.

parameters:
	sn	base of a group of 8 socket numbers to be released to the
		free pool.

returns:
	nothing.

globals:
	skt_base
	skt_mpsz
	skt_map=

calls:
	set_bit

called by:
	f_rlse

history:
	initial coding 5/28/75 by G. R. Grossman.

*/

rls_sktn(sn)
int	sn;
{
	register int	n;	/* will hold bit-map index computed from
				   socket number */

	n = ( swab(sn) - skt_base ) >> 3;	/* convert from socket # to
						   bit map index */
	if ( ( n >= 0 ) && ( n < skt_mpsz ) )	/* within scope of bit map? */
		set_bit( &skt_map[0], n );	/* set bit if so */
}

/*name:
	asn_lnkn

function:
	attempts to assign a receive link number for the current host.

algorithm:
	initializes a local bit map to 1's in all legal link number positions
	(these are 2 through 71).
	loop through all socket structs:
		if socket state is not null
		   and host matches
		   and state is not queued
		   and it is a receive socket:
			reset bit corresponding to its link number in
			the bit map.
	if laslnkn ( last link number allocated ) is < 0 ( we failed
	to allocate last time ):
		set laslnkn to 1 (lowest legal link # - 1 )
	attempt to allocate via find_bit on the bit map just constructed,
	starting at laslnkn+1; if fail:
		return, and set laslnkn to, the result of attempting to
		allocate as above, but starting at 2.
	if no failure:
		return, and set laslnkn to, the result of the allocation.

parameters:
	none.

returns:
	2 thru 71	if successful.
	-1		if fails.

globals:
	laslnkn
	host
	sockets
	ss_null
	ss_q

calls:
	reset_bit
	find_bit

called by:
	kr_ouicp
	sm_splx

history:
	initial coding 1/7/75 by G. R. Grossman.
	modified to remember last link number allocated 9/17/75 by
	G. R. Grossman.

*/

int	asn_lnkn()
{
	register struct socket	*s_p;	/* points to socket structs during
					   search */
	register char	*bp;		/* for initializing bit map */
	register int	n;		/* remembers results of search */

	static char	bm[9];		/* for the bit map */

	bp = &bm[0];			/* point at bit map for init */
	*bp++ = 0374;			/* 0 and 1 are illegal */
	for (  ; bp < &bm[9] ; *bp++ = 0377 );	/* init rest to all ones */
	for ( s_p = &sockets[0] ; s_p < &sockets[nsockets] ; s_p++ )
		/* loop through all socket structs */
		if ( (s_p->s_state != ss_null)	/* in use? */
		     &&(s_p->s_hstlnk.hl_host == host.lo_byte)	/* host matches? */
		     &&(s_p->s_state != ss_q)		/* not queued? */
		     &&((s_p->s_lskt[1] & 1) == 0) )	/* and rcv skt? */
			reset_bit(&bm[0],s_p->s_hstlnk.hl_link);
				/* reset bit in map corresponding to link
				   number assigned to socket */
	if ( laslnkn < 0 )	/* failed last time? */
		laslnkn = 1;	/* lowest legal # -1 */
	if ( ( n = find_bit ( &bm[0], 72, laslnkn+1 ) ) < 0 )	/* failure? */
		return ( laslnkn = find_bit ( &bm[0], 72, 2 ) );
				/* try 1 last time */
	return ( laslnkn = n );		/* successful return */
}

