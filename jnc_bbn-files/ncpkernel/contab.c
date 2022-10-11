#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/contab.h"
#include "../h/net.h"

#ifdef NCP                      /* jsq bbn 10/19/78 */
/*name:
	entcontab

function:
	make an entry in the read or write connection table

algorithm:
	search the connection table passed (link & 0200) for a zero
	entry.  If found stick the host, link and socket ptr into
	their resp. locations of the entry

parameters:
	host    - long host number
	link    - link number
	siptr	- address of the socket to be associated with the host link 
		  pair.

returns:
	a pointer to the entry found

globals:
	none

calls:
	incontab	-	 to look for a zero entry in the table

called by:
	wf_setup 

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 10/01/75 by S. F. Holmgren 
	modified for long host numbers by jsq bbn 12-20-78
*/


/* SCCS PROGRAM IDENTIFICATION STRING */
char id_contab[] "~|^`contab.c\tV3.9E0\tJan78\n";
entcontab( host, link, siptr )
long host;
{

	/* Tries to make an entry in contab returns ptr to conentry if
	    found and empty entry or 0 if table full  */

	register struct conentry *conptr;
	register int sps;		/* for storing ps */
	long z;

	if (siptr == 0) {
		return (0);
	}
	sps = PS->integ;		/* save processor prio */
	spl_imp();				/* insure integrity of conn tab */
	link =& 0377;   /* we only want the low byte */
	z = 0;          /* fake a long constant zero */
	if( (conptr = incontab(z, (link&0200),0 )))    /* srch for empty entry */
	{
		conptr -> c_host = host;
		conptr -> c_link = link;
		conptr->c_siptr = siptr;
		siptr -> r_conent = conptr;  /* double link */
	}
	PS->integ = sps;		/* give processor back */
	return( conptr );
}


/*name:
	incontab

function:
	searches the connection table, which is determined by
	the 0200 bit of the link passed .

algorithm:
	determine which connection table to look in
	starts at the beginning word of the connection table and
	searches thru for the first occurance of the search words

parameters:
	host    - long host number
	link    - link number
	iptr	-  possible second match word if != 0

returns:
	pointer to the found entry
	or 0 if no match could be found

globals:
	none

calls:
	return( system )

called by:
	entcontab
	imphostlink
	wf_mod
	wf_setup
	daedes

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 4/28/76 check for siptr match if iptr non-zero
	by S. F. Holmgren
	modified 10/1/75 by S. F. Holmgren
	modified for long host numbers by jsq bbn 12-20-78
*/

incontab( ahost, link, iptr )
long ahost;
{
	register struct conentry *contab;
	register struct conentry *conptr;

	link =& 0377;   /* we only want the low byte */

	/* get the correct entry table from bit 0200 of link.  if
	   bit on use write connection table else read connection tab
	*/
	contab = (link &  0200) ? &w_contab : &r_contab;
	/* if caller is looking for a zero entry then look for a zero entry in
	   proper table
	*/
	if( (link & 0377) == 0200 ) { link = 0; ahost = 0; }
	for( conptr = &contab[0]; conptr < &contab[CONSIZE]; conptr++ )
	if( (ahost == conptr -> c_host) && (link == conptr -> c_link)
		&& (iptr == 0 || iptr == conptr->c_siptr) )
	{
		return( conptr );
	}
	return( 0 );

}
/*

name: ipcontab
function:       take a socket (inode) pointer and see if it has a cor-
	responding connection table entry.
algorithm:
	if ip has a contab pointer and the socket pointer in that entry
	is the same as ip, return the contentry pointer.
parameters:
	ip      -       socket pointer
globals:        none
returns:
	conentry pointer
	zero
calls:  nothing
called by:
history:
	initial coding jsq bbn 12-20-78
*/
ipcontab(aip)
int *aip;
{
	register int ip;
	ip = aip;
	if (ip && (ip -> r_conent) && (ip -> r_conent -> c_siptr == ip))
		return(ip -> r_conent);
	return(0);
}

/*name:
	rmcontab
function:
	This is to be used in conjunction with incontab to search
	and remove entries from the connection table
algorithm:
	stick a zero into the host, link and siptr fields of the entry passed
parameters:
	pointer to a connection table entry
returns:
	nothing
globals:
	none
calls:
	nothing
called by:
	daedes
history:
	initial coding 1/7/75 by S. F. Holmgren
	modified for long host numbers jsq bbn 12-20-78
*/

rmcontab( aentryp )
int *aentryp;
{
	register int *entryp;
	entryp = aentryp;

	if( entryp ) {
		entryp -> c_host = 0;
		entryp -> c_link = 0;
		if (entryp -> c_siptr &&
			(entryp -> c_siptr -> w_conent == entryp))
				entryp -> c_siptr -> w_conent = 0;
		entryp -> c_siptr = 0;
	}
}


/*name:
	infiletab

function:
	look up an entry in the file table

algorithm:
	step thru the the file table looking for a match with the srchwd

parameters:
	srchwd	-  and entry in the file table to look for

returns:
	if entry is found : pointer to entry : else : 0

globals:
	file_tab

calls:
	nothing

called by:
	ncpwrite
	netopen
	daedes

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

infiletab( srchwd )
{

	register *p;

	/* srch all entries */
	for( p = &file_tab[0]; p < &file_tab[ FILSIZE ]; p++ )
		if( *p == srchwd )	/* match buddy */
			return( p );	/* sure sam - here */
	return( 0 );                    /* return not found */
}

#endif NCP
