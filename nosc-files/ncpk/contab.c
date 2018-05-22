#
#include "param.h"
#include "net_contab.h"
#include "net_net.h"

#ifdef SCCSID
/* SCCS PROGRAM IDENTIFICATION STRING */
char id_contab[] "~|^`contab.c\tV3.9E0\tJan78\n";
#endif


/*name:
	entcontab

function:
	make an entry in the read or write connection table

algorithm:
	search the connection table passed (hstlink & 0200) for a zero
	entry.  If found stick the hostlink and socket ptr into
	their resp. locations of the entry

parameters:
	hstlink	- word contain the host(hibyte) and link(lobyte) for the entry
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

*/


entcontab( hstlink,siptr )
{

	/* Tries to make an entry in contab returns ptr to conentry if
	    found and empty entry or 0 if table full  */

	register struct conentry *conptr;
	register int sps;		/* for storing ps */

	sps = PS->integ;		/* save processor prio */
	spl_imp();				/* insure integrity of conn tab */
	if( (conptr = incontab( (hstlink&0200),0 )))	/* srch for empty entry */
	{
		conptr->c_hostlink = hstlink;
		conptr->c_siptr = siptr;
	}
	PS->integ = sps;		/* give processor back */
	return( conptr );
}


/*name:
	incontab

function:
	searches the connection table, which is determined by
	the 0200 bit of the word passed .

algorithm:
	determine which connection table to look in
	starts at the beginning word of the connection table and
	searches thru for the first occurance of the srchword

parameters:
	srchwd	-  the word to look for in the conection table
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

*/

incontab( srchwd,iptr )
{

/*	This searches through the connection table looking for a match
	between c_hostlink and srchwrd		*/

	register struct conentry *contab;
	register struct conentry *conptr;

	/* get the correct entry table from bit 0200 of srchwrd.  if
	   bit on use write connection table else read connection tab
	*/
	contab = (srchwd &  0200) ? &w_contab : &r_contab;
	/* if caller is looking for a zero entry then look for a zero entry in
	   proper table 
	*/
	if( (srchwd & 0377) == 0200 )  srchwd = 0;
	for( conptr = &contab[0]; conptr < &contab[CONSIZE]; conptr++ )
	if( (srchwd == conptr->c_hostlink) && (iptr == 0 || iptr == conptr->c_siptr) )
	{
		return( conptr );
	}
	return( 0 );

}


/*name:
	rmcontab

function:
	to make the connection entry available for further use

algorithm:
	stick a zero into the hostlink field of the entry passed

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

*/

rmcontab( entryp )
{

/*	This is to be used in conjunction with incontab to search
	and remove entries from the connection table  */

	if( entryp )
		entryp->c_hostlink = 0;

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
	return( 0 );			/* return not found */
}
