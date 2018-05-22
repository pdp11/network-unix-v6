#

/*	kwrite.h	*/

struct	kw			/* structure for kernel writes */
{
char	kw_op;			/* op code */
char	kw_sinx;		/* socket index in file */
int	kw_id;			/* file id */
struct	hostlink	kw_hstlnk;	/* host and link */
char	kw_stat;		/* socket status */
char	kw_bysz;		/* byte size */
char	kw_data[9+120];		/* room for leader and data for send */
}
kw_buf;				/* this is the write assembly buffer itself */


/*	kernel write instruction codes		*/
#define	kwi_send	0
#define	kwi_stup	1	/* setup */
#define kwi_mod		2
#define	kwi_redy	3	/* ready */
#define kwi_clen	4	/* clean */
#define kwi_rset	5	/* reset */
#define kwi_frlse	6	/* file release */
#define kwi_timo        7       /* timeout */

/*	kernel socket status bits	*/
#define	ksb_open	0100
#define	ksb_ddat	0040	/* signifies that the ncp daemon is to be
				  the recipient of any data received for
				  this socket */

/*	kernel socket indices	*/
#define	ksx_rcv		0
#define	ksx_xmit	1
#define	ksx_icp		2

