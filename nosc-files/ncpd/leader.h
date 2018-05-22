#

/*	leader.h	*/

struct	leader
{
char	l_type;		/* imp message type */
char	l_host;		/* imp host field */
char	l_link;		/* imp link field */
char	l_sbty;		/* imp subtype field */
char	l_fill1;	/* must be zero */
char	l_bysz;		/* host-host byte size */
int	l_bycnt;	/* host_host byte count */
char	l_fill2;	/* must be zero */
char	l_data[];	/* data starts here */
};

