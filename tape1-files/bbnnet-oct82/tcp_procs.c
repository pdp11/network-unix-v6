#include "../h/param.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ucb.h"
#include "../bbnnet/fsm.h"

/*
 * TCP finite state machine procedures.
 *
 * Called from finite state machine action routines, these do most of the work
 * of the protocol.  They in turn call primitive routines (in tcp_prim) to
 * perform lower level functions.
 */

/*
 * Set up a TCB for a connection
 */
t_open(tp, mode)                
register struct tcb *tp;
int mode;
{
	register struct ucb *up = tp->t_ucb;

	/* enqueue the tcb */

	if (netcb.n_tcb_head == NULL) {
		netcb.n_tcb_head = tp;
		netcb.n_tcb_tail = tp;
	} else {
		tp->t_tcb_next = netcb.n_tcb_head;
		netcb.n_tcb_head->t_tcb_prev = tp;
		netcb.n_tcb_head = tp;
	}

	/* initialize non-zero tcb fields */

	tp->t_rcv_next = (struct th *)tp;
	tp->t_rcv_prev = (struct th *)tp;
	tp->t_xmtime = T_REXMT;
#ifndef NOTCPOPTS
	tp->t_maxseg = MIN(up->uc_srcif->if_mtu - TCPIPMAX, TCPMAXSND);
#else
	tp->t_maxseg =  up->uc_srcif->if_mtu - TCPIPMAX;
#endif
	tp->snd_end = tp->seq_fin = tp->snd_nxt = tp->snd_hi = 
				tp->snd_una = tp->iss = netcb.n_iss;
	netcb.n_iss += (ISSINCR >> 1) + 1;

	/* set timeout for open */

	tp->t_timers[TINIT] = (up->uc_timeo != 0 ? up->uc_timeo : 
					(mode == TCP_ACTIVE ? T_INIT : 0));
	up->uc_timeo = 0;       /* overlays uc_ssize */
}

/*
 * Delete TCB and free all resources used by the connection.  Called after
 * the close protocol is complete.
 */
t_close(tp, state)
register struct tcb *tp;
short state;
{
	register struct ucb *up;
	register struct th *t;
	register struct mbuf *m;
	register struct work *w;
	int i;

	up = tp->t_ucb;

	/* cancel all timers */

	for (i = TINIT; i <= TFINACK; i++)
		tp->t_timers[i] = 0;

	/* remove all work entries for tcb */

	for (w = netcb.n_work; w != NULL; w = w->w_next)
		if (w->w_tcb == tp)
			w->w_type = INOP;

	/* delete tcb */

	if (tp->t_tcb_prev == NULL)
		netcb.n_tcb_head = tp->t_tcb_next;
	else
		tp->t_tcb_prev->t_tcb_next = tp->t_tcb_next;
	if (tp->t_tcb_next == NULL)
		netcb.n_tcb_tail = tp->t_tcb_prev;
	else
		tp->t_tcb_next->t_tcb_prev = tp->t_tcb_prev;

	/* free all data on receive and send buffers */

	for (t = tp->t_rcv_next; t != (struct th *)tp; t = t->t_next)
		m_freem(dtom(t));

	if (up->uc_rbuf != NULL) {
		m_freem(up->uc_rbuf);
		up->uc_rbuf = NULL;
	}

	if (up->uc_sbuf != NULL) {
		m_freem(up->uc_sbuf);
		up->uc_sbuf = NULL;
	}

	for (m = tp->t_rcv_unack; m != NULL; m = m->m_act) {
		m_freem(m);
		tp->t_rcv_unack = NULL;
	}

	/* free tcb buffer */

	m_free(dtom(tp));
	up->uc_tcb = NULL;

	/* lower buffer allocation and decrement host entry */

	netcb.n_lowat -= up->uc_snd + up->uc_rcv + 2;
	netcb.n_hiwat = (netcb.n_lowat * 3) >> 1;
	if (up->uc_route != NULL) {
		h_free(up->uc_route);
		up->uc_route = NULL;
	}

	/* if user has initiated close (via close call), delete ucb
	   entry, otherwise just wakeup so user can issue close call */

	if (tp->usr_abort) {
		if (up->uc_flags & UCWAIT)
			wakeup(tp);
        	up->uc_proc = NULL;
		if (up->uc_next != NULL)
			up->uc_next->uc_prev = up->uc_prev;
		if (up->uc_prev != NULL)
			up->uc_prev->uc_next = up->uc_next;
		else
			netcb.n_ucb_hd = up->uc_next;
		m_free(dtom(up));
	} else
        	to_user(up, state);

}

