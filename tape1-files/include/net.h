#ifdef BBNNET

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

typedef unsigned long unlong;
typedef unsigned char unchar;

#else

#define short_to_net(x)	((((x)<<2) & ((unsigned)0776000)) | ((x) & 0377))
#define short_from_net(x) ((((x)>>2) & ((unsigned)0177400)) | ((x) & 0377))
#define long_to_net(x) long_1822(x)
#define long_from_net(x) long_mbb(x)

typedef unsigned int holey_int;
typedef long holey_long;
typedef long unlong;
typedef char unchar;

#endif mbb

typedef unsigned short n_short; /* short as received from the net */
typedef unlong n_long;   	/* long as received from the net */
typedef unlong sequence; 	/* sequence numbers in tcp */
typedef	unlong net_t;		/* network number */

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
        		unchar s_b1;
        		unchar s_b2;
        		unchar s_b3;
        		unchar s_b4;
		}S_un_b;
		struct {
			unsigned short s_w1;
			unsigned short s_w2;
		}S_un_w;
		struct {
			long
#ifndef mbb
				s_la:24,
				s_lb:8;
#else
				s_lx:8,
				s_lb:8,
				s_la:24;
#endif
		}S_un_lab;
		unsigned long s_l;
	} S_un;

#define s_addr  S_un.s_l        /* can be used for most tcp & ip code */
				/* net specific fields */
#define s_neta  S_un.S_un_b.s_b1	/* network (class a) */
#define s_netb  S_un.S_un_w.s_w1	/* network (class b) */
#define s_netc  S_un.S_un_lab.s_la	/* network (class c) */
				/* ARPANET */
#define s_host  S_un.S_un_b.s_b2	/* host on imp */
#define s_imp   S_un.S_un_w.s_w2	/* imp field */
#define s_impno	S_un.S_un_b.s_b4	/* imp number */	
#define s_lh	S_un.S_un_b.s_b3	/* logical host */
};

/* ip addr to net representation */
#define iptonet(x) (net_t)(!((x).s_neta&0x80) ? (x).s_neta \
				: (!((x).s_neta&0x40) ? (x).s_netb \
					: (!((x).s_neta&0x20) ? (x).s_netc \
									: -1)))
/* net representation to right justified long integer */
#define netoint(x) (int)(x)

struct host {                   /* host table entry */
	struct socket h_addr;           /* internet address */
	unchar h_rfnm;			/* # outstanding rfnm's */
	unchar h_refct;			/* reference count */
	unsigned short h_stat;          /* host status */
	struct mbuf *h_outq;		/* -> host msg holding queue */
};

#define HUP 128                 /* host up */

struct net {                    /* network buffer allocation control */
	struct proc *n_proc;            /* -> network process */
	struct inode *n_debug;          /* -> inode of debugging file */
 	struct ipq *n_ip_head;          /* -> top of ip reass. queue */
	struct ipq *n_ip_tail;          /* -> end of ip reass. queue */
	struct tcb *n_tcb_head;         /* -> top of tcp tcb list */
	struct tcb *n_tcb_tail;         /* -> end of tcp tcb list */
	struct work *n_work;            /* -> top of tcp work queue */
	sequence n_iss;                 /* tcp initial send seq # */
	short n_free;                   /* index of top of page free list */
#define netlow n_free			/* page # of lowest mem used by net (mbb) */
	short n_pages;                  /* # pages owned by network */
	short n_bufs;                   /* # free msg buffers remaining */
	short n_hiwat;                  /* max. # free mbufs allocated */
	short n_lowat;                  /* min. # free mbufs allocated */
	unsigned short n_ip_cnt;        /* ip packet counter, used for ids */
	short n_logct;			/* logging queue size */
	short n_loglim;			/* logging queue limit */
	struct mbuf *n_log_hd;		/* logging queue head */
	struct mbuf *n_log_tl;		/* logging queue tail */
	short n_ip_lock;                /* ip reass. queue lock */
};

struct net_stat {               /* interesting statistics */
	int imp_resets;                 /* # times imp reset */
	int imp_flushes;                /* #packets flushed by imp */
	int imp_drops;                  /* #msgs from imp no-one wants */
	int m_drops;                    /* #mbuf drops from lack of bufs */
	int ip_badsum;                  /* #bad ip checksums */
	int t_badsum;                   /* #bad tcp checksums */
	int t_badsegs;                  /* #bad tcp segments */
	int t_unack;                    /* #tcp segs placed on rcv_unack */
	int ic_drops;			/* #bad icmp messages */
	int ic_badsum;			/* #bad icmp checksums */
	int ic_quenches;		/* #icmp source quenches */
	int ic_redirects;		/* #icmp redirects */
	int ic_echoes;			/* #icmp echoes */
	int ic_timex;			/* #icmp time exceeded messages */
};

struct t_debug {                /* tcp debugging record */
        long t_tod;                     /* time of day */
	struct tcb *t_tcb;              /* -> tcb */
	char t_old;                     /* old state */
	char t_inp;                     /* input */
	char t_tim;                     /* timer id */ 
	char t_new;                     /* new state */
	sequence t_sno;                 /* sequence number */
	sequence t_ano;                 /* acknowledgement */
	unsigned short t_wno;           /* window */
	unsigned short t_lno;           /* length */
	unchar t_flg;			/* message flags */
};

#ifdef KERNEL

typedef struct socket netaddr;
struct work *workfree, *workNWORK;      /* ->work entry table */
int nwork;
struct host *host, *hostNHOST;          /* ->start, end of host table */
int nhost;
extern struct net netcb;                /* network control block */
extern struct net_stat netstat;         /* net statistics block */

#ifdef mbb
holey_long long_1822();
long long_mbb();
#endif mbb

#endif KERNEL

#endif BBNNET
