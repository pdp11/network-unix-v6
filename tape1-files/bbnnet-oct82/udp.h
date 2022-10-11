struct udp {			/* user dgram proto leader (fits over ip hdr) */
	char u_x[8];			/* unused fields from ip */
	u_char u_x1;			/* unused */
	u_char u_pr;			/* protocol number */
	u_short u_ilen;			/* pseudo header length == UDP length */
	struct socket u_s;		/* source internet address */
	struct socket u_d;		/* destination internet address */
	u_short u_src;			/* source port */
	u_short u_dst;			/* destination port */
	u_short u_len;			/* length */
	u_short u_sum;			/* checksum */
};

#define UDPROTO	17
#define UDPSIZE 8			/* UDP header only */
#define UDPCKSIZE 12			/* UDP pseudo header */
