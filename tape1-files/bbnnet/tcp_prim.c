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
send_tcp(tp, fin, syn, eol, urg, urp, len, dat)        
register struct tcb *tp;
char fin, syn, eol, urg;
short urp;
int len;
struct mbuf *dat;
{
	register struct mbuf *m;
	register struct th *t;
	register struct ucb *up;
	register i;
	struct work w;

	if ((m = m_get(1)) == NULL)
		return(FALSE);
	up = tp->t_ucb;

	m->m_off = MSIZE - sizeof(struct th);
	m->m_len = sizeof(struct th);
	m->m_next = dat;

	/* adjust data length for syn and fin */

	if (syn)
		len--;
	if (fin)
		len--;

	t = (struct th *)((int)m + m->m_off);

	t->t_s = up->uc_srcif->if_addr;
	t->t_d = up->uc_host;

	t->t_src = short_to_net(tp->t_lport);
	t->t_dst = short_to_net(tp->t_fport);

	t->t_seq = long_to_net(tp->snd_nxt);
	t->t_ackno = long_to_net(tp->rcv_nxt);

	t->t_fin = fin;
	t->t_syn = syn & ~tp->snd_rst;
	t->t_rst = tp->snd_rst;
	t->t_eol = eol;
	t->t_ack = tp->syn_rcvd;   
	t->t_urg = urg;

	i = rcv_resource(tp) * MLEN;
	t->t_win = short_to_net(i);
	t->t_urp = short_to_net(urp);

	t->t_next = NULL;
	t->t_prev = NULL;
	t->t_x1 = 0;
	t->t_x2 = 0;
	t->t_x3 = 0;
	t->t_sum = 0;
	t->t_pr = TCPROTO;
	t->t_len = short_to_net(len + TCPSIZE);
	t->t_off = TCPSIZE/4;

#ifndef mbb
	t->t_sum = cksum(m, len + sizeof(struct th));
#else
	t->t_x0 = 0;
	t->t_x00 = 0;
	i = cksum(m, len + sizeof(struct th));
	t->t_sum = short_to_net(i);
#endif mbb

	i = ip_output(up, m, TCPROTO, len+TCPSIZE, tp->t_optlen, tp->t_opts);

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

	if ((p = tp->t_rcv_next) == (struct th *)tp || tp->rcv_nxt < p->t_seq)
		return(tp->rcv_nxt);

	while ((q = p->t_next) != (struct th *)tp &&    
		(t_end(p) + 1) == q->t_seq)
		p = q;

	return(t_end(p) + 1);
}

/*
 * Get number of rcv bufs available 
 */
rcv_resource(tp)                       
register struct tcb *tp;
{
	register struct ucb *up;
	register struct mbuf *m;
	register struct th *t;
	register i;

	up = tp->t_ucb;                 /* -> ucb */

	i = up->uc_rcv - up->uc_rsize;  /* total # rcv bufs allocated */
	if (i < 0)
		return(0);

	/* count bufs in tcp msg rcv queue */

	if (netcb.n_bufs < netcb.n_lowat)
		for (t = tp->t_rcv_next; i > 0 && t != (struct th *)tp; 
								t = t->t_next)
			for (m = dtom(t); i > 0 && m != NULL; m = m->m_next)
				i--;
	return(i);
}

/*
 * TCP/IP checksum routine: one's complement of the one's complement sum of
 * the sixteen bit words of header/data
 */
#ifndef mbb
#define lobyte(x) (*((char *)x) & 0xff)
#define hibyte(x) ((unsigned)((*((char *)x) & 0xff) << 8))
#else
#define lobyte(x) ((unsigned)((*((char *)x) & 0xff) << 8))
#define hibyte(x) (*((char *)x) & 0xff)
#endif

