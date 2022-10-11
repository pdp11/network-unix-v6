#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/user.h"
#include "../h/reg.h"
#include "../h/net.h"
#ifdef NETWORK

int localnet LOCALNET;  /* default:  should be set via sgnet system call */

#ifdef NCP                      /* jsq bbn 10/19/78 */

/*
 *      This file contains six ncp host-to-bitmap utility routines
 *      to set a bit for a host, reset the bit, tell if it is on,
 *      reset all bits in the map,
 *      to sleep on the host number, and to wakeup the previous.
 *
 *      Four bits of host on imp are kept per imp, thus allowing
 *      one word per imp for the bitmap.
 *
 *      There is also good_host, which tells if a host is legal.
 *
 *      The general utility routines stolhost, ltoshost are here.
 *      localnet and netparam are initialized here.
 *      The sgnet system call is herein.
 */
/* SCCS PROGRAM IDENTIFICATION STRING */
/*char id_btest[] "~|^`btest.c\tV3.9E0\tJan78\n";*/
/*
name:   set_host, reset_host, host_on, host_clean

function:
	set, reset, tell whether on, a host in a map per host number arg.

algorithm:
	the host map is an array of integers, which is indexed by the
	imp number of the host.  The bit used in the word corresponds
	to the host on imp number.

	This scheme can only deal with four bits of host on imp number
	(higher numbers would be taken mod 16).  Though it would be possible
	to deal with any number of imps by increasing the length of the
	host map, eventually it would get large enough to make a better
	hashing scheme (such as the one used in the daemon) profitable.

parameters:
	map_addr        the address of the int[] containing the bit map.
	ahost           host number to find the corresponding bit for.

returns:
	nothing, or (0: not set; else: set) for host_on.

globals:
	none.

calls:
	nothing.

called by:
	sendalloc, siguser
history:
	initial	coding 12/12/74	by G. R. Grossman.
	Borrowed with no change	01/23/75 S. F. Holmgren.
	Changed to treat bit_num as a byte 04/31/75 S. F. Holmgren
	changed to use long host numbers instead of a byte host number
	and to use a new host map format jsq bbn 12-20-78.
	names changed from set_bit reset_bit bit_on jsq bbn 1-23-79
*/

set_host(map_addr, ahost)
int     *map_addr;
long    ahost;
{
	map_addr[ahost.h_imp] =| (1 << ahost.h_hoi);
}

reset_host(map_addr, ahost)
int     *map_addr;
long    ahost;
{
	map_addr[ahost.h_imp] =& ~(1 << ahost.h_hoi);
}

host_on(map_addr, ahost)
int     *map_addr;
long    ahost;
{
	return( map_addr[ahost.h_imp] & (1 << ahost.h_hoi));
}

host_clean(map_addr)
int     *map_addr;
{
	register int*q;
	for( q = &map_addr[0]; q < &map_addr[LIMPS]; q++ )
		*q = 0;
}
/*

name: host_sleep, host_wakeup
function:       sleep on or wakeup per host and bitmap.
algorithm:      convert host to 11 bit format and add to bitmap address
		for number to sleep on, then do sleep or wakeup.
parameters:
	map_addr        the address of the int[] containing the bit map.
	ahost           host number to find the corresponding bit for.
returns:        nothing
globals:        none.
calls:          nothing.
called by:
history:        initial coding jsq bbn 12-20-78
*/
host_sleep(map_addr, ahost)
int     *map_addr;
long    ahost;
{
	sleep (map_addr + ((ahost.h_imp&0177) + ((ahost.h_hoi&017) << 7)),
		NETPRI);
}
host_wakeup(map_addr, ahost)
int     *map_addr;
long    ahost;
{
	wakeup (map_addr + ((ahost.h_imp&0177) + ((ahost.h_hoi&017) << 7)));
}
/*

name:   good_host
function:       is host legal?
algorithm:      see code
parameters:     ahost
returns:        true or false
globals:        LHOI, LIMPS, localnet
calls:          nothing
called by:      net_reset       (nopcls.c)
history:
	initial coding jsq bbn 1-24-79
	copy to daemon from kernel jsq BBN 3-20-79
*/
good_host(ahost)
long ahost;
{
	return( (ahost == 0) ||
	    (
		ahost.h_net == localnet &&
		ahost.h_hoi >= 0 && ahost.h_hoi < LHOI &&
		ahost.h_imp > 0 && ahost.h_imp < LIMPS
	    )
	);
}
#endif NCP
/*

name: ltoshost, stolhost - NOT the same as the ones in libn.
function: convert between old and new host formats
algorithm:      see code
parameters:     ltoshost:                       stolhost:
		host - a new format long host   a short host in a char
returns:        short host in an int            long host
		or -1 if can't convert
globals:        localnet                        localnet
calls:          nothing
called by:      rawopen, rawhostmsg, netopen
history:
	initial coding jsq bbn 1-22-79
*/
ltoshost(ahost)
long ahost;
{
	register imp, hoi;
	hoi = ahost.h_hoi;
	imp = ahost.h_imp;
	if (ahost && ((ahost.h_net != localnet) ||
		(imp > 077) || (hoi > 03))) return (-1);
	return (imp | (hoi << 6));
}

long stolhost (ac)
char ac;
{
	register i;
	long ahost;

	if (i = ac & 0377) {
		ahost.h_imp = i & 077;
		ahost.h_hoi = (i & 0300) >> 6;
		ahost.h_net = localnet;
	} else {
		ahost = 0;
	}
	return (ahost);
}

/*

	This is the initialization of netparam, the network parameter
	structure which can be gotten from the kernel by the table system
	call.
*/

struct netparam netparam {
	0,              /* this gets changed when a NOP comes from the imp */
	0
#ifdef NETWORK
	| net_network
#endif NETWORK
#ifdef SHORT
	| net_short
#endif SHORT
#ifdef NCP
	| net_ncp
#endif NCP
#ifdef RMI
	| net_raw
#endif RMI
#ifdef GATEWAY
	| net_gate
#endif GATEWAY
#ifdef MSG
	| net_msg
#endif MSG
	,0      /* this is ncpopnstat, which is set by rwkopen or ncpopen */
};

/*

name: sgnet
function:       to implement the sgnet system call
algorithm:      if user's r0 is zero, return localnet
		else set localnet to value from user's r0
			unless a daemon is up, when return ENCP2
			or not superuser, whence EPERM.
parameters:     none
returns:        see above
globals:        localnet
called by:      system
calls:          nothing
history:        initial coding jsq bbn 1-9-79
*/
sgnet() {
	register r;
	if ((r = u.u_ar0[R0]) == 0) {
		u.u_ar0[R0] = localnet;
	} else {
		if (ncpopnstate) u.u_error = ENCP2;
		else if (!suser()) u.u_error = EPERM;
		else localnet = r;
	}
}
#endif NETWORK
