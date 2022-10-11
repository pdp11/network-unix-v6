
#define NCON 20                         /* maximum number of connections */
#define NHOST  20                       /* maximum number of foreign hosts */
#define NGATE	20			/* maximum size of gateway table */
#define NWORK 20                        /* # of work entries in table */

#define TRUE  1
#define FALSE 0
#ifndef HZ
#define HZ 60
#endif

/*
 * The following macros define how shorts and longs are modified when
 * they are sent out on the net or received from the net as part of a
 * header.  In the case of the VAX, the bytes must be swapped.  On the c70,
 * a 2 bit "hole" must be inserted in the top  bits of each byte before
 * being sent out, or squeezed out when received.
 */
#ifndef mbb

#define short_to_net(x) ntohs(x)
#define short_from_net(x) ntohs(x)
#define long_to_net(x) ntohl(x)
#define long_from_net(x) ntohl(x)

#else

#define short_to_net(x)	((((x)<&lt;2) & ((unsigned)0776000)) | ((x) & 0377))
#define short_from_net(x) ((((x)>>2) & ((unsigned)0177400)) | ((x) & 0377))
#define long_to_net(x) long_1822(x)
#define long_from_net(x) long_mbb(x)

typedef unsigned int holey_int;
typedef long holey_long;
typedef long u_long;
typedef char u_char;

#endif mbb

typedef unsigned short n_short; /* short as received from the net */
typedef u_long n_long;   	/* long as received from the net */
typedef u_long sequence; 	/* sequence numbers in tcp */
typedef	u_long net_t;		/* network number */

struct work {                   /* worklist entry */
	short w_type;                   /* what to do with entry */
	short w_stype;                  /* subtype for timer names */
	struct tcb *w_tcb;              /* -> tcb for entry */
	char *w_dat;                    /* -> work data chain */
	struct work *w_next;            /* -> next work entry */
};

struct socket {                 /* all purpose internet address */
				/* (can be extended for any network) */
	union {
		struct {
        		u_char s_b1;
        		u_char s_b2;
        		u_char s_b3;
        		u_char s_b4;
		}S_un_b;
		struct {
			u_short s_w1;
			u_short s_w2;
		}S_un_w;
#ifndef mbb
		struct {
			long
				s_la:24,
				s_lb:8;
		}S_un_lab;
#endif
		u_long s_l;
	} S_un;

#define s_addr  S_un.s_l        /* can be used for most tcp & ip code */
				/* net specific fields */
#define s_neta  S_un.S_un_b.s_b1	/* network (class a) */
#define s_netb  S_un.S_un_w.s_w1	/* network (class b) */
#ifndef mbb
#define s_netc  S_un.S_un_lab.s_la	/* network (class c) */
#define netc(x) (x).s_netc
#else
#define	netc(x) ((((x).s_l)&0xfffffffc00) >> 10)
#endif
				/* ARPANET */
#define s_host  S_un.S_un_b.s_b2	/* host on imp */
#define s_imp   S_un.S_un_w.s_w2	/* imp field */
#define s_impno	S_un.S_un_b.s_b4	/* imp number */	
#define s_lh	S_un.S_un_b.s_b3	/* logical host */
				/* V2LNI */
#define s_vhst	S_un.S_un_b.s_b4	/* ring address */
};

/* ip addr to net representation */
#define iptonet(x) (net_t)(!((x).s_neta&0x80) ? (x).s_neta \
				: (!((x).s_neta&0x40) ? (x).s_netb \
					: (!((x).s_neta&0x20) ? netc(x) \
									: -1)))
/* net representation to right justified long integer */
#define netoint(x) (long)(x)

/* make an ip addr */
#ifndef mbb
#define ipaddr(w,x,y,z) ((long)(((z)<&lt;24)|((y)<&lt;16)|((x)<&lt;8)|(w)))
#else
#define ipaddr(w,x,y,z) ((long)(((long)(w)<&lt;30)|((long)(x)<&lt;20) \
				|((long)(y)<&lt;10)|(long)(z)))
#endif

struct host {                   /* host table entry */
	struct socket h_addr;           /* internet address */
	u_char h_rfnm;			/* # outstanding rfnm's */
	u_char h_refct;			/* reference count */
	u_short h_stat;			/* host status */
	struct mbuf *h_outq;		/* -> host msg holding queue */
};

