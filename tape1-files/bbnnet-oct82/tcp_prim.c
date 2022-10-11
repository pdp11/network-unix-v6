#include "../h/param.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/imp.h"
#include "../bbnnet/ucb.h"
#include "../bbnnet/fsm.h"

/*
 * TCP finite state machine primitives
 *
 * These routines are called from the procedures in tcp_procs.c to do low
 * level protocol functions.
 */

/*
 * Send a tcp segment
 */
send_pkt(tp, flags, len, dat)        
register struct tcb *tp;
register int flags;
int len;
struct mbuf *dat;
{
	register struct mbuf *m;
	register struct th *t;
	register struct ucb *up;
	register i;
	short *p;
	struct work w;

	if ((m = m_get(1)) == NULL)
		return(FALSE);
	up = tp->t_ucb;
	/*
	 * Build tcp leader at bottom of new buffer to leave room for lower
	 * level leaders.  Leave an extra four bytes for TCP max segment size
	 * option, which is sent in SYN packets.
	 */
	m->m_off = MSIZE - sizeof(struct th) - 4;
	m->m_len = sizeof(struct th);
	m->m_next = dat;
	t = mtod(m, struct th *);
	/*
	 * Adjust data length for SYN and FIN.  Also, insert max seg size
	 * option for SYN.
	 */
	if (flags&T_FIN)
		len--;
	if (flags&T_SYN)
#ifdef NOTCPOPTS
		len--;
#else
	{
		m->m_len += TCP_MAXSEG_OPTLEN;
		len += TCP_MAXSEG_OPTLEN - 1;	/* SYN occupies seq space */	
		t->t_off = (TCPSIZE + TCP_MAXSEG_OPTLEN) >> 2;
		p = (short *)((int)t + sizeof(struct th));
		*p++ = TCP_MAXSEG_OPTHDR;
		*p = short_to_net(up->uc_srcif->if_mtu - TCPIPMAX);
	} else 
#endif
		t->t_off = TCPSIZE >> 2;
	t->t_len = short_to_net(len + TCPSIZE);
	/*
	 * Set up Internet addresses in IP part of leader, and fill in
	 * the TCP part.
	 */
	t->t_s = up->uc_local;
	t->t_d = up->uc_host;
	t->t_src = short_to_net(tp->t_lport);
	t->t_dst = short_to_net(tp->t_fport);
	t->t_seq = long_to_net(tp->snd_nxt);
	t->t_ackno = long_to_net(tp->rcv_nxt);

	if (tp->snd_rst) {
		flags |= T_RST;
		flags &= ~T_SYN;
	}
	if (tp->snd_urg)
		flags |= T_URG;
	if (tp->syn_rcvd)
		flags |= T_ACK;
	t->t_flags = flags;
	/*
	 * If we sent a zero window, we should try to send a non-zero ACK ASAP.
	 */
	if ((i = rcv_resource(tp)) == 0)
		tp->sent_zero = TRUE;
	else
		tp->sent_zero = FALSE;

	t->t_win = short_to_net(i);
	t->t_urp = short_to_net(tp->snd_urp-tp->snd_nxt);
	/*
	 * Get rest of IP part of leader ready for checksum and IP processing.
	 */
	t->t_next = NULL;
	t->t_prev = NULL;
	t->t_x1 = 0;
	t->t_x2 = 0;
	t->t_sum = 0;
	t->t_pr = TCPROTO;

#ifndef mbb
	t->t_sum = cksum(m, len + sizeof(struct th));
#else
	t->t_x0 = 0;
	t->t_x00 = 0;
	i = cksum(m, len + sizeof(struct th));
	t->t_sum = short_to_net(i);
#endif mbb

	i = ip_send(up, m, TCPROTO, len+TCPSIZE, tp->t_optlen, tp->t_opts, 
		    FALSE);

	if (up->uc_flags & UDEBUG) {
		w.w_dat = (char *)t;
		w.w_stype = i;
		tcp_debug(tp, &w, INRECV, -1);
	}
	return(i);
}

/*
 * Find the first empty spot in rcv buffer 
 */
sequence firstempty(tp)                  
register struct tcb *tp;
{
	register struct th *p, *q;

	if ((p = tp->t_rcv_next) == (struct th *)tp || 
	    SEQ_LT(tp->rcv_nxt, p->t_seq))
		return(tp->rcv_nxt);

	while ((q = p->t_next) != (struct th *)tp &&    
	       SEQ_EQ(t_end(p)+1, q->t_seq))
		p = q;

	return(t_end(p) + 1);
}

/*
 * Get number of rcv bufs available 
 */
rcv_resource(tp)                       
register struct tcb *tp;
{
	register struct ucb *up = tp->t_ucb; 
	register struct th *t;
	register i;

	/* first count bufs in user receive queue */

	i = (up->uc_rcv - up->uc_rsize) * MLEN;  

	/* now reduce by segments in sequencing queue */

	for (t = tp->t_rcv_next; i > 0 && t != (struct th *)tp; t = t->t_next)
		i -= t->t_len;

	return(i < 0 ? 0 : i);
}

