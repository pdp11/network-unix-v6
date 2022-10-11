struct ifcb {           /* net i/f control block */
	struct ifcb *if_next;		/* ->next ifcb */
	char *if_name;			/* name string */
	short if_unit;			/* i/f sub-device number */
	u_char				/* dev indep flags */
		if_avail:1,		/* i/f available */
		if_error:1,		/* error on i/f */
		if_needinit:1,		/* i/f dev needs reset */
		if_active:1,		/* output in progress on i/f */
		if_flush:1,		/* flushing input buffers */
		if_blocked:1,		/* i/f temporarily blocked */
		if_disab:1;		/* disable i/f on init */
	u_char if_flag;			/* device dependent flags */
	u_short if_bufs;		/* no. of bufs queued for this i/f */
	u_short if_limit;		/* maximum bufs for this i/f */
	short if_mtu;          		/* maximum message size */
	short if_link;			/* link to IP protocol for this i/f */
	struct socket if_addr;		/* base internet address for this i/f */
	int if_olen;			/* output length */
	long if_oaddr;        		/* output uba mapping */
	struct mbuf *if_outq_hd;        /* -> output queue head */
	struct mbuf *if_outq_tl;        /* -> output queue tail */
	struct mbuf *if_outq_cur;       /* -> buffer currently being sent */
	int if_ilen;                    /* input length */
	long if_iaddr;        		/* input uba mapping */
	struct mbuf *if_inq_hd;         /* -> input queue head */
	struct mbuf *if_inq_tl;         /* -> input queue tail */
	struct mbuf *if_inq_msg;        /* -> top of input msg being rcvd */
	struct mbuf *if_inq_cur;        /* -> bottom of input msg being rcvd */
	int (*if_send)();               /* -> local net send routine */
	int (*if_rcv)();                /* -> local net input routine */
	int (*if_raw)();		/* -> raw local net output routine */
	int (*if_init)();		/* -> driver init routine */
	int (*if_out)();		/* -> driver output routine */
	int if_opkts;			/* #packets sent */
	int if_ipkts;			/* #packets rcvd */
	int if_resets;			/* #i/f resets */
	int if_flushes;			/* #i/f flushes */
	int if_oerrs;			/* #output errors */
	int if_ierrs;			/* #input errors */
	int if_colls;			/* #collisions */
};

#define if_attach(ip)							\
{									\
	ip->if_next = netcb.n_ifcb_hd;					\
	netcb.n_ifcb_hd = ip;						\
}

struct gway {		/* gateway table entry */
	u_long g_flags;			/* flags */
	net_t g_fnet;			/* foreign net */
	net_t g_lnet;			/* local net */
	struct socket g_local;		/* local gateway address */
	struct ifcb *g_ifcb;		/* -> ifcb for gateway */
};

#define GWROUTE	1			/* routing gateway */
#define GWFORMAT 2			/* gateway format flag */

#ifdef KERNEL
struct gway *gateway, *gatewayNGATE;		/* -> gateway table */
int ngate;
#endif KERNEL
