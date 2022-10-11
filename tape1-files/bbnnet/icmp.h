struct icmp {				/* icmp header */
	
	unchar ic_type;				/* icmp message type */
	unchar ic_code;				/* icmp message sub-type */
	unsigned short ic_sum;			/* checksum */
	union {
		unchar ic_off;			/* parameter error offset */
		struct socket ic_gaddr;		/* redirect gateway addr */
		struct {
			unsigned short id;	/* echo/timestamp id */
			unsigned short seq;	/* echo/timestamp sequence */
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

struct ggp {			/* gateway-gateway header */
	
	unchar gg_type;				/* ggp type */
	unchar gg_code;				/* ggp subtype */
	unsigned short gg_seq;			/* sequence number */
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
