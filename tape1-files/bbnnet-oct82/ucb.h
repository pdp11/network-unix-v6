#ifdef BBNNET

struct ucb {                    /* user connection block */
	struct ucb *uc_next;		/* ->next ucb */
	struct ucb *uc_prev;		/* ->prev ucb */
	struct socket uc_host;          /* foreign host address */
	struct socket uc_local;		/* local host address */
	struct proc *uc_proc;           /* -> user proc */
	union {                         /* -> protocol control block */
		char *unull;                    /* general */
		struct tcb *utcb;               /* ->tcb (tcp) */
		struct proto *uproto;		/* ->proto blcok (raw) */
		struct {
			u_short u_lport;	/* udp local port */
			u_short u_fport;	/* udp foreign port */
		} U_udp;
	} U_cp;
#define uc_tcb  U_cp.utcb
#define uc_proto U_cp.uproto
#define uc_udp U_cp.U_udp
	struct ifcb *uc_srcif;		/* -> source ifcb */
	struct host *uc_route;		/* -> host entry for local net route */
	struct mbuf *uc_sbuf;           /* -> user send buffer */
	struct mbuf *uc_rbuf;           /* -> user receive buffer */
	u_char uc_snd;			/* # send bufs allocated */
	u_char uc_rcv;			/* # receive bufs allocated */
	u_char uc_ssize;		/* # bufs on send buffer */
#define uc_timeo uc_ssize               /* user timeout parameter */
	u_char uc_rsize;		/* # bufs on receive buffer */
	u_short uc_xstat;		/* network status word */	
	u_short uc_state;		/* state of this connection */
	u_short uc_flags;		/* misc. flags (see con.h) */
};

#include "con.h"

#endif BBNNET