/*
 * Send a TCP segment.  Send data from left window edge of send buffer up to
 * window size or end (whichever is less).  Set retransmission timers.
 */
send_tcp(tp, ctl)                        
register struct tcb *tp;
int ctl;
{
	register struct ucb *up;
	register sequence last, wind;
	struct mbuf *m;
	int forced, sent;
	int snd_flags;
	struct mbuf *snd_copy();
	int len;

	up = tp->t_ucb;
	snd_flags = 0;
	tp->snd_lst = tp->snd_nxt;
	forced = FALSE;
	m = NULL;
	/* 
	 * Send SYN if this is first data (ISS)
	 */
	if (SEQ_EQ(tp->snd_nxt, tp->iss)) {	
		snd_flags |= T_SYN;	
		tp->snd_lst++;
	} 
	/*
	 * Get seq # of last datum in send buffer 
	 */
	last = tp->snd_una;
	if (!tp->syn_acked)
		last++;				/* don't forget SYN */
	for (m = up->uc_sbuf; m != NULL; m = m->m_next) 
		last += m->m_len;
	/*
	 * If no data to send in buffer, just do FIN check, otherwise see
	 * how much we should send in segment.
	 */
	if (SEQ_GEQ(tp->snd_nxt, last)) {
		/*
		 * should send FIN?  don't unless haven't already sent one 
		 */
		if (tp->snd_fin && 
		    (SEQ_EQ(tp->seq_fin, tp->iss) || 
		     SEQ_LEQ(tp->snd_nxt, tp->seq_fin))) {
			snd_flags |= T_FIN;
			tp->seq_fin = tp->snd_lst++;
		}
	} else if (tp->syn_acked) { 
		/*
		 * Only send a segment if there is something in the buffer,
		 * and a window has been received.
		 */
		wind = tp->snd_una + tp->snd_wnd;

		/* Use window to limit send */

		tp->snd_lst = SEQ_MIN(last, wind);

		/* Make sure we don't do IP fragmentation or send
		   more than peer can handle */

		if ((len = tp->snd_lst - tp->snd_nxt) > tp->t_maxseg) 
			tp->snd_lst -= len - tp->t_maxseg;

		/* see if PUSH should be sent */

		if (SEQ_GT(tp->snd_end, tp->iss) && 
		    SEQ_LEQ(tp->snd_end, tp->snd_lst)) 
			snd_flags |= T_PUSH;

		/* Avoid silly window syndrome: make sure usable window
		   is at least 25% of offered window.  Be sure the user
		   is not blocked for send resources. 
		
		else if (!tp->rexmt && !tp->ack_due && !tp->snd_fin &&
			 up->uc_snd != up->uc_ssize && tp->snd_wnd > 0 && 
			 100*((tp->snd_lst-tp->snd_una)/tp->snd_wnd) < 25)
			tp->snd_lst = tp->snd_nxt; 

		/* Set persist timer, if we exceeded the window */

		if (SEQ_GEQ(tp->snd_lst, wind))                       
			tp->t_timers[TPERSIST] = T_PERS;

		/* Check if window is closed and must force a byte out 
		   (persist timer went off) */

		if (tp->force_one && SEQ_EQ(tp->snd_lst, wind)) {
			tp->snd_lst = tp->snd_nxt + 1;
			forced = TRUE;
		}

		/* copy data to send from send buffer */

		m = snd_copy(tp, SEQ_MAX(tp->iss+1,tp->snd_nxt), tp->snd_lst);

		/* must send FIN and no more data left to send after this */

		if (tp->snd_fin && !forced && SEQ_EQ(tp->snd_lst, last) &&
		    (SEQ_EQ(tp->seq_fin, tp->iss) || 
		     SEQ_LEQ(tp->snd_nxt, tp->seq_fin))) {
			snd_flags |= T_FIN;
			tp->seq_fin = tp->snd_lst++;
		}
	}
	/*
	 * If there is something to send, do it and update timers for rexmt.
	 */
	if (SEQ_LT(tp->snd_nxt, tp->snd_lst)) {        

		sent = send_pkt(tp, snd_flags, 
				    (int)(tp->snd_lst-tp->snd_nxt), m);  

		/* set timers for retransmission if necessary */

		if (!forced) {
			tp->t_timers[TREXMT] = tp->t_xmtime;
			tp->t_rexmt_val = tp->snd_lst;
			if (!tp->rexmt) {
				tp->t_timers[TREXMTTL] = T_REXMTTL;
				tp->t_rtl_val = tp->snd_lst;
			}
		}

		/* update seq for next send if this one got out */

		if (sent)
			tp->snd_nxt = tp->snd_lst;                

		/* if last timed message has been acked, start timing
		   this one */

		if (tp->syn_acked && SEQ_GT(tp->snd_una, tp->t_xmt_val)) {
			tp->t_timers[TXMT] = 0;
			tp->t_xmt_val = tp->snd_lst;
		}

		tp->ack_due = FALSE;
		tp->snd_hi = SEQ_MAX(tp->snd_nxt, tp->snd_hi);
		tp->rexmt = FALSE;
		tp->force_one = FALSE;
		if (sent)
			return(TRUE);
	}
	/*
	 * If ctl, make sure to send something so ACK gets through 
	 */
	if (ctl == TCP_CTL) {
		send_pkt(tp, 0, 0, NULL);
		tp->ack_due = FALSE;
	}
	return(FALSE);
}

