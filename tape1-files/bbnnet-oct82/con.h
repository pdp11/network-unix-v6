struct con {			/* user connection structure */
	u_short c_mode;			/* mode (see below) */
	u_char c_sbufs;			/* # send buffers to use */
	u_char c_rbufs;			/* # rcv buffers to use */
	u_short c_proto;		/* init raw proto number */
	u_char c_timeo;			/* tcp open timeout */
	u_char c_x;			/* (unused) */
	u_short c_lport;		/* local port */
	u_short c_fport;		/* foreign port */
	netaddr c_fcon;			/* foreign net address */
	netaddr c_lcon;			/* local net address */
};

struct netstate {		/* network status structure */
	u_char n_snd;			/* # send bufs allocated */
	u_char n_rcv;			/* # receive bufs allocated */
	u_char n_ssize;			/* # bufs on send buffer */
	u_char n_rsize;			/* # bufs on receive buffer */
	u_short n_xstat;		/* network status word */
	u_short n_state;		/* state of this connection */
	u_short n_flags;		/* misc. flags (see below) */
	u_short n_lport;		/* local port */
	u_short n_fport;		/* foreign port */
	netaddr n_fcon;			/* foreign socket */
	netaddr n_lcon;			/* local socket */
};

				/* c_mode field definitions */
#define CONACT		0000001		/* active connection */
#define CONTCP		0000002		/* open a tcp connection */
#define CONIP		0000004		/* open a raw ip connection */
#define CONRAW		0000010		/* open a raw local net connection */
#define CONCTL		0000020		/* open a control port (no conn) */
#define CONUDP		0000040		/* open a udp connection */
#define CONDEBUG 	0000200		/* turn on debugging info */
#define CONRAWCOMP	0001000		/* system supplies raw leaders */
#define CONRAWVER	0002000		/* system supplies cksum only */
#define CONRAWASIS	0004000		/* user supplies raw leaders */
#define CONRAWERR	0010000		/* user wants raw ICMP error msgs */
#define CONCWAIT	0020000		/* block on TCP close */
#define CONOBLOK	0040000		/* don't block on TCP open */

				/* n_flags field definitions */
#define ULISTEN		CONACT		/* awaiting a connection */
#define UTCP		CONTCP		/* this is a TCP connection */
#define UIP		CONIP		/* this is a raw IP connection */
#define URAW		CONRAW		/* this is a raw 1822 connection */
#define UCTL		CONCTL		/* this is a control port only */
#define UUDP		CONUDP		/* this is a UDP connecetion */
#define UEOL		0000100		/* EOL sent */
#define UDEBUG		CONDEBUG	/* turn on debugging info recording */
#define UURG		0000400		/* urgent data sent */
#define RAWCOMP		CONRAWCOMP	/* system supplies raw headers */
#define RAWVER		CONRAWVER	/* system supplies raw ip cksum only */
#define RAWASIS		CONRAWASIS	/* user supplies raw headers */
#define RAWERR		CONRAWERR	/* give user ICMP error messages */
#define UCWAIT		CONCWAIT	/* wait for TCP close */
#define UNOBLOK		CONOBLOK	/* don't block on TCP open */
#define ULOCK		0100000		/* receive buffer locked */
#define RAWMASK		(RAWCOMP+RAWVER+RAWASIS)
#define CONMASK		(UTCP+UIP+URAW+UCTL+UUDP)

				/* n_state field definitions */
#define UCLOSED		0000		/* connection closed */
#define UCLSERR		0001		/* error -- connection closing */
#define UABORT		0002		/* connection aborted */
#define UINTIMO		0004		/* open failed -- init timeout */
#define URXTIMO		0010		/* retransmit too long timeout */
#define URESET		0020		/* connection aborted due to reset */
#define UDEAD		0040		/* destination dead */
#define UURGENT		0100		/* urgent data received */
#define UNRCH		0200		/* destination unreachable */
#define UDROP		0400		/* raw message dropped */
			
				/* net ioctl definitions */
#define NETGETS 	1		/* get status */
#define NETSETD 	2		/* set debugging info */
#define NETSETU 	3		/* set urgent mode */
#define NETRSETU 	4		/* reset urgent mode */
#define NETSETE 	5		/* set EOL mode */
#define NETRSETE 	6		/* reset EOL mode */
#define NETCLOSE 	7		/* initiate tcp close */
#define NETABORT 	8		/* initiate tcp abort */
#define NETRESET	9		/* forced tcp connection reset */
#define NETDEBUG	10		/* toggle debugging flag */
#define NETGINIT	11		/* re-read gateway file */
#define NETTCPOPT	12		/* set tcp option string */
#define NETPRADD	13		/* add to raw proto list */
#define NETPRDEL	14		/* delete from raw proto list */
#define NETPRSTAT	15		/* return raw proto list */
#define NETROUTE	16		/* override IP routing info */
#define NETOWAIT	17		/* wait for tcp connection estab */
#define NETINIT		18		/* initialize net i/f */
#define NETDISAB	19		/* disable net i/f */

#define SIGURG		16		/* urgent signal */

struct netopt {			/* net ioctl option argument */
	int nio_len;			/* length of argument */
	char *nio_opt;			/* -> argument */
};

#define NIMAX 8
struct netinit {		/* netinit ioctl argument */
	char ni_name[NIMAX];		/* name of device */
	int ni_unit;			/* unit number */
	netaddr ni_addr;		/* network address */
};

