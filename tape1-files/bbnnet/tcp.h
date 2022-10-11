struct th {                     /* tcp header (fits over ip header) */
	struct th *t_next;              /* -> next tcp on rcv chain */
	struct th *t_prev;              /* -> prev tcp on rcv chain */
#ifdef mbb
	short t_x0, t_x00;		/* (for alignment) */
#endif mbb
	unchar t_x1;			/* (unused) */
	unchar t_pr;			/* protocol */
	unsigned short t_len;           /* seg length */
	struct socket t_s;              /* source internet address */
	struct socket t_d;              /* destination internet address */
	unsigned short t_src;           /* source port */
	unsigned short t_dst;           /* destination port */
	sequence t_seq;                 /* sequence number */
	sequence t_ackno;               /* acknowledgement number */
#define t_end(x) (x->t_seq + x->t_len - 1)
#ifndef mbb
	unchar
		t_x2:4,                 /* (unused) */
		t_off:4;                /* data offset */
	unchar
		t_fin:1,                /* fin flag */
		t_syn:1,                /* syn flag */
		t_rst:1,                /* reset flag */
		t_eol:1,                /* eol flag */
		t_ack:1,                /* ack flag */
		t_urg:1,                /* urgent flag */
		t_x3:2;                 /* (unused) */
#else
	unchar
		t_x21:2,		/* (unused) */
		t_off:4,		/* data offset */
		t_x2:4;			/* (unused) */
	unchar
		t_x3:4,			/* (unused) */
		t_urg:1,		/* urgent flag */
		t_ack:1,		/* ack flag */
		t_eol:1,		/* eol flag */
		t_rst:1,		/* reset flag */
		t_syn:1,		/* syn flag */
		t_fin:1;		/* fin flag */
#endif mbb
	unsigned short t_win;           /* window */
	unsigned short t_sum;           /* checksum */
	unsigned short t_urp;           /* urgent pointer */
};

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
	sequence snd_wnd;               /* send window max */
	sequence t_rexmt_val;           /* val saved in rexmt timer */
	sequence t_rtl_val;             /* val saved in rexmt too long timer */
	sequence t_xmt_val;             /* seq # sent when xmt timer started */

	/* various flags and state variables */

	unsigned short
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
		usr_abort:1;            /* user has closed and does not expect
					   to receive any more data */

	short t_maxseg;			/* max seg size peer can handle */
	unsigned short t_lport;         /* local port */
	unsigned short t_fport;         /* foreign port */
	unchar t_state;			/* state of this connection */
	unchar t_xmtime;		/* current rexmt time */
	unchar t_optlen;		/* length of ip option string */

	/* timers */

	unchar t_init;			/* initialization too long */
	unchar t_rexmt;			/* retransmission */
	unchar t_rexmttl;		/* retransmit too long */
	unchar t_persist;		/* retransmit persistance */
	unchar t_finack;		/* fin acknowledged */
	unchar t_xmt;			/* round trip transmission time */
};          
	    
/* size of TCP leader (bytes) */
#define TCPSIZE (sizeof(struct th)-sizeof(struct ip))                      

#define ISSINCR 128                     /* increment for iss each second */
#define TCPROTO 6                       /* TCP-4 protocol number */
#define TCPMAXSND 950			/* initial maximum segment size */
#define T_2ML   10                      /* 2*maximum packet lifetime */
#define T_PERS  5                       /* persist time */
#define T_INIT  30                      /* init too long timeout */
#define T_REXMT 1                       /* base for retransmission time */
#define T_REXMTTL 30                    /* retransmit too long timeout */
#define T_REMAX 30                      /* maximum retransmission time */
#define ACTIVE  1                       /* active open */
#define PASSIVE 0                       /* passive open */

sequence firstempty();
