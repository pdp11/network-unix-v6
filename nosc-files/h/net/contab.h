
/*	this file contains the definitions for the kernel 
			connection table
*/

#ifndef CONSIZE
#define CONSIZE	32
#endif
#ifndef FILSIZE
#define FILSIZE	18
#endif

/*	procedure entcontab, incontab, and rmcontab manage this table  */

struct conentry				/* connection table */
{
	int c_hostlink;			/* host in hibyte; link in lobyte */
	int c_siptr;			/* ptr to i_addr of socket inode */
	int c_localskt;			/* local socket number */
	int c_fskt[2];			/* foreign socket number */
} r_contab[ CONSIZE ];

struct conentry w_contab[ CONSIZE ];


/*	This table contains a list of the valid net file pointers in use  */


int	file_tab[ FILSIZE ];


