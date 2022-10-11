struct th {                     /* tcp header (fits over ip header) */
	struct th *t_next;              /* -> next tcp on rcv chain */
	struct th *t_prev;              /* -> prev tcp on rcv chain */
#ifdef mbb
	short t_x0, t_x00;		/* (for alignment) */
#endif mbb
	u_char t_x1;			/* (unused) */
	u_char t_pr;			/* protocol */
	u_short t_len;			/* seg length */
	struct socket t_s;              /* source internet address */
	struct socket t_d;              /* destination internet address */
	u_short t_src;			/* source port */
	u_short t_dst;			/* destination port */
	sequence t_seq;                 /* sequence number */
	sequence t_ackno;               /* acknowledgement number */
#define t_end(x) (x->t_seq + x->t_len - 1)
#ifndef mbb
	u_char
		t_x2:4,                 /* (unused) */
		t_off:4;                /* data offset */
#else
	u_char
		t_x21:2,		/* (unused) */
		t_off:4,		/* data offset */
		t_x2:4;			/* (unused) */
#endif mbb
	u_char 	t_flags;		
#define T_FIN	0x01			/* fin flag */
#define T_SYN	0x02			/* syn flag */
#define T_RST	0x04			/* reset flag */
#define T_PUSH	0x08			/* push flag */
#define T_ACK	0x10			/* ack flag */
#define T_URG	0x20			/* urgent flag */
	u_short t_win;			/* window */
	u_short t_sum;			/* checksum */
	u_short t_urp;			/* urgent pointer */
};

#define TCP_END_OPT	0		/* end of option list */
#define TCP_NOP_OPT	1		/* nop option */
#define TCP_MAXSEG_OPT	2		/* maximum segment size option */
#define		TCP_MAXSEG_OPTLEN 4		/* max seg option length */
#define		TCP_MAXSEG_OPTHDR 0x0402		/* max seg opt/len */

struct tcb {                    /* tcp control block */

	/* various pointers */

	struct th *t_rcv_next;          /* -> first el on rcv queue */
	struct th *t_rcv_prev;          /* -> last el on rcv queue */
	struct tcb *t_tcb_next;         /* -> next tcb */
	struct tcb *t_tcb_prev;         /* -> prev tcb */
	struct ucb *t_ucb;              /* -> ucb */
	struct mbuf *t_rcv_unack;       /* -> unacked message queue */
	char *t_opts;			/* -> ip option string */

	/* sequence number variables */

	sequence iss;                   /* initial send seq # */
	sequence irs;                   /* initial recv seq # */
	sequence rcv_urp;               /* rcv urgent pointer */
	sequence rcv_nxt;               /* next seq # to rcv */
	sequence rcv_end;               /* rcv eol pointer */
	sequence seq_fin;               /* seq # of FIN sent */
	sequence snd_end;               /* send eol pointer */
	sequence snd_urp;               /* snd urgent pointer */
	sequence snd_lst;               /* seq # of last datum to send */
	sequence snd_nxt;               /* seq # of next datum to send */
	sequence snd_una;               /* seq # of first unacked datum */
	sequence snd_wl;                /* seq # of last sent window */
	sequence snd_hi;                /* highest seq # sent */
	sequence t_rexmt_val;           /* val saved in rexmt timer */
	sequence t_rtl_val;             /* val saved in rexmt too long timer */
	sequence t_xmt_val;             /* seq # sent when xmt timer started */

	/* various flags and state variables */

#ifndef mbb
	u_long
#else 
	u_short
#endif
		ack_due:1,              /* must we send ACK */
		cancelled:1,            /* retransmit timer cancelled */
		dropped_txt:1,          /* dropped incoming data */
		fin_rcvd:1,             /* FIN received */
		force_one:1,            /* force sending of one byte */
		new_window:1,           /* received new window size */
		rexmt:1,                /* this msg is a retransmission */
		snd_fin:1,              /* FIN should be sent */
		snd_rst:1,              /* RST should be sent */
		snd_urg:1,              /* urgent data to send */
		syn_acked:1,            /* SYN has been ACKed */
		syn_rcvd:1,             /* SYN has been received */
		usr_closed:1,           /* user has closed connection */
		waited_2_ml:1,          /* wait time for FIN ACK is up */
		net_keep:1,             /* don't free this net input */
		usr_abort:1,            /* user has closed and does not expect
					   to receive any more data */
		sent_zero:1;		/* sent zero window */

