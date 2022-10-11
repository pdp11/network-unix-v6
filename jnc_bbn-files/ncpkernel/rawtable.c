#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/user.h"
#include "../h/file.h"
#include "../h/inode.h"
#include "../h/reg.h"
#include "../h/buf.h"
#include "../h/net.h"
#include "../h/netbuf.h"
#include "../h/imp.h"
#include "../h/rawnet.h"

#ifdef RMI

/*
 *  raw buffer management:
 *      rawbuf
 *
 *  to manage the raw msg table:
 *      getrawtab, putrawtab, rmrawtab, inrawtab
 */

/*

name:   rawbuf

function:
	get a net buffer, marked raw, and maintain raw buffer use count

algorithm:
	if there are too many raw buffers already in use,
		return none available
	if getbuf returns a net buffer
		check again to see if too many raw buffers in use
			if so, free the buffer and return none available
		else increment count and mark the buffer raw
	return whatever we've got (if anything)

parameters:
	none

globals:
	rbuf_count      (netbuf.h)

calls:
	getbuf
	spl_imp         (sys)
	freebuf

called by:
	rawwrite        (rawmain.c)
	appendb
	rawlget         (rawmain.c)

history:
	initial coding jsq bbn 9-22-78

*/
rawbuf () {
	register struct netbuf *bp;
	register sps;

	if (rbuf_count >= rbuf_max) return (0);
	if (bp = getbuf()) {
		sps = PS -> integ;
		spl_imp();
		if (rbuf_count >= rbuf_max) {
			PS -> integ = sps;
			freebuf (bp);
			return (0);
		}
		rbuf_count++;
		PS -> integ = sps;
		bp -> b_resv =| b_raw;
		return(bp);
	}
	return(0);
}
/*

name:   getrawtab
function:
	find an unused entry in r_rawtab or w_rawtab
	or find the entry for a rawskt
	or find the entry for a host lomsg himsg set
algorithm:
	find the appropriate table
	if riptr was given
		if riptr is -1
			set riptr to zero (for unused entry)
		try to match riptr
	if not
		see if the msg range is not used by the ncp and does
			not exceed the highest possible
		for each entry in the table
			if host matches
				if msgrange matches
					return pointer to entry
				if msgrange overlaps something else
					return -1
	if neither match or overlap, return zero for no find
parameters:
	mode    -       0 is read, 1 is write   -       must be given
	host    -       host                            \
	lomsg   -       low msgrange bound               |  all these
	himsg   -       high msgrange bound             /
	riptr   -       rawskt pointer for hostmsgrange  \   or this must be
			or -1 (means find unused entry)  /   given, but not
							     both
returns:
	0       -       not in rawtab
	-1      -       range overlap with something in the table
				(or with ncp's reserved links)
	entry pointer - if arguments match a table entry
globals:
	r_rawtab        (rawnet.h)
	w_rawtab        (rawnet.h)
calls:
	nothing
called by:
	putrawtab       (rawtable.c)
	rawrel          (rawmain.c)
history:
	initial coding by jsq bbn 9-22-78

*/
getrawtab(mode, host, lomsg, himsg, riptr)
long host;
		/* riptr struct rawskt *riptr; to avoid mode mismatch in */
		/* if (riptr == -1) comparison */
{
	register struct rawentry *rp;
	register struct rawentry *rawtab;

	rawtab = (mode ? &w_rawtab : &r_rawtab);
	if (riptr) {
		if (riptr == -1) riptr = 0;
		for (rp = &rawtab[0]; rp < &rawtab[RAWTSIZE]; rp++) {
			if (rp -> y_rawskt == riptr) return(rp);
		}
		return(0);
	} else {
		if ((lomsg < LOMSG) || (himsg > HIMSG))
			return (-1);
		for (rp = &rawtab[0]; rp < &rawtab[RAWTSIZE]; rp++) {
			if (rp -> y_rawskt) {
				if ((rp -> y_host == host) &&
				    (rp -> y_lomsg == lomsg) &&
				    (rp -> y_himsg == himsg))
					return(rp);
				if (((rp -> y_host == host) ||
					(rp -> y_host == 0) || (host == 0))&&
				    (rp -> y_lomsg <= himsg) &&
				    (rp -> y_himsg >= lomsg))
					return(-1);
			}
		}
		return (0);
	}
}
/*

name:   putrawtab

function:
	put an entry in r_rawtab or w_rawtab: all parameters must be passed

algorithm:
	lock out interrupts
	use getrawtab to see if there is already an entry
	if not,
		if we can make one with getrawtab
			put parameters in it
			if this is new ELSEMSG
				say so (uniqueness is guaranteed by getrawtab)
	restore previous ps
	return appropriate value

parameters:
	mode    -       0 is read, 1 is write
	host    -       host
	lomsg   -       low msgrange bound
	himsg   -       high msgrange bound
	riptr   -       rawskt pointer for hostmsgrange

returns:
	0       -       can't enter
	entry pointer

globals:
	r_rawtab        (rawnet.h)
	w_rawtab        (rawnet.h)
	elseentry       (rawnet.h)

calls:
	getrawtab       (rawtable.c)
	mkrawtab        (rawtable.c)
	spl_imp         (sys)

called by:
	rawopen         (rawmain.c)

history:
	initial coding jsq bbn 9-22-78

*/
putrawtab(mode, host, lomsg, himsg, riptr)
long host;
struct rawskt *riptr;
{
	register struct rawentry *rp;
	register int sps;

