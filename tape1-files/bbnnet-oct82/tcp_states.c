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
 * These are the action routines of the TCP finite state machine.  They are
 * called from the TCP fsm dispatcher (in tcp_input.c).  These routines call 
 * on the routines in tcp_procs.c to do the actual segment processing.
 */
lis_cls(wp)             /* passive open (1) */
struct work *wp;
{
	t_open(wp->w_tcb, TCP_PASSIVE);
	return(LISTEN);
}

sys_cls(wp)             /* active open (6) */
register struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	t_open(tp, TCP_ACTIVE);
	send_tcp(tp, TCP_CTL);		/* send SYN */
	return(SYN_SENT);
}

cls_opn(wp)             /* close request before receiving foreign SYN (10) */
struct work *wp;                           
{
	t_close(wp->w_tcb, UCLOSED);
	return(CLOSED);
}

cl2_clw(wp)             /* close request after receiving foreign FIN (13) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	tp->snd_fin = TRUE;             /* send our own FIN */
	send_tcp(tp, TCP_CTL);                   
	tp->usr_closed = TRUE;
	return(CLOSING2);
}

cls_rwt(wp)             /* rcv request after foreign close (20) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	present_data(tp);       /* present any remaining data */
	if (rcv_empty(tp)) {
        	t_close(tp, UCLOSED);
         	return(CLOSED);
	} else
		return(RCV_WAIT);

}

fw1_syr(wp)             /* close request on synched connection (24,25) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	tp->snd_fin = TRUE;                     /* send FIN */
	send_tcp(tp, TCP_CTL);
	tp->usr_closed = TRUE;
	return(FIN_W1);
}

sss_syn(wp)             /* incoming seq on open connection (39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	rcv_tcp(tp, wp->w_dat, TCP_DATA);
	present_data(tp);
	return(SAME);
}

sss_snd(wp)             /* send request on open connection (40,41) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct ucb *up = tp->t_ucb;
	register struct mbuf *m, *n;
	register off;
	sequence last;

	last = tp->snd_una;

	/* count number of mbufs in send data */

	for (m = n = (struct mbuf *)wp->w_dat; m != NULL; m = m->m_next) {
		up->uc_ssize++;
		last += m->m_len;
	}

	/* find end of send buffer and append data */

	if ((m = up->uc_sbuf) != NULL) {        /* something in send buffer */
		while (m->m_next != NULL) {             /* find the end */
			m = m->m_next;
			last += m->m_len;
		}
		last += m->m_len;

		/* if there's room in old buffer for new data, consolidate */

		off = m->m_off + m->m_len;
		while (n != NULL && (MSIZE - off) >= n->m_len) {
			bcopy((caddr_t)((int)n + n->m_off), 
			      (caddr_t)((int)m + off), n->m_len);
			m->m_len += n->m_len;
			off += n->m_len;
			up->uc_ssize--;
			n = m_free(n);
		}
		m->m_next = n;
	} else                                  /* nothing in send buffer */
		up->uc_sbuf = n;

	if (up->uc_flags & UEOL) {		/* set PUSH */
		tp->snd_end = last;
	}
	if (up->uc_flags & UURG) {              /* urgent data */
		tp->snd_urp = last+1;
		tp->snd_urg = TRUE;
	} 
	send_tcp(tp, TCP_DATA);
	return(SAME);
}

sss_rcv(wp)             /* rcv request on open connection (42) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	present_data(tp);

	/* if last window sent was zero, send an ACK to update window */

	if (tp->sent_zero)
		send_tcp(tp, TCP_CTL);
	return(SAME);
}

cls_nsy(wp)                  /* abort request on unsynched connection (44) */
struct work *wp;
{
	t_close(wp->w_tcb, UABORT);
	return(CLOSED);
}

cls_syn(wp)             /* abort request on synched connection (45) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;

	tp->snd_rst = TRUE;            /* send reset */
	send_pkt(tp, 0, 0, NULL);
	tp->ack_due = FALSE;
	t_close(tp, UABORT);
	return(CLOSED);
}

cls_act(wp)             /* net closing open connection (47) */
struct work *wp;
{
	t_close(wp->w_tcb, UABORT);
	return(CLOSED);
}