	short snd_wnd;			/* advertised window */
	short t_maxseg;			/* max seg size peer can handle */
	u_short t_lport;		/* local port */
	u_short t_fport;		/* foreign port */
	u_char t_state;			/* state of this connection */
	u_char t_xmtime;		/* current rexmt time */
	u_char t_optlen;		/* length of ip option string */
	u_char t_timers[6];		/* timers */ 
	u_char t_rxtct;			/* # of retransmissions */
};          
	    
/* size of TCP leader (bytes) */
#define TCPSIZE (sizeof(struct th)-sizeof(struct ip))                      
/* max size of TCP/IP leader */
#define TCPIPMAX (sizeof(struct th) + IPMAXOPT)
/* initial maximum segment size */
#define TCPMAXSND (IPMAX - TCPIPMAX)

#define ISSINCR 128                     /* increment for iss each second */
#define TCPROTO 6                       /* TCP-4 protocol number */
#define T_2ML   20                      /* 2*maximum packet lifetime */
#define T_PERS  5                       /* persist time */
#define T_INIT  30                      /* init too long timeout */
#define T_REXMT 2                       /* base for retransmission time */
#define T_REXMTTL 30                    /* retransmit too long timeout */
#define T_REMAX 30                      /* maximum retransmission time */
#define TCP_ACTIVE  1			/* active open */
#define TCP_PASSIVE 0			/* passive open */
#define TCP_CTL	1			/* send/receive control call */
#define TCP_DATA 0			/* send/receive data call */

/* macros for sequence number comparison (32 bit modular arithmatic) */
#ifndef mbb
#define SEQ_LT(a,b)	((long)((a)-(b)) < 0)
#define SEQ_LEQ(a,b)	((long)((a)-(b)) <= 0)
#define SEQ_GT(a,b)	((long)((a)-(b)) > 0)
#define SEQ_GEQ(a,b)	((long)((a)-(b)) >= 0)
#define SEQ_EQ(a,b)	((a) == (b))
#define SEQ_NEQ(a,b)	((a) != (b))
#else
#define NBIT    8
#define BMASK   037777777777

#define SEQ_LT(a,b)	((((a)-(b)) << NBIT) < 0)
#define SEQ_LEQ(a,b)	((((a)-(b)) << NBIT) <= 0)
#define SEQ_GT(a,b)	((((a)-(b)) << NBIT) > 0)
#define SEQ_GEQ(a,b)	((((a)-(b)) << NBIT) >= 0)
#define SEQ_EQ(a,b)	((((a)-(b)) & BMASK) == 0)
#define SEQ_NEQ(a,b)	((((a)-(b)) & BMASK) != 0)
#endif
#define SEQ_MIN(a,b)	(SEQ_LT((a),(b)) ? (a) : (b))
#define SEQ_MAX(a,b)	(SEQ_GT((a),(b)) ? (a) : (b))

/* tcp machine predicates */

/* acceptable ACK */
#define ack_ok(x, y) (SEQ_LT(x->iss, y->t_ackno) && \
			SEQ_LEQ(y->t_ackno, x->snd_hi))
 
/* ACK of local FIN */
#define ack_fin(x, y) (SEQ_GT(x->seq_fin, x->iss) && \
			SEQ_GT(y->t_ackno,x->seq_fin))

/* receive buffer empty */
#define rcv_empty(x) (x->usr_abort || \
		(x->t_ucb->uc_rbuf == NULL && x->t_rcv_next == x->t_rcv_prev))

/* Enqueue/dequeue segment on tcp sequencing queue */
#ifdef vax
#define tcp_enq(x, y) insque(x, y)
#define tcp_deq(x) remque(x)
#else

#define tcp_enq(p, prev) 						\
{	register struct th *PREV = prev;				\
									\
	p->t_prev = PREV;						\
	p->t_next = PREV->t_next;					\
	PREV->t_next->t_prev = p;					\
	PREV->t_next = p;						\
}
 
#define tcp_deq(p)							\
{									\
	p->t_prev->t_next = p->t_next;					\
	p->t_next->t_prev = p->t_prev;					\
}
#endif

sequence firstempty();
