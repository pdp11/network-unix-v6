#

/*	util.c		*/

/*	globals declared in thsi file:
		queue

	functions declared in this file:
		q_enter
		q_dlink
		set_bit
		reset_bit
		bit_on
		find_bit
*/


/* SCCS PROGRAM IDENTIFICATION STRING */
char id_util[] "~|^`util.c\tV3.9E0\tJan78\n";

struct	queue		/* defines q link as a structure for the queue
			   manipulation procedures */
{
	struct	queue	*q_link;
};

/*name:
	q_enter

function:
	enters an element in a queue.

algorithm:
	if the queue isempty (q head = 0), point the element at itself
	and point the q head at it.
	otherwise,
		point the element at the current 1st element.
		point the current last element at the element.
		point the q head at the element, which thus becomes the
		new last element.

parameters:
	qha	the address of the q head.
	elemp	the address of the element to be put at the end of 
		the q.

returns:
	nothing.

globals:
	host= (for error message)

calls:
	nothing.

called by:
	qup_pro

history:
	initial coding 12/11/74 by G. R. Grossman.
	modified 31Aug77 by J.S.Kravitz to complain if zero
	passed as element address.

*/

q_enter(qha,elemp)
struct queue	**qha,
		*elemp;
{
	extern int host;	/* JSK 31Aug77 */

	if (!elemp)
	{
		log_bin ("Qenter (x,0)", &host, 1);
		return;
	}
	if ( *qha == 0 )	/* is q empty? */
		*qha = elemp->q_link = elemp;	/* point element at itself
						   and q head at element */
	else	/* q wasn't empty */
	{
		elemp->q_link = qha->q_link->q_link;	/* point element at
							   current 1st
							   element */
		qha->q_link = 
			qha->q_link->q_link =
				elemp;		/* point last element at
						   new element, q head at
						   new element, which thus
						   becomes new last element */
	}
}

/*name:
	q_dlink

function:
	delinks the 1st entry in a queue and returns a pointer to it.
	returns zero if the q was empty.

algorithm:
	if the queue is non-empty (q head != 0):
		set result to address of current 1st element.
		delink current 1st element.
		if this reveals that it was the only element:
			zero the q head.
	otherwise set result to zero.
	return result.

parameters:
	qha	the address of the q head from which the 1st element
		is to be delinked.

returns:
	address of the former 1st element of the q, if q was non-empty.
	otherwise (q was empty) returns zero.

globals:
	none.

calls:
	nothing.

called by:
	ir_rfnm
	pb_flush

history:
	initial coding 12/12/74 by G. R. Grossman.

*/

struct	queue	*q_dlink(qha)
struct	queue	**qha;
{
	register struct queue	**qhp,		/* will hold qha for
						   efficiency */
				*result;	/* holds result temporarily */

	qhp = qha;
	if ( qhp->q_link != 0 )		/* is q non-empty? */
	{
		result = qhp->q_link->q_link;	/* 1st element in q is pointed
						   to by last element which
						   is pointed to by head */
		if ( (qhp->q_link->q_link = qhp->q_link->q_link->q_link)
		     == result )	/* is the new 1st element the same
					   as the old one? */
			qhp->q_link = 0;	/* mark q empty if so */
		return ( result );		/* return pointer to former
						   1st element */
	}	/* end q was not empty */
	else
		return(0);	/* signifies q was empty */
}

/*name:
	set_bit

function:
	sets a bit in a bit map.

algorithm:
	the bit map is considered to be an array of bytes each of which
	contains 8 bits numbered from the low-order bit. the byte is
	selected by taking the bit number / 8; the correct bit is set
	by or'ing the byte with 1 shifted left (bit number mod 8).

parameters:
	map_addr	the address of the char[] containing the bit map.
	bit_num		number of the bit to be set.

returns:
	nothing.

globals:
	none.

calls:
	nothing.

called by:
	asn_lnkn
	send_pro

history:
	initial coding 12/12/74 by G. R. Grossman.

*/

set_bit(map_addr,bit_num)
char	*map_addr;
int	bit_num;
{
	map_addr[bit_num>>3] =| (1 << (bit_num & 07));
}

/*name:
	reset_bit

function:
	reset a bit in a bit_map.

algorithm:
	same as set_bit except that instead of or, the logical function
	is "and not".

parameters:
	same as set_bit.

returns:
	nothing.

globals:
	none.

calls:
	nothing.

called by:
	ir_rfnm
	ir_re_x

history:
	initial coding 12/12/74 by G. R. Grossman.

*/

reset_bit(map_addr,bit_num)
char	*map_addr;
int	bit_num;
{
	map_addr[bit_num>>3] =& ~(1 << (bit_num & 07));
}

/*name:
	bit_on

function:
	tests a bit in a bit map.

algorithm:
	same as set_bit, but logical function is and and the result is
	returned.

parameters:
	same as set_bit.

returns:
	zero if the bit is off.
	a one in the bit position of the byte if the bit is on.

globals:
	none.

calls:
	nothing.

called by:
	send_pro

history:
	initial coding 12/12/74 by G. R. Grossman.

*/

int	bit_on(map_addr,bit_num)
char	*map_addr;
int	bit_num;
{
	return( map_addr[bit_num>>3] & (1 << (bit_num & 07)) );
}
/*name:
	find_bit

function:
	finds the 1st "one" bit in a bit mapfrom a given starting point.

algorithm:
	if start < map_size:
		take map_size /8 (now in bytes)
		set i to start / 8
		set n to start mod 8
		set m 1*(2**n)   {mask for testing bits}
		go to commence
		outer loop:
			increment i and fall out if >= map_size.
			if map_addr[i] !=0:
				set m to 1
	commence:		loop on n from 0 to 7:
					if bit n of byte i is set:
						return bit number
					otherwise:
						shift m left 1 for next test
	if start was >= map_size or fell out of search loop, return -1.

parameters:
	map_addr	address of the char[] containing the bit map.
	map_size	size of map in bits (should be multiple of 8).
	start		bit number at which to start search (1st bit in
			map is numbered 0).

returns:
	if the search is successful, the number of the 1st set bit whose
	number is >= start.
	otherwise, -1.

globals:
	none.

calls:
	nothing.

called by:
	asn_sktn
	asn_lnkn

history:
	initial coding 12/12/74 by G. R. Grossman.

*/

int	find_bit(map_addr,map_size,start)
char	*map_addr;
int	map_size,
	start;
{
	register int	i,	/* will hold byte number in map */
			m,	/* will hold testing mask */
			n;	/* will hold bit  number in byte */

	if ( start < map_size )		/* start within map? */
	{
		map_size =>> 3;		/* size now in bytes */
		i = start >> 3;		/* starting byte */
		n = start & 07;		/* starting bit */
		m = 1 << n;		/* starting test mask */
		goto commence;		/* enter loop in appropriate place */
		for (	; i < map_size ; i++ )	/* loop thru map bytes */
			if ( map_addr[i] != 0 )	/* this byte have any 1's? */
			{
				m=1;		/* set initial test mask */
				for ( n = 0 ; n < 8 ; n++ ) /* loop thru bits */
				{
commence:			/* here's where we enter loop 1st time */
					if ( map_addr[i] & m ) /* bit on? */
						return( (i<<3) | n );
							/* bit number is 
							   (byte number)*8
							   + (bit number) */
					m =<< 1;	/* otherwise shift mask 
							*/
				}
			}
	}
	return ( -1 );	/* search failed */
}