cls_err(wp)             /* invalid user request in closing states */
struct work *wp;
{
	to_user(wp->w_tcb->t_ucb, UCLSERR);
	return(SAME);
}

lis_netr(wp)             /* incoming seg in LISTEN (3,4) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;
	register struct ucb *up = tp->t_ucb;
	register struct ifcb *ip;
	struct socket local;

	/* fill in unspecified foreign host address. */

	up->uc_host = n->t_s;
	tp->t_fport = n->t_src;
	up->uc_local = n->t_d;
	local.s_addr = addr_1822(n->t_d);

	/* pick a network interface to respond on */
	
	for (ip = netcb.n_ifcb_hd; ip != NULL; ip = ip->if_next) 
		if (ip->if_addr.s_addr == local.s_addr && ip->if_avail) {
			up->uc_srcif = ip;
			tp->t_maxseg = MIN(ip->if_mtu - TCPIPMAX, tp->t_maxseg);
			break;
		}

	/* in case we can't find a good i/f, close the connection,  foreign
	   host will get a reset on his next retransmission */

	if (ip == NULL) {
		t_close(tp, UNRCH);
		return(CLOSED);
	}
	rcv_tcp(tp, n, TCP_DATA);

	if (!tp->fin_rcvd) {            /* no FIN (4) */

		/* start init timer now that we have foreign host */

		tp->t_timers[TINIT] = T_INIT/2;
		return(L_SYN_RCVD);
	} else {                        /* got a FIN, start timer (3) */
        	tp->t_timers[TFINACK] = T_2ML;
        	tp->waited_2_ml = FALSE;
		return(CLOSE_WAIT);
	}
}

sys_netr(wp)            /* incoming segment after SYN sent (8,9,11,32) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	rcv_tcp(tp, n, TCP_DATA);
	if (tp->fin_rcvd) {             /* got a FIN */

		/* if good ACK, present any data */

		if (n->t_flags&T_ACK) {
			if (SEQ_GT(n->t_ackno, tp->iss))	/* 32 */
				present_data(tp);
		} else {                                /* 9 */
                	tp->t_timers[TFINACK] = T_2ML;
                	tp->waited_2_ml = FALSE;
		}
		return (CLOSE_WAIT);
	} else                          /* no FIN */

		/* if good ACK, open connection, otherwise wait for one */

		if (n->t_flags&T_ACK) {			/* 11 */
			present_data(tp);
			return(ESTAB);
		} else
			return(SYN_RCVD);               /* 8 */
}

cl1_netr(wp)            /* incoming seg after we closed (15,18,22,23,30,39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	if (ack_fin(tp, n)) {			/* got ACK of our FIN */
        	if (n->t_flags&T_FIN) {		/* got for FIN (23) */
        		rcv_tcp(tp, n, TCP_CTL);
                	tp->t_timers[TFINACK] = T_2ML;
                	tp->waited_2_ml = FALSE;
                        return(TIME_WAIT);
        	} else {

			/* if wait done, see if any data left for user */

        		if (tp->waited_2_ml)
        			if (rcv_empty(tp)) {    /* 15 */
        				t_close(tp, UCLOSED);
        				return(CLOSED);
        			} else
        				return(RCV_WAIT);       /* 18 */
        		else
        			return(TIME_WAIT);      /* 22 */
		}
	} else {				/* our FIN not ACKed yet */
		if (n->t_flags&T_FIN) {		/* rcvd for FIN (30) */
			rcv_tcp(tp, n, TCP_CTL);
                	tp->t_timers[TFINACK] = T_2ML;
                	tp->waited_2_ml = FALSE;
		} else {		/* no FIN, just proc new data (39) */
        		rcv_tcp(tp, n, TCP_DATA);
        		present_data(tp);
		}
	}
	return(SAME);
}

cl2_netr(wp)            /* incoming seg after foreign close (16,19,31,39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	if (ack_fin(tp, n)) {                   /* this is ACK of our fin */

		/* if no data left for user, close; otherwise wait */

		if (rcv_empty(tp)) {                            /* 16 */
			t_close(tp, UCLOSED);
			return(CLOSED);
		} else                                          /* 19 */
			return(RCV_WAIT);
	} else {				/* no ACK of our FIN */
		/* duplicate FIN or data */

		if (n->t_flags&T_FIN)				/* 31 */
			send_tcp(tp, TCP_CTL);		/* ACK duplicate FIN */
		else {                                          /* 39 */
			rcv_tcp(tp, n, TCP_DATA);                         
			present_data(tp);
		}
	}
	return(SAME);
}

