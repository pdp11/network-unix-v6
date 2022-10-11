struct vii {			/* v2lni leader */
	u_char v_dst;			/* destination address */
	u_char v_src;			/* source address */
	u_char v_ver;			/* version */
	u_char v_pr;			/* protocol */
	u_short v_len;			/* length (internal only) */
};

#define VIIMTU	2046
#define VIIVERSION 2
#define VIIPROTO 1
				/* ifcb device specific flags */
#define VIIRETRY 7			/* # of timeout retries */
