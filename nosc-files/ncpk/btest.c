#
/*
 *	This file contains three bit utility routines one to set a bit
 *	one to reset a bit and one to tell if it is one.
 */
#include "param.h"

#ifdef SCCSID
/* SCCS PROGRAM IDENTIFICATION STRING */
char id_btest[] "~|^`btest.c\tV3.9E0\tJan78\n";
#endif
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
	sendalloc

history:
	initial coding 12/12/74 by G. R. Grossman.
	Borrowed with no change 01/23/75 S. F. Holmgren.
	Changed to treat bit_num as a byte 04/31/75 S. F. Holmgren

*/

set_bit(map_addr,bit_num)
char	*map_addr;
char	bit_num;
{
	map_addr[(bit_num&0377)>>3] =| (1 << (bit_num & 07));
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
	siguser

history:
	initial coding 12/12/74 by G. R. Grossman.
	borrowed with no change 01/23/75 S. F. Holmgren.
	Changed to treat bit_num as a byte 04/31/75 S. F. Holmgren

*/

reset_bit(map_addr,bit_num)
char	*map_addr;
char	bit_num;
{
	map_addr[(bit_num&0377)>>3] =& ~(1 << (bit_num & 07));
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
	siguser
	sendalloc

history:
	initial coding 12/12/74 by G. R. Grossman.
	borrowed with no change 01/23/75 S. F. Holmgren
	Changed to treat bit_num as a byte 04/31/75 S. F. Holmgren

*/

int	bit_on(map_addr,bit_num)
char	*map_addr;
char	bit_num;
{
	return( map_addr[(bit_num&0377)>>3] & (1 << (bit_num & 07)) );
}