/*
 * Process incoming segments 
 */
rcv_tcp(tp, n, ctl)                 
register struct tcb *tp;
register struct th *n;
int ctl;
{
	register struct mbuf *m;
	register sequence last;
	int sent;

	tp->dropped_txt = FALSE;
	tp->ack_due = FALSE;
	tp->new_window = FALSE;
	/*
	 * Process SYN
	 */
	if (!tp->syn_rcvd && n->t_flags&T_SYN) {	
		tp->irs = n->t_seq;
		tp->rcv_nxt = n->t_seq + 1;
		tp->snd_wl = tp->rcv_urp = tp->irs;
		tp->syn_rcvd = TRUE;
		tp->ack_due = TRUE;
	}
	/*
	 * Process ACK if data not already acked previously. (Take ACKed data
	 * off send queue, and reset rexmt timers).
	 */
	if (n->t_flags&T_ACK && tp->syn_rcvd && SEQ_GT(n->t_ackno, tp->snd_una))
        	rcv_ack(tp, n);
	/*
	 * Check for new window.
	 */
	if (tp->syn_rcvd && SEQ_GEQ(n->t_seq, tp->snd_wl)) {	
		tp->snd_wl = n->t_seq;
		tp->snd_wnd = n->t_win;
		tp->new_window = TRUE;
		t_cancel(tp, TPERSIST);         /* cancel persist timer */  
	}
	/*
	 * For data packets only (vs. ctl), process data and URG.
	 */
	if (ctl == TCP_DATA) {				
		if (n->t_len != 0)
			rcv_text(tp, n);	/* accept and sequence data */
		/*
		 * Process URG if new urgent pointer received.
		 */
		if (n->t_flags&T_URG) {			
			last = n->t_urp + n->t_seq;
			if (SEQ_LT(tp->rcv_nxt, last)) {

				/* Tell user about start of urgent data only */

				if (SEQ_LEQ(tp->rcv_urp, tp->rcv_nxt))
					to_user(tp->t_ucb, UURGENT);
				tp->rcv_urp = last;
			}
		}
		/*
		 * Process PUSH, mark end of data chain.
		 */
		if (n->t_flags&T_PUSH && !tp->dropped_txt &&
		    tp->t_rcv_prev != (struct th *)tp) {	

			/* Find last mbuf on received data chain and mark */

			m = dtom(tp->t_rcv_prev);
			if (m != NULL) {
				while (m->m_next != NULL)
					m = m->m_next;
				m->m_act = (struct mbuf *)(m->m_off+m->m_len-1);
			}
		}
	}
	/*
	 * Process FIN, check for duplicates and make sure all data is in.
	 */
	if (n->t_flags&T_FIN && !tp->dropped_txt) {		
		if (tp->fin_rcvd) 
			tp->ack_due = TRUE;
		else {
			/*
			 * Check if we really have FIN 
			 * (rcv buf filled in, no drops) 
			 */
			last = firstempty(tp);
			if ((tp->t_rcv_prev == (struct th *)tp &&
			     SEQ_EQ(last, t_end(n)+1)) || 
			    SEQ_EQ(last, t_end(tp->t_rcv_prev))) {
				tp->fin_rcvd = TRUE;
				wakeup(tp->t_ucb);
			}
			/* 
			 * If FIN, then set to ACK: incr rcv_nxt, since FIN 
			 * occupies sequence space 
			 */
			if (tp->fin_rcvd && SEQ_GEQ(tp->rcv_nxt, last)) {
				tp->rcv_nxt = last + 1;
				tp->ack_due = TRUE;
			}
		}
	}
	/*
	 * If ACK required or rcv window has changed, try to send something.
	 */
	sent = FALSE;
	if (tp->ack_due)                
		sent = send_tcp(tp, TCP_CTL);
	else if (tp->new_window)       
		sent = send_tcp(tp, TCP_DATA);
	/*	
	 *  Set rexmt and round trip timers, if anything sent, and unacked
	 *  data left in queue.
	 */
	if (!sent && SEQ_LT(tp->snd_una, tp->snd_nxt) && tp->cancelled) {
		tp->t_timers[TREXMT] = tp->t_xmtime;
		tp->t_timers[TREXMTTL] = T_REXMTTL;
		tp->t_rexmt_val = tp->t_rtl_val = tp->snd_lst;
		tp->cancelled = FALSE;
	}
}

