struct ip {                     /* ip leader */
#ifdef mbb
	u_char ip_x1:2,			/* (unused) */
		ip_v:4,			/* version */
		ip_hl:4;		/* header length */
#else	/* vax */
	u_char ip_hl:4,			/* header length */
		ip_v:4;			/* version */
#endif
	u_char ip_tos;			/* type of service */
#define ip_mff ip_tos                   /* more fragments flag (input) */
	u_short ip_len;			/* total length */
	u_short ip_id;			/* identification */
	u_short ip_off;			/* fragment offset field */
#define ip_df 0x4000                    /* dont fragment flag */
#define ip_mf 0x2000                    /* more fragments flag (output) */
	u_char ip_ttl;			/* time to live */
	u_char ip_p;			/* protocol */
	u_short ip_sum;			/* checksum */
#define ip_end ip_sum                   /* fragment end */
	union {
		struct socket ip_s;     /* source address */
		struct ip *ip_nxt;      /* ->next fragment */      
	} I_sun;
#define ip_src  I_sun.ip_s
#define ip_next I_sun.ip_nxt
	union {
		struct socket ip_d;     /* destination address */
		struct ip *ip_prv;      /* ->prev fragment */
	} I_dun;
#define ip_dst  I_dun.ip_d
#define ip_prev I_dun.ip_prv
};

/* ip options */
#define IP_OPT_COPY	0x80		/* option copy type flag */
#define IP_OPT_DEBUG	0x40		/* option debug class (2) */

#define IP_END_OPT	0		/* end of option list */
#define IP_NOP_OPT	1		/* nop option */
#define IP_SEC_OPT	2+IP_OPT_COPY	/* security option */
#define 	IP_SEC_OPTLEN	11		/* length */
#define		IP_SEC_OPTHDR	0x0b82		/* opt/len */
#define	IP_LRTE_OPT	3+IP_OPT_COPY	/* loose source and record route */
#define	IP_TIME_OPT	4+IP_OPT_DEBUG	/* timestamp */
#define IP_RRTE_OPT	7		/* record route */
#define IP_STRID_OPT	8+IP_OPT_COPY	/* stream ID */
#define		IP_STRID_OPTLEN	4		/* length */
#define		IP_STRID_OPTHDR	0x0488		/* opt/len */
#define IP_SRTE_OPT	9+IP_OPT_COPY	/* strict source and record route */

struct ipq {                    /* ip reass.q header */
	struct ip iqx;                  /* dummy ip element for top of list */
	struct ipq *iq_next;            /* -> next chain on q */
	struct ipq *iq_prev;            /* -> prev chain on q */
	struct ip iqh;                  /* fragment header */
};

#define IPVERSION 4             /* internet protocol version number */
#define IPMAX 576		/* maximum spec ip packet length */
#define IPMAXOPT 40		/* maximum length of internet options */	
#define MAXTTL 10              /* maximum time to live (seconds) */

/* Enqueue a fragment on the reassembly chain.  */
#define ip_enq(p, prev)							\
{	register struct ip *PREV = prev;				\
									\
	p->ip_prev = PREV;						\
	p->ip_next = PREV->ip_next;					\
	PREV->ip_next->ip_prev = p;					\
	PREV->ip_next = p;						\
}
 
/* Dequeue a fragment from reassembly chain.  */
#define ip_deq(p)							\
{									\
	p->ip_prev->ip_next = p->ip_next;				\
	p->ip_next->ip_prev = p->ip_prev;				\
}
