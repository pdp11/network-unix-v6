struct icmp {				/* icmp header */
	u_char ic_type;				/* icmp message type */
	u_char ic_code;				/* icmp message sub-type */
	u_short ic_sum;	 			/* checksum */
	union {
		u_char ic_off;			/* parameter error offset */
		struct socket ic_gaddr;		/* redirect gateway addr */
		struct {
			u_short id;		/* echo/timestamp id */
			u_short seq;		/* echo/timestamp sequence */
		} ic_iseq;
	} ic_misc;
	union {
		struct th hdr;			/* ip/tcp hdr of orig d'gram */
		struct {
			long torig;		/* originate timestamp */
			long trecv;		/* receive timestamp */
			long ttrans;		/* transmit timestamp */
		} ic_time;
		char data[1];			/* echo data */
	} ic_data;
};

#define ICMPROTO 1
					/* icmp message types */
#define ICMP_ECHOR	0			/* echo reply */
#define ICMP_UNRCH	3			/* destination unreachable */
#define		ICMP_UNRCH_NET		0		/* net unreachable */
#define		ICMP_UNRCH_HOST		1		/* host unreachable */
#define		ICMP_UNRCH_PR		2		/* protocol unrch */
#define		ICMP_UNRCH_PORT		3		/* port unreachable */
#define 	ICMP_UNRCH_FRAG		4		/* DF on fragment */
#define		ICMP_UNRCH_SRC		5		/* bad source route */
#define	ICMP_SRCQ	4			/* source quench */
#define ICMP_REDIR	5			/* redirect */
#define 	ICMP_REDIR_NET		0		/* network */
#define		ICMP_REDIR_HOST		1		/* host */
#define		ICMP_REDIR_TNET		2		/* TOS & network */
#define		ICMP_REDIR_THOST	3		/* TOS & host */
#define ICMP_ECHO	8			/* echo */
#define ICMP_TIMEX	11			/* time exceeded */
#define 	ICMP_TIMEX_XMT		0		/* in transit */
#define		ICMP_TIMEX_REASS	1		/* reassembly */
#define ICMP_PARM	12			/* parameter problem */
#define ICMP_TIMES	13			/* timestamp */
#define ICMP_TIMESR	14			/* timestamp reply */
#define ICMP_INFO	15			/* information request */
#define ICMP_INFOR	16			/* information reply */

struct ggp {			/* gateway-gateway header */
	
	u_char gg_type;				/* ggp type */
	u_char gg_code;				/* ggp subtype */
	u_short gg_seq;				/* sequence number */
	union {
		struct {
			struct socket raddr;	/* redirect gateway addr */
			struct th rhdr;		/* ip/tcp hdr of orig d'gram */
		} gg_redir;
		struct th gg_hdr;		/* ip/tcp hdr of orig d'gram */
		char data[1];			/* echo data */
	} gg_data;
};
 
#define GGPROTO 3
				/* GGP message types */
#define GGP_ECHOR	0			/* echo reply */
#define GGP_ROUTE	1			/* routing update */
#define GGP_ACK		2			/* acknowledgement */
#define GGP_UNRCH	3			/* destination unreachable */
#define		GGP_UNRCH_NET		0		/* net unreachable */
#define		GGP_UNRCH_HOST		1		/* host unreachable */
#define	GGP_SRCQ	4			/* source quench */
#define GGP_REDIR	5			/* redirect */
#define GGP_ECHO	8			/* echo */
#define GGP_STAT	9			/* net interface status */
#define GGP_NACK	10			/* negative acknowledgement */
