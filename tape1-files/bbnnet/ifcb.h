struct ifcb {           /* net i/f control block */
	unchar				/* dev indep flags */
		if_avail:1,		/* i/f available */
		if_error:1,		/* error on i/f */
		if_needinit:1,		/* i/f dev needs reset */
		if_active:1,		/* output in progress on i/f */
		if_flush:1,		/* flushing input buffers */
		if_blocked:1;		/* i/f temporarily blocked */
	unchar if_flag;			/* device dependent flags */
	dev_t if_dev;			/* i/f device */
	unsigned short if_bufs;		/* no. of bufs queued for this i/f */
	unsigned short if_limit;	/* maximum bufs for this i/f */
	short if_mtu;          		/* maximum message size */
	net_t if_net;			/* net this i/f is on */
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
};

struct if_local {        /* local host address/ifcb correspondance */
	struct socket if_addr;		/* local internet address */
	struct ifcb *if_ifcb;           /* -> ifcb for this address */
};

/*
 * Dummy structure for initialization.  This ugliness is needed because it is
 * impossible to initialize anything containing a union with C's wonderful
 * initialization syntax.  In particular, the socket struct is composed
 * of several unions.  The dummy structures substitute an unsigned long
 * for the structure so that we can initialize it in netconf.
 */
struct if_localx {       /* dummy local host structure for init */
	long if_addr;			/* local internet address */
	struct ifcb *if_ifcb;		/* -> ifcb for this address */
};

struct gway {		/* gateway table entry */
	unsigned int g_flags;		/* flags */
	net_t g_fnet;			/* foreign net */
	net_t g_lnet;			/* local net */
	struct socket g_local;		/* local gateway address */
	struct ifcb *g_ifcb;		/* -> ifcb for gateway */
};

#define GWSMART	1			/* smart gateway */
#define GWFORMAT 2			/* gateway format flag */

#ifdef KERNEL

struct gway *gateway, *gatewayNGATE;		/* -> gateway table */
int ngate;

#ifndef NETCONF
extern struct ifcb ifcb[];
extern nifcb;
extern struct if_local locnet[];
extern nlocnet;
#endif NETCONF

#endif KERNEL