cksum(m, len)
register struct mbuf *m;
register len;
{
	register unsigned short *w;
	register unlong sum;
	register mlen;
	register unsigned short i;

	w = (unsigned short *)((int)m + m->m_off);
	mlen = m->m_len;
	sum = 0;

	for (; len > 0; len -= 2, mlen -= 2) {

try:            if (mlen > 1) {         /* can get a word */

			if (len > 1) {
#ifndef mbb
				sum += *(w++);
#else
				i = *(w++);
				sum += short_from_net(i);
#endif mbb

			} else            /* trailing odd byte */

				sum += lobyte(w);

		} else if (mlen > 0) {  /* last byte of mbuf */

			sum += lobyte(w);

			if (len > 1) {

        			/* get next good mbuf for hi byte */
        
        			while ((m = m->m_next) != NULL && 
					(mlen = m->m_len + 1) == 1);
        			if (m != NULL) {
        				w = (unsigned short *)((int)m + m->m_off);
        				sum += hibyte(w);
					w = (unsigned short *)((int)w + 1);
        			} else
        				len = 0;        /* force loop exit */
			}

		} else {                /* end of mbuf, get next and try again */

			while ((m = m->m_next) != NULL && (mlen = m->m_len) == 0);
			if (m != NULL) {
				w = (unsigned short *)((int)m + m->m_off);
				goto try;
			} else
				break;
		}
	}

	/* add in one's complement carry */

	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum + (sum >> 16));
	return(~sum & 0xffff);
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

	if (start >= end)    
		return(NULL);

	off = tp->snd_una;
	if (!tp->syn_acked)		/* skip over SYN */
		off++;
	m = tp->t_ucb->uc_sbuf;

	/* find mbuf to start copying */

	while (m != NULL && start >= (off + m->m_len)) {
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

		if (end > off && adj < m->m_len) {
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

	} while (end > off && (m = m->m_next) != NULL);

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
 * Enqueue segment on tcp sequencing queue
 */
tcp_enq(p, prev)
register struct th *p;
register struct th *prev;
{
	p->t_prev = prev;
	p->t_next = prev->t_next;
	prev->t_next->t_prev = p;
	prev->t_next = p;
}
 
/*
 *  Dequeue segment from tcp sequencing queue
 */
tcp_deq(p)
register struct th *p;
{
	p->t_prev->t_next = p->t_next;
	p->t_next->t_prev = p->t_prev;
}

/*
 * Cancel a timer
 */
t_cancel(tp, timer)                     
register struct tcb *tp;
register timer;
{
	register struct work *w;
	register char *t;

	/* reset timer value in tcb */

	t = (char *)((char *)&tp->t_init + timer - 1);
	*t = 0;

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

	/* search through tcb and update active timers */

	for (tp = netcb.n_tcb_head; tp != NULL; tp = tp->t_tcb_next) {

		if (tp->t_init != 0)
			if (--tp->t_init == 0)
				w_alloc(ISTIMER, TINIT, tp, 0);

		if (tp->t_rexmt != 0)
			if (--tp->t_rexmt == 0)
				w_alloc(ISTIMER, TREXMT, tp, 0);

		if (tp->t_rexmttl != 0)
			if (--tp->t_rexmttl == 0)
				w_alloc(ISTIMER, TREXMTTL, tp, 0);

		if (tp->t_persist != 0)
			if (--tp->t_persist == 0)
				w_alloc(ISTIMER, TPERSIST, tp, 0);

		if (tp->t_finack != 0)
			if (--tp->t_finack == 0)
				w_alloc(ISTIMER, TFINACK, tp, 0);

		tp->t_xmt++;

	}

	netcb.n_iss += ISSINCR;         /* increment iss */

	timeout(tcp_timeo, 0, HZ);      /* reschedule every second */
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
tcp_opt(t, tp, hlen)        
register struct th *t;
register struct tcb *tp;
int hlen;
{
	register char *p;
	register i;

	p = (char *)((int)t + TCPSIZE);		/* -> at options */
	
	if ((i = hlen - TCPSIZE) > 0) {			/* any options */

		while (i > 0)

  			switch (*p++) {
			case 0:                 
			case 1:                 
				i--;
				break;

			case 2:			/* max segment size */
				if (t->t_syn && !tp->syn_rcvd)
					tp->t_maxseg = MIN(tp->t_maxseg,
						        *(char *)((int)p+1));
			default:
				i -= *p;
				p += *p;
			}
                p += i;
	}
}
