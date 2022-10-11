#ifdef BBNNET

struct ucb {                    /* user connection block */
	struct socket uc_host;          /* foreign host address */
	struct proc *uc_proc;           /* -> user proc */
	union {                         /* -> protocol control block */
		char *unull;                    /* general */
		struct tcb *utcb;               /* ->tcb (tcp) */
	} U_cp;
#define uc_tcb  U_cp.utcb
	struct if_local *uc_srcif;	/* -> source address */
	struct host *uc_route;		/* -> host entry for local net route */
	struct mbuf *uc_sbuf;           /* -> user send buffer */
	struct mbuf *uc_rbuf;           /* -> user receive buffer */
	unchar uc_lo;			/* lowest link no. in range (raw) */
	unchar uc_hi;			/* highest link no. in range (raw) */
	unchar uc_snd;			/* # send bufs allocated */
	unchar uc_rcv;			/* # receive bufs allocated */
	unchar uc_ssize;		/* # bufs on send buffer */
#define uc_timeo uc_ssize               /* user timeout parameter */
	unchar uc_rsize;		/* # bufs on receive buffer */
	unsigned short uc_state;	/* state of this connection */
	unsigned short uc_flags;	/* misc. flags (see below) */
};

#include "con.h"

#ifdef KERNEL
struct ucb *contab, *conNCON;           /* ->start, end of connection table */
int nnetcon;
#endif KERNEL

#endif BBNNET