/*
 * Process incoming ACKs.  Remove data from send queue up to acknowledgement.
 * Also handles round-trip timer for retransmissions and acknowledgement of
 * SYN.
 */
rcv_ack(tp, n)          
register struct tcb *tp;
register struct th *n;
{
	register struct ucb *up;
	register struct mbuf *m;
	register len;

	up = tp->t_ucb;

	len = n->t_ackno - tp->snd_una; 
	tp->snd_una = n->t_ackno;
	if (SEQ_GT(tp->snd_una, tp->snd_nxt)) 
		tp->snd_nxt = tp->snd_una;

	/* if timed message has been acknowledged, use the time to set
	   the retransmission time value */

	if (tp->syn_acked && SEQ_GT(tp->snd_una, tp->t_xmt_val)) {
		tp->t_xmtime = (tp->t_timers[TXMT]>T_REXMT ? tp->t_timers[TXMT]
							   : T_REXMT);
		if (tp->t_xmtime > T_REMAX)
			tp->t_xmtime = T_REMAX;
	}

	/* handle ack of opening syn (tell user) */

	if (!tp->syn_acked && SEQ_GT(tp->snd_una, tp->iss)) {
		tp->syn_acked = TRUE;
		len--;			/* ignore SYN */
		t_cancel(tp, TINIT);	/* cancel init timer */
	}

	/* remove acknowledged data from send buff */

	m = up->uc_sbuf;
	while (len > 0 && m != NULL) 
		if (m->m_len <= len) {
			len -= m->m_len;
			m = m_free(m);
			up->uc_ssize--;
		} else {
			m->m_len -= len;
			m->m_off += len;
			break;
		}

	up->uc_sbuf = m;
	wakeup(tp->t_ucb);

	/* handle ack of closing fin */

	if (SEQ_NEQ(tp->seq_fin, tp->iss) && SEQ_GT(tp->snd_una, tp->seq_fin))
		tp->snd_fin = FALSE;
	t_cancel(tp, TREXMT);          /* cancel retransmit timer */
	t_cancel(tp, TREXMTTL);        /* cancel retransmit too long timer */
	tp->cancelled = TRUE;      
}

/*
 * Process incoming data.  Put the segments on sequencing queue in order,
 * taking care of overlaps and duplicates.  Data is removed from sequence
 * queue by present_data when sequence is complete (no holes at top).
 * Drop data that falls outside buffer quota if tight for space.  Otherwise,
 * process and recycle data held in tcp_input.
 */
