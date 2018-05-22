#

/*	hstat.c		*/

#include "files.h"
#include "globvar.h"
#include "probuf.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_hstat[] "~|^`hstat.c\tV3.9E0\tJan78\n";

/*	globals declared in this file:
		hs_file
		host_dead
		host_alive

	functions declared in this file:
		hs_init
		hs_update
		hs_alive
*/

int	hs_file		-1;	/* host status file descriptor */

char	host_dead[2]	{ 0377, 0341 };	/* default host dead code,
					   see BBN report 1822 */

char	host_alive[2]	{ 0, 0 };	/* host alive code */

/*name:
	hs_init

function:
	initializes the host status file.

algorithm:
	creates host status file via creat.
	marks us (0'th entry of host_status) alive
	writes the host dead code on the file for every host.

parameters:
	none.

returns:
	nothing.

globals:
	hs_file=
	host_dead

calls:
	creat	(sys)

	write	(sys)
called by:
	main

history:
	initial coding 5/28/75 by G. R. Grossman
	modified to mark 0'th entry alive, 3/4/77, S. M. Abraham

*/

hs_init()
{
	int	h;	/* will count thru host numbers */

	hs_file = creat("/etc/host_status",0744);	/* create host status file */
	write( hs_file, host_alive, 2) ; 	/* mark us alive */
	for ( h = 1 ; h < 256 ; h++ )		/* count thru host numbers */
		write ( hs_file, host_dead, 2);	/* mark 'em all dead */
}
/*name:
	hs_update

function:
	updates host status file entry for a host.

algorithm:
	since each entry takes 2 bytes, multiply host number by 2.
	seek to that byte position.
	write two bytes from the given address.

parameters:
	hnum	number of the host whose status is to be updated.
	stat	address of two byte vector containing the update info.

returns:
	nothing.

globals:
	hs_file

calls:
	seek	(sys)
	write	(sys)

called by:
	ir_igd
	ir_hdeds
	h_dead

history:
	initial coding 5/28/75 by G. R. Grossman

*/

hs_update(hnum,stat)
int	hnum;
char	stat[];
{
	seek ( hs_file, hnum << 1, 0 );	/* seek to right spot in file */
	write ( hs_file, stat, 2);	/* write new status in file */
}

/*name:
	hs_alive

function:
	mark a host alive

algorithm:
	set the hosts bit in the host up bit map
	call hs_update to get it out into the status file

parameters:
	none

returns:
	nothing

globals:
	host
	h_up_bm

calls:
	set_bit
	hs_update

called by:
	hr_nop
	hr_rfc
	hr_eco
	hr_rst
	hr_rrp

history:
	initial coding 6/07/76 by S. F. Holmgren

*/
hs_alive()
{

	set_bit( &h_up_bm[0],host );	/* host alive internally */
	hs_update( host,host_alive );	/* host alive externally */
}