#define HUP 	128		/* host up */
#define HTEMP 	64		/* temp host entry (goes away if h_rfnm==0) */

struct net {                    /* network buffer allocation control */
	struct proc *n_proc;		/* -> network process */
	struct inode *n_debug;		/* -> inode of debugging file */
 	struct ucb *n_ucb_hd;		/* -> top of ucbs */
	struct ifcb *n_ifcb_hd;		/* -> top of ifcbs */
 	struct ipq *n_ip_head;		/* -> top of ip reass. queue */
	struct ipq *n_ip_tail;		/* -> end of ip reass. queue */
	struct proto *n_ip_proto;	/* -> ip raw proto chain */
	struct tcb *n_tcb_head;		/* -> top of tcp tcb list */
	struct tcb *n_tcb_tail;		/* -> end of tcp tcb list */
	struct proto *n_tcp_proto;	/* -> tcp raw proto chain */
	struct proto *n_udp_proto;	/* -> udp raw proto chain */
	struct proto *n_icmp_proto;	/* -> icmp raw proto chain */
	struct proto *n_ggp_proto;	/* -> ggp raw proto chain */
	struct work *n_work;		/* -> top of tcp work queue */
	sequence n_iss;			/* tcp initial send seq # */
	short n_free;			/* index of top of page free list */
#define netlow n_free			/* page # of lowest mem used by net (mbb) */
	short n_pages;			/* # pages owned by network */
	short n_bufs;                   /* # free msg buffers remaining */
	short n_hiwat;			/* max. # free mbufs allocated */
	short n_lowat;			/* min. # free mbufs allocated */
	u_short n_ip_cnt;		/* ip packet counter, used for ids */
	short n_logct;			/* logging queue size */
	short n_loglim;			/* logging queue limit */
	struct mbuf *n_log_hd;		/* logging queue head */
	struct mbuf *n_log_tl;		/* logging queue tail */
	short n_ip_lock;		/* ip reass. queue lock */
};

struct net_stat {               /* interesting statistics */
	int m_drops;			/* #mbuf drops from lack of bufs */
	int net_drops;			/* local net msgs no one wants */
	int ip_badsum;			/* #bad ip checksums */
	int ip_drops;			/* #ip packets not addressed to us */
	int t_badsum;			/* #bad tcp checksums */
	int t_badsegs;			/* #bad tcp segments */
	int t_unack;			/* #tcp segs placed on rcv_unack */
	int ic_drops;			/* #bad icmp messages */
	int ic_badsum;			/* #bad icmp checksums */
	int ic_quenches;		/* #icmp source quenches */
	int ic_redirects;		/* #icmp redirects */
	int ic_echoes;			/* #icmp echoes */
	int ic_timex;			/* #icmp time exceeded messages */
	int u_badsum;			/* #udp bad checksums */
	int u_drops;			/* #udb drops */
};

struct buf_stat {		/* buffer usage statistics */
	int b_cons;			/* active connections */
	int b_useq;			/* user send+rcv queues */
	int b_work;			/* in transit on work list */
	int b_devq;			/* device input/output queues */
	int b_rfnm;			/* blocked for rfnm */
	int b_ipfr;			/* ip fragment reassembly queues */
	int b_tcbs;			/* tcbs */
	int b_tseq;			/* tcp sequencing queues */
	int b_tuna;			/* tcp unacked message queues */
};

struct t_debug {                /* tcp debugging record */
        long t_tod;			/* time of day */
	struct tcb *t_tcb;		/* -> tcb */
	char t_old;			/* old state */
	char t_inp;			/* input */
	char t_tim;			/* timer id */ 
	char t_new;			/* new state */
	sequence t_sno;			/* sequence number */
	sequence t_ano;			/* acknowledgement */
	u_short t_wno;			/* window */
	u_short t_lno;			/* length */
	u_char t_flg;			/* message flags */
};

#ifdef KERNEL

typedef struct socket netaddr;
struct work *workfree, *workNWORK;      /* ->work entry table */
int nwork;
struct host *host, *hostNHOST;          /* ->start, end of host table */
int nhost;
extern struct net netcb;                /* network control block */
extern struct net_stat netstat;         /* net statistics block */
#ifdef BUFSTAT
extern struct buf_stat bufstat;		/* buffer stats */
#endif BUFSTAT

#ifdef mbb
holey_long long_1822();
long long_mbb();
#endif mbb

#endif KERNEL
