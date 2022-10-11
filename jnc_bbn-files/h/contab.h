
/*	this file contains the definations for the kernel 
			connection table
*/
#ifdef NCP                      /* jsq bbn 10/19/78 */
#define CONSIZE	32
#define FILSIZE	18

/*	procedure entcontab, incontab, and rmcontab manage this table  */

struct conentry				/* connection table */
{
	long c_host;                    /* host number */
	int c_link;                     /* link number */
	int c_siptr;			/* ptr to i_addr of socket inode */
	int c_localskt;			/* local socket number */
	int c_fskt[2];			/* foreign socket number */
} r_contab[ CONSIZE ];

struct conentry w_contab[ CONSIZE ];


/*	This table contains a list of the valid net file pointers in use  */


int	file_tab[ FILSIZE ];


#endif                  /* on NCP */
