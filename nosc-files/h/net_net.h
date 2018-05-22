

/*	this file contains the necessary structures and defines needed
	to deal with a network inode.
*/


struct wrtskt				/*  used in writing to net fits over
					    i_addr of an inode
					*/
{
	int w_hostlink;			/*  hibyte host number - lobyte link  */
	int w_bsize;			/*  byte size of the connection  */
	int w_flags;			/*  contains state see defines below  */
	int w_msgs;			/*  num msgs frn host has allocated  */
	char *w_falloc[2];		/*  num of bits frn host has allocated  */
	int  w_fid;			/*  ptr to file asociated this socket  */
	int w_tty;			/*  for server points to addr of tty */
	int *w_wrtproc;			/* process waiting for write to complete */
} ;


struct rdskt
{					/*  used in reading from net  */
	int r_hostlink;			/*  same as wrtskt  */
	int r_bsize;			/*       "        */
	int r_flags;			/*        "         */
	int r_msgs;			/*         "        */
	int r_bytes;			/*  num bytes allocated to frn host  */
	char * r_msgq;			/*  ptr to msg of queued data for
					    process from frn host
					*/
	int r_qtotal;			/*  num bytes in msgq */
	int r_hiwat;			/*  max num bytes to alloc to frn host */
	char *r_rdproc;			/* current process doing the reads */
} ;

/*  these defines are used to isolate bits in the flags field of a read
    or write skt to allow communication between a user process, the kernel
    and the ncp daemon.
*/

/*  flag fields  */
#define n_flgfld	037777
#define n_allocwt	01		/*  indicates skt is awaiting allocation  */
#define n_rfnmwt	02		/*  on skt awaiting rfnm from net  */
#define n_eof		04		/*  skt has been closed  */
#define n_ncpiu		010		/*  ncp still using this skt  */
#define n_usriu		020		/*  user still using this skt  */
#define n_toncp		040		/*  all comm over this goes to ncp  */
#define n_open		0100		/*  skt is open and usable  */
#define n_xmterr	0200		/*  incomplete trans occured  */
#define n_not_used	0400		/*  was for test server 12Jun77 JSK */

/* the following flags defined by S.M. Abraham */

#define	n_allwt		01000		/* write skt waiting for allocation */
#define	n_sendwt	02000		/* write skt waiting for snd to cmplt*/
#define	n_prevmerr	04000		/* err occured on prev send operation*/
#define	n_rcvwt		010000		/* read skt waiting for input */
#define	n_hhchanwt	020000		/* read skt waiting for link 0 to clr*/

/* note that definition for n_flgfld was changed from 0777 to 037777 */
/* note also that the names of some of the other flags are misleading.
   Specifically, n_allocwt is on during the interval of time between when
   the ALL arrives and the process is woken. n_rfnmwt is on during the
   interval between when the RFNM arrives and the process is woken.
   n_xmterr is on during the interval of time between when the error
   is discovered and the process is woken.
*/

#define NETPRI		2		/*  soft pri for sleeping on above bits  */

int ncpopnstate	;			/*  open close state variable */


/*
 * there are a number of things of the nature of what follows that should
 * be collected into a file with the name "netparam.h". This file would
 * then be included into ncpkernel, ncpdaemon, and user-network programs
 * JSK
 */
#define	NOMMSG	10		/* JSK Nominal message allocation */