fw1_netr(wp)            /* incoming seg after user close (26,27,28,39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	/* process any incoming data, since we closed but they didn't */

	rcv_tcp(tp, n, TCP_DATA);
	present_data(tp);

	/* send any data remaining on send buffer */

	send_tcp(tp, TCP_DATA);
	if (ack_fin(tp, n)) {			/* our FIN got ACKed */
		if (tp->fin_rcvd) {                     /* got for FIN (28) */
                	tp->t_timers[TFINACK] = T_2ML;
                	tp->waited_2_ml = FALSE;
			return(TIME_WAIT);
		} else                                  /* no FIN, wait (27) */
			return(FIN_W2);
	} else {				/* no ACK of FIN */
		if (tp->fin_rcvd) {                     /* got for FIN (26) */
                	tp->t_timers[TFINACK] = T_2ML;
                	tp->waited_2_ml = FALSE;
			return(CLOSING1);
                } 
	}
	return(SAME);                                   /* 39 */
}

syr_netr(wp)             /* incoming seg after SYN rcvd (5,33) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	rcv_tcp(tp, n, TCP_DATA);
	present_data(tp);

	/* if no FIN, open connection, otherwise wait for user close */

	if (tp->fin_rcvd)                               /* 33 */
		return(CLOSE_WAIT);
	else                                            /* 5 */
		return(ESTAB);
}

est_netr(wp)            /* incoming seg on open connection (12,39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	rcv_tcp(tp, n, TCP_DATA);
	present_data(tp);

	/* if no FIN, remain open, otherwise wait for user close */

	if (tp->fin_rcvd)                       /* 12 */
		return(CLOSE_WAIT);
	else                                    /* 39 */
        	return(SAME);
}

fw2_netr(wp)            /* incoming seg while waiting for for FIN (12,39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	/* process data since we closed, but they may not have */

	rcv_tcp(tp, n, TCP_DATA);
	present_data(tp);

	/* if we get the FIN, start the finack timer, else keep waiting */

	if (tp->fin_rcvd) {                     /* got for FIN (29) */
		tp->t_timers[TFINACK] = T_2ML;
		tp->waited_2_ml = FALSE;
		return(TIME_WAIT);
	} else                                  /* 39 */
        	return(SAME);
}

cwt_netr(wp)            /* incoming seg after exchange of FINs (30,31,39) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	/* either duplicate FIN or data */

	if (n->t_flags&T_FIN) {
		if (n->t_flags&T_ACK && SEQ_LEQ(n->t_ackno, tp->seq_fin)) {	
                	rcv_tcp(tp, n, TCP_CTL);
                	tp->t_timers[TFINACK] = T_2ML;
                	tp->waited_2_ml = FALSE;
		} else                                          /* 31 */
			send_tcp(tp, TCP_CTL);
	} else {				/* duplicate data (39) */
		rcv_tcp(tp, n, TCP_DATA);
		present_data(tp);
	}
	return(SAME);
}

rwt_netr(wp)            /* incoming seg while waiting for user rcv (30,21) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register struct th *n = (struct th *)wp->w_dat;

	/* handle duplicate ACK of our FIN */

	if (n->t_flags&T_FIN && n->t_flags&T_ACK && 
	    SEQ_LEQ(n->t_ackno, tp->seq_fin)) { 		/* 30 */
        	rcv_tcp(tp, n, TCP_CTL);
        	tp->t_timers[TFINACK] = T_2ML;
        	tp->waited_2_ml = FALSE;
	} 
	return(SAME);
}

timers(wp)              /* timer processor (14,17,34,35,36,37,38) */
struct work *wp;
{
	register struct tcb *tp = wp->w_tcb;
	register type = wp->w_stype;