rcv_text(tp, t)                 
register struct tcb *tp;
register struct th *t;
{
	register i;
	register struct th *p, *q;
	register struct mbuf *m, *n;
	struct ucb *up;
	struct th *savq;
	int j;
	sequence last;

	/* throw away any data we have already received */

	if ((i = tp->rcv_nxt - t->t_seq) > 0)  {
		if (i < t->t_len) {
        		t->t_seq += i;
        		t->t_len -= i;
        		m_adj(dtom(t), i);
		} else {
			tp->ack_due = TRUE;	/* send ack just in case */
			return;
		}
	}

	last = t_end(t);                /* last seq # in incoming seg */

	/* # buffers available to con */

	up = tp->t_ucb;
	i = (int)up->uc_rcv - (int)up->uc_rsize;
	if (i < 0)
		i = 0;
	        
	/* count buffers in segment */
	        
	for (m = dtom(t), j = 0; m != NULL; m = m->m_next)
		if (m->m_len != 0)
        		j++;
	        
	/* not enough resources to process segment */

	if (j > i && netcb.n_bufs < netcb.n_lowat) {            

		/* if segment preceeds top of seqeuncing queue, try to take
		   buffers from bottom of queue */

                q = tp->t_rcv_next;             
		if (q != (struct th *)tp && SEQ_LT(tp->rcv_nxt, q->t_seq) &&
		    SEQ_LT(t->t_seq, q->t_seq)) 

			for (p = tp->t_rcv_prev; i < j &&
			     p != (struct th *)tp;) {
				savq = p->t_prev;
				tcp_deq(p);
				i += m_freem(dtom(p));
				p = savq;
			}

		/* if still not enough room, drop text from end of segment */

		if (j > i) {

			for (m = dtom(t); i > 0 && m != NULL; i--)
				m = m->m_next;

        		while (m != NULL) {
        			t->t_len -= m->m_len;
        			last -= m->m_len;
        			m->m_len = 0;
        			m = m->m_next;
        		}
        		tp->dropped_txt = TRUE;
        		if (SEQ_LT(last, t->t_seq))
        			return;
        	}       
	}

	/* merge incoming data into the sequence queue */

        q = tp->t_rcv_next;             /* -> top of sequencing queue */
	        
        /* skip frags which new doesn't overlap at end */
                
        while ((q != (struct th *)tp) && SEQ_GT(t->t_seq, t_end(q)))
        	q = q->t_next;
                
        if (q == (struct th *)tp) {     /* frag at end of chain */

		if (SEQ_GEQ(last, tp->rcv_nxt)) {
		        tp->net_keep = TRUE;
        	        tcp_enq(t, tp->t_rcv_prev);
		}

        } else {  

		/* frag doesn't overlap any on chain */

        	if (SEQ_LT(last, q->t_seq)) {  
			tp->net_keep = TRUE;
        		tcp_enq(t, q->t_prev);
                
        	/* new overlaps beginning of next frag only */
                
        	} else if (SEQ_LT(last, t_end(q))) {
        		if ((i = last - q->t_seq + 1) < t->t_len) {
                		t->t_len -= i;
        			m_adj(dtom(t), -i);
				tp->net_keep = TRUE;
        			tcp_enq(t, q->t_prev);
        		} 
                
        	/* new overlaps end of previous frag */
                
        	} else {
        		savq = q;
        		if (SEQ_LEQ(t->t_seq, q->t_seq)) {  /* complete cover */
        			savq = q->t_prev;
        			tcp_deq(q);
        			m_freem(dtom(q));
        		  
        		} else {                        /* overlap */
        			if ((i = t_end(q) - t->t_seq + 1) < t->t_len) {
                			t->t_seq += i;  
                			t->t_len -= i;  
                			m_adj(dtom(t), i);
				} else 
					t->t_len = 0;
        		}
                
        	/* new overlaps at beginning of successor frags */
                
        		q = savq->t_next;
        		while ((q != (struct th *)tp) && (t->t_len != 0) && 
        		       SEQ_LT(q->t_seq, last))
                
        			/* complete cover */
                
        			if (SEQ_LEQ(t_end(q), last)) {
        				p = q->t_next;
        				tcp_deq(q);
        				m_freem(dtom(q));
        				q = p;
                
        			} else {        /* overlap */
                
        				if ((i = last-q->t_seq+1) < t->t_len) {
                				t->t_len -= i;
                				m_adj(dtom(t), -i);
					} else
						t->t_len = 0;
        				break;
        			}
                
        	/* enqueue whatever is left of new before successors */
                
        		if (t->t_len != 0) {
				tp->net_keep = TRUE;
        			tcp_enq(t, savq);
			}
        	}
        }

	/* set to ack completed data (no gaps) */

	tp->rcv_nxt = firstempty(tp);
	tp->ack_due = TRUE;

	/* if any room remaining in rcv buf, take any unprocessed
	   messages and schedule for later processing */

	if ((m = tp->t_rcv_unack) != NULL && 
	    (i = (int)up->uc_rcv - (int)up->uc_rsize) > 0 &&
	    netcb.n_bufs > netcb.n_lowat)
		do {
		
			/* schedule work request */

			t = mtod(m, struct th *);
			j = (t->t_off << 2) + sizeof(struct ip);
			m->m_off += j;
			m->m_len -= j;
			tp->t_rcv_unack = m->m_act;
			m->m_act = (struct mbuf *)0;
			netstat.t_unack++;
			w_alloc(INRECV, 0, tp, t);     

			/* remaining buffer space */

			for (n = m; n != NULL; n = n->m_next)
				i--;
		} while ((m = tp->t_rcv_unack) != NULL && i > 0);        
}

