#

/*	socket.h	*/

#define	nsockets	4*16	/* 3 for each file plus q'd ones */

struct	socket
{
char	s_lskt[2];		/* local socket */
char	s_fskt[4];		/* foreign socket */
struct	hostlink	s_hstlnk;	/* host and link */
char	s_state;		/* socket state */
char	s_bysz;			/* byte size */
struct	file	*s_filep;	/* points to file owning this socket */
long    s_timeo;                /* timeout value */
char	s_sinx;			/* kernel - file socket index */
}
sockets[nsockets];

#define	skt_size	14	/* size in bytes of socket struct */

int	n_s_left;		/* counter for allocating sockets */

struct	socket	skt_req;	/* the request for the socket machine */

/* socket states */
#define	ss_glsn		0	/* general listen */
#define	ss_slsn		1	/* specific listen */
#define	ss_rfcw		2	/* rfc wait */
#define	ss_q		3	/* foreign rfc queued */
#define	ss_cacw		4	/* cls and close wait */
#define	ss_clsw		5	/* cls wait */
#define	ss_clow		6	/* close wait */
#define	ss_open		7	/* open */
#define	ss_c2cw		8	/* close to cls wait */
#define ss_orfnmw       9       /* open and also in rfnm wait */
#define ss_clorfnmw     10      /* close to cls and also rfnm wait */
#define ss_rfnmw        11      /* rfnm wait */
#define	ss_null		0177777	/* null state to indicate unused struct */

/* socket instruction opcodes */
#define	si_glsn		0	/* general listen */
#define si_slsn		1	/* specific listen */
#define	si_init		2	/* init */
#define	si_rfc		3	/* rfc arrived from foreign host */
#define	si_cls		4	/* cls   "      "     "       "  */
#define	si_close	5	/* close from kernel */
#define	si_kill		6	/* ncp daemon decides to get rid of socket
				   for some private reason such as
				   termination of an associated socket */

#define	si_timo		7	/* timeout */
#define	si_dead		8	/*host died */

/* external declaration for socket machine operation/state matrix */
int     (*skt_oper[9][12])();

/* external declaration for unmatched socket decoding vector */
int	(*so_unm[])();


#define	skt_mpsz	256	/* size of socket number map in bits.
				   must be multiple of 8 */

#define	skt_base	1024	/* lowest socket number to be allocated
				   automatically via asn_sktn. should be
				   a multiple of 8. */

#define STIMEOUT        20      /* number of seconds to timeout socket */