	switch (type) {

	case TINIT:             /* initialization timer */

		if (!tp->syn_acked) {	/* haven't got ACK of our SYN (35) */
			t_close(tp, UINTIMO);
			return(CLOSED);
		}
		break;

	case TFINACK:           /* fin-ack timer */   

		if (tp->t_state == TIME_WAIT) {

			/* can be sure our ACK of for FIN was rcvd,
			   can close if no data left for user */

			if (rcv_empty(tp)) {            /* 14 */
				t_close(tp, UCLOSED);
				return(CLOSED);
			} else                          /* 17 */
				return(RCV_WAIT);

		} else if (tp->t_state == CLOSING1)     /* 37 */

			/* safe to close */

			tp->waited_2_ml = TRUE;

	        break;

	case TREXMT:            /* retransmission timer */

		/* set up for a retransmission, increase rexmt time
		   in case of multiple retransmissions. */

	        if (SEQ_GT(tp->t_rexmt_val, tp->snd_una)) {   /* 34 */
	        	tp->snd_nxt = tp->snd_una;
	        	tp->rexmt = TRUE;
			tp->t_rxtct++;
			tp->t_xmtime = tp->t_xmtime << 1;                  
        		if (tp->t_xmtime > T_REMAX)
        			tp->t_xmtime = T_REMAX;
	        	send_tcp(tp, TCP_DATA);
	        }
		break;

	case TREXMTTL:          /* retransmit too long */

		/* tell user */

        	if (SEQ_GT(tp->t_rtl_val, tp->snd_una))		/* 36 */
        		to_user(tp->t_ucb, URXTIMO);

		/* if user has already closed, abort the connection */

		if (tp->usr_closed) {
			t_close(tp, URXTIMO);
			return(CLOSED);
		}
		break;

	case TPERSIST:          /* persist timer */

		/* force a byte send through closed window */

        	tp->force_one = TRUE;                   /* 38 */
        	send_tcp(tp, TCP_DATA);
		break;
	}
	return(SAME);
}

netprepr(tp, n)         /* net preproc (66,67,68,69,70,71,72,73,74,75,76) */
register struct tcb *tp;
register struct th *n;
{
	register struct ucb *up;

	switch (tp->t_state) {

	case LISTEN:
		/*
		 * Ignore resets, ACKs cause resets, must have SYN.
		 */
		if (n->t_flags&T_RST)
			break;
		else if (n->t_flags&T_ACK)
			send_rst(tp, n);
		else if (n->t_flags&T_SYN)
			return(0);
		break;

	case SYN_SENT:
		/*
		 * Bad ACKs cause resets, good resets close, must have SYN.
		 */
		if (n->t_flags&T_ACK && !ack_ok(tp, n))
			send_rst(tp, n);		
		else if (n->t_flags&T_RST) {
			if (n->t_flags&T_ACK) {
				t_close(tp, URESET);
				return(CLOSED);
			}
		} else if (n->t_flags&T_SYN)
			return(0);
		break;

	default:
		/*
		 * Sequence number must fall in window, otherwise just
		 * ACK and drop.
		 */
		if (SEQ_LT(n->t_seq, tp->rcv_nxt) ||
		    SEQ_GT(n->t_seq, tp->rcv_nxt + rcv_resource(tp)))
			send_tcp(tp, TCP_CTL);
		/*
		 * Acceptable resets close in all states except L_SYN_RCVD,
		 * where connection returns to LISTEN.
		 */
		else if (n->t_flags&T_RST) {		
			if (tp->t_state == L_SYN_RCVD) {
				t_cancel(tp, TREXMT);
				t_cancel(tp, TREXMTTL);
				t_cancel(tp, TPERSIST);
				up = tp->t_ucb;
				up->uc_host.s_addr = NULL;
				if (up->uc_route != NULL) {
					h_free(up->uc_route);
					up->uc_route = NULL;
				}
				return(LISTEN);
			} else {                        /* 66 */
				t_close(tp, URESET);
				return(CLOSED);
			}
		/*
		 * No SYNs allowed in window.
		 */
		} else if (n->t_flags&T_SYN) {
			send_rst(tp, n);        
			t_close(tp, URESET);
			return(CLOSED);
		/*
		 * Must have good ACK.  Bad ACKs cause resets only in
		 * SYN_RCVD states.
		 */
		} else if (n->t_flags&T_ACK)
			if (!ack_ok(tp, n)) {
				if (tp->t_state == SYN_RCVD || 
				    tp->t_state == L_SYN_RCVD)
					send_rst(tp, n);
			} else
				return(0);	/* acceptable segment */
	}
	return(-1);     /* tell caller to eat segment (unacceptable) */
}