/*
 * Send a reset segment
 */
send_rst(tp, n)                         
register struct tcb *tp;
register struct th *n;
{
	register struct ucb *up;
	struct socket src, dst;
	u_short port;
	int temp_rst;

        /* don't send a reset in response to a reset */

	if (n->t_flags&T_RST || (up = tp->t_ucb) == NULL)
		return;

	tp->snd_rst = TRUE;
	temp_rst = FALSE;
	if (n->t_flags&T_ACK)
		tp->snd_nxt = n->t_ackno;

	/* if reset required from "wildcard" listener, take addresses and
	   port from incoming packet */

	if (up->uc_local.s_addr == 0 || up->uc_host.s_addr == 0 || 
	    tp->t_fport == 0) {
		src = up->uc_local;
		dst = up->uc_host;
		port = tp->t_fport;
		up->uc_local = n->t_d;
		up->uc_host = n->t_s;
		tp->t_fport = n->t_src;
		temp_rst = TRUE;
	}
	tp->syn_rcvd = FALSE;
	send_pkt(tp, 0, 0, NULL);
	tp->ack_due = FALSE;
	tp->snd_rst = FALSE;

	/* restore "wildcard" addresses */

	if (temp_rst) {
		up->uc_local = src;
		up->uc_host = dst;
		tp->t_fport = port;
		tp->snd_nxt = tp->iss;
		if (up->uc_route != NULL) {
			h_free(up->uc_route);
			up->uc_route = NULL;
		}
	}
}

/*
 * Accept data for the user to receive.  Moves data from sequenced tcp
 * segments from the sequencing queue to the user's receive queue (in the
 * ucb).  Observes locking on receive queue.
 */
present_data(tp)                
register struct tcb *tp;
{
	register struct th *t;
	register struct ucb *up;
	register struct mbuf *m, *n, *top, *bot;
	register sequence ready;
	int i;

	up = tp->t_ucb;                 /* -> ucb */

	/* connection must be synced and data available for user */

	if (tp->syn_acked && (t = tp->t_rcv_next) != (struct th *)tp) {

		/* lock the user's receive buffer */

		while (up->uc_flags & ULOCK)
			sleep(&up->uc_flags, PZERO);
		up->uc_flags |= ULOCK;

		/* find the end of the user's receive buffer */

		m = up->uc_rbuf;
		if (m != NULL)
			while (m->m_next != NULL)
				m = m->m_next;

		/* move as many mbufs as possible from tcb to user queue */

		ready = firstempty(tp);
		while (up->uc_rsize < up->uc_rcv && t != (struct th *)tp && 
	  	       SEQ_LT(t_end(t), ready)) {

			/* count mbufs in msg chunk and free null ones */

			bot = NULL;
			for (n = top = dtom(t); n != NULL;) { 
				if (n->m_len == 0) {
					if (n == top)  
						top = n = m_free(n);
					else
						bot->m_next = n = m_free(n);
				} else {
					bot = n;
					up->uc_rsize++;
					n = n->m_next;
				}
			}

			/* chain new data to user receive buf */

			if (m == NULL)
				up->uc_rbuf = top;
			else 
				m->m_next = top;

			if (bot != NULL)
        			m = bot;

			/* dequeue chunk from tcb */

			tcp_deq(t);
			t = t->t_next;
		}

		/* unlock receive queue for user */

		up->uc_flags &= ~ULOCK;
		wakeup(&up->uc_flags);

		/* awaken reader only if any data on user rcv queue */

		if (up->uc_rsize != 0)
			wakeup(up);

		/* let user know about foreign tcp close if no more data */

		if (tp->fin_rcvd && !tp->usr_closed && rcv_empty(tp))
			to_user(up, UCLOSED);  
	}
}
