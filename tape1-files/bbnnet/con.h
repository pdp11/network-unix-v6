struct con {                    /* user connection structure */
	unsigned short c_mode;		/* mode (see below) */
	unchar c_sbufs;			/* # send buffers to use */
	unchar c_rbufs;			/* # rcv buffers to use */
	unchar c_lo;			/* low raw link or proto # */
	unchar c_hi;			/* hi raw link or proto # */
	unchar c_timeo;			/* tcp open timeout */
	unchar c_x;			/* (unused) */
	unsigned short c_lport;         /* local port */
	unsigned short c_fport;         /* foreign port */
	netaddr c_fcon;			/* foreign net address */
	netaddr c_lcon;			/* local net address */
};

struct netstate {               /* network status structure */
	unchar n_lo;			/* low link no. in range (IP, RAW) */
	unchar n_hi;			/* high link no. in range (IP, RAW) */
	unchar n_snd;			/* # send bufs allocated */
	unchar n_rcv;			/* # receive bufs allocated */
	unchar n_ssize;			/* # bufs on send buffer */
	unchar n_rsize;			/* # bufs on receive buffer */
	unsigned short n_state;		/* state of this connection */
	unsigned short n_flags;		/* misc. flags (see below) */
	unsigned short n_lport;         /* local port */
	unsigned short n_fport;         /* foreign port */
	netaddr n_fcon;			/* foreign socket */
	netaddr n_lcon;			/* local socket */
};

				/* c_mode field definitions */
#define CONACT		0000001		/* active connection */
#define CONTCP  	0000002		/* open a tcp connection */
#define CONIP   	0000004		/* open a raw ip connection */
#define CONRAW  	0000010		/* open a raw local net connection */
#define CONCTL  	0000020		/* open a control port (no conn) */
#define CONUDP		0000040		/* open a udp connection */
#define CONDEBUG 	0000200		/* turn on debugging info */
#define CONRAWCOMP	0001000		/* system supplies raw leaders */
#define CONRAWVER	0002000		/* system supplies cksum only */
#define CONRAWASIS	0004000		/* user supplies raw leaders */
#define CONRAWERR	0010000		/* user wants raw ICMP error msgs */
#define CONRAWRALL	0020000		/* rcv raw msgs to all local addrs */

				/* net ioctl definitions */
#define NETGETS 1                       /* get status */
#define NETSETD 2                       /* set debugging info */
#define NETSETU 3                       /* set urgent mode */
#define NETRSETU 4                      /* reset urgent mode */
#define NETSETE 5                       /* set EOL mode */
#define NETRSETE 6                      /* reset EOL mode */
#define NETCLOSE 7                      /* initiate tcp close */
#define NETABORT 8                      /* initiate tcp abort */
#define NETRESET 9                      /* forced tcp connection reset */
#define NETDEBUG 10                     /* toggle debugging flag */
#define NETGINIT 11			/* re-read gateway file */
#define NETTCPOPT 12			/* set tcp option string */

#define SIGURG 16               /* urgent signal */

				/* n_flags field definitions */
#define UEOL    0000001			/* EOL sent */
#define UURG    0000002			/* urgent data sent */
#define UDEBUG  0000004			/* turn on debugging info recording */
#define ULOCK   0000010			/* receive buffer locked */
#define UTCP    0000020			/* this is a TCP connection */
#define UIP     0000040			/* this is a raw IP connection */
#define URAW    0000100			/* this is a raw 1822 connection */
#define ULISTEN 0000200			/* awaiting a connection */
#define UCTL    0000400			/* this is a control port only */
#define URMSK	0000560			/* mask for conn mode */
#define RAWCOMP	0001000			/* system supplies raw headers */
#define RAWVER	0002000			/* system supplies raw ip cksum only */
#define RAWASIS	0004000			/* user supplies raw headers */
#define RAWFLAG 0007000			/* mask for raw mode flags */
#define RAWERR	0010000			/* give user ICMP error messages */
#define RAWRALL 0020000			/* accept raw msgs to all local addrs */


				/* n_state field definitions */
#define UCLOSED 0000                    /* connection closed */
#define UCLSERR 0001                    /* error -- connection closing */
#define UABORT  0002                    /* connection aborted */
#define UINTIMO 0004                    /* open failed -- init timeout */
#define URXTIMO 0010                    /* retransmit too long timeout */
#define URESET  0020                    /* connection aborted due to reset */
#define UDEAD	0040                    /* destination dead */
#define UURGENT 0100                    /* urgent data received */
#define UNRCH	0200                    /* destination unreachable */
#define UDROP	0400			/* raw message dropped */