	sps = PS -> integ;
	spl_imp();
	if ((rp = getrawtab(mode, host, lomsg, himsg, 0)) == 0) {
		if (rp = getrawtab (mode, ZERO0L, 0, 0, -1)) {
			rp -> y_host = host;
			rp -> y_lomsg = lomsg;
			rp -> y_himsg = himsg;
			rp -> y_rawskt = riptr;
			if ((mode == 0) && (host == 0) &&
			    (lomsg == himsg) && (lomsg == ELSEMSG)) {
				elseentry = rp;
			}
		}
	} else {
		rp = 0;
	}
	PS -> integ = sps;
	return(rp);
}
/*

name:   rmrawtab

function:
	to remove an entry from the raw msg table

algorithm:
	check to see if this entry is same as elseentry
		if so, clear elseentry
	remove entry from table

parameters:
	rentry  -       pointer to a rawtab entry (gotten from getrawtab)

returns:
	nothing

globals:
	elseentry       (rawnet.h)

calls:
	nothing

called by:
	rawrel          (rawmain.c)

history:
	initial coding by jsq bbn 9-22-78
*/
rmrawtab(rentry)
struct rawentry *rentry;
{

	if (rentry) {
		if (rentry == elseentry) {
			elseentry = 0;
		}
		rentry -> y_rawskt = 0;
	}
	return;
}
/*

name:   inrawtab

function:
	to find if a host msg combination is in the range of some
		rawtab entry

algorithm:
	get appropriate table
	if msg is in range used by ncp
		return zero
	for each entry
	    if there is a rawskt for it and it is marked open
		if host matches given or is zero
			if msg is in range
				return entry pointer
	if mode is zero (read)
		return elseentry
	return zero

parameters:
	mode
	host
	msg

returns:
	entry pointer or zero

globals:
	r_rawtab        (rawnet.h)
	w_rawtab        (rawnet.h)

calls:
	nothing

called by:
	rawhostmsg      (rawmain.c)

history:
	initial coding by jsq bbn 9-22-78
	added check to make sure rawskt is open jsq bbn 1-18-79

*/
inrawtab(mode, host, msg)
long host;
{

	register struct rawentry *rp;
	register struct rawentry *rawtab;

	rawtab = (mode ? &w_rawtab : &r_rawtab);

	if (msg < LOMSG) return(0);
	for (rp = &rawtab[0]; rp < &rawtab[RAWTSIZE]; rp++) {
	    if (rp -> y_rawskt && (rp -> y_rawskt -> v_flags & n_open)) {
		if ((rp -> y_host == host) || (rp -> y_host == 0)) {
			if ((rp -> y_lomsg <= msg) &&
			    (rp -> y_himsg >= msg)) {
				return (rp);
			}
		}
	    }
	}
	if (mode == 0) return (elseentry);
	return(0);
}
#endif RMI