/*
 * Copy mbuf chain from snd buffer for sends
 */
struct mbuf *snd_copy(tp, start, end)
struct tcb *tp;
sequence start, end;
{
	register struct mbuf *m, *n;
	register sequence off;
	register adj, len;
	struct mbuf *top;

	/* make sure we have something to copy */

	if (SEQ_GEQ(start, end))    
		return(NULL);

	off = tp->snd_una;
	if (!tp->syn_acked)		/* skip over SYN */
		off++;
	m = tp->t_ucb->uc_sbuf;

	/* find mbuf to start copying */

	while (m != NULL && SEQ_GEQ(start, off+m->m_len)) {
		off += m->m_len;
		m = m->m_next;
	}

	/* get buffer to copy into */

	if (m == NULL || (n = top = m_get(1)) == NULL)
		return(NULL);

	adj = start - off;
	off = start;
	goto partial;		/* start with partial copy */

	do {		/* main copy loop */

		/* amount we can copy into mbuf */

		if ((adj = MLEN - n->m_len) >= m->m_len)
			len = m->m_len;		/* can copy all */
		else
			len = adj;		/* partial copy */

		/* copy as much as possible into mbuf */

		bcopy((caddr_t)((int)m + m->m_off), 
			(caddr_t)((int)n + MHEAD + n->m_len), len);
		n->m_len += len;
		off += len;

		/* must get another mbuf to copy into */

		if (SEQ_GT(end, off) && adj < m->m_len) {
			if ((n = n->m_next = m_get(1)) == NULL) {
				m_freem(top);
				return(NULL);
			}

			/* partial copy into new mbuf */

partial:		n->m_off = MHEAD;
			n->m_next = NULL;
			n->m_len = m->m_len - adj;
			bcopy((caddr_t)((int)m + m->m_off + adj),
				(caddr_t)((int)n + MHEAD), n->m_len);
			off += n->m_len;
		}

	} while (SEQ_GT(end, off) && (m = m->m_next) != NULL);

	/* make sure there was enough in buffer to copy */

	if (m == NULL) {
		printf("snd_copy: bad copy\n");
		m_freem(top);
		return(FALSE);
	}

	/* adjust length of last mbuf copied */

	n->m_len -= (off - end);
	return(top);
}


/*
 * Cancel a timer
 */
t_cancel(tp, timer)                     
register struct tcb *tp;
register timer;
{
	register struct work *w;

	/* reset timer value in tcb */

	tp->t_timers[timer] = 0;

	/* remove any timer work entries already enqueued */

	for (w = netcb.n_work; w != NULL; w = w->w_next) {
		if (w->w_tcb == tp && w->w_type == ISTIMER && 
				w->w_stype == timer)
			w->w_type = INOP;
		
		/* THIS SHOULD GO AWAY */

		if (w == w->w_next) 
			w->w_next = NULL;
	}
}

/*
 * TCP timer update routine 
 */
tcp_timeo()                     
{
	register struct tcb *tp;
	register i;

	/* search through tcb and update active timers */

	for (tp = netcb.n_tcb_head; tp != NULL; tp = tp->t_tcb_next) {
		for (i = TINIT; i <= TFINACK; i++)
			if (tp->t_timers[i] != 0 && --tp->t_timers[i] == 0)
				w_alloc(ISTIMER, i, tp, 0);
		tp->t_timers[TXMT]++;
	}
	netcb.n_iss += ISSINCR;         /* increment iss */
}

/*
 * Tell user proc about change of tcp state 
 */
to_user(up, state)              
register struct ucb *up;
register short state;
{

	/* set user state flag and awaken user proc if asleep */

	up->uc_state |= state;
	wakeup(up);

	/* send urgent signal to user process */

  	if (state == UURGENT)  
		psignal(up->uc_proc, SIGURG);
}

/*
 * Do TCP option processing
 */
tcp_opt(tp, t, hlen)        
register struct tcb *tp;
register struct th *t;
int hlen;
{
	register char *p;
	register i, len;

	p = (char *)((int)t + sizeof(struct th));	/* -> at options */
	
	if ((i = hlen - TCPSIZE) > 0) {			/* any options */

		while (i > 0)

  			switch (*p++) {
			case TCP_END_OPT:                 
				return;
			case TCP_NOP_OPT:                 
				i--;
				break;

			case TCP_MAXSEG_OPT:		/* max segment size */
				if (t->t_flags&T_SYN && !tp->syn_rcvd) {
					len = short_from_net(
					          *(short *)((int)p + 1));
					tp->t_maxseg = 
					       MIN(tp->t_ucb->uc_srcif->if_mtu -
						   TCPIPMAX, len);
				}
			
			default:
				i -= *p;
				p += *p - 1;
			}
	}
}
