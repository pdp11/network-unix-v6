/*
 * MODIFIED:
 *	7 Sept 82	Jeffrey Mogul @ Stanford
 *	- as per Gurwitz's suggestion, moved ip_adj back into
 *	  common code and removed it from drivers.
 */

#include "param.h"
#include "mbuf.h"
#include "net.h"
#include "ifcb.h"
#include "ip.h"
#include "tcp.h"
#include "raw.h"
#include "icmp.h"
#include "udp.h"
#include "ucb.h"

int nosum = 0;

/*
 * IP level input routine 
 *
 * Called from 1822 level upon receipt of an internet datagram or fragment.  
 * This routine does fragment reassembly, if necessary, and passes completed
 * datagrams to higher level protocol processing routines on the basis of the 
 * ip header protocol field.  It is passed a pointer to an mbuf chain 
 * containing the datagram/fragment. The mbuf offset/length are set to point 
 * at the ip header.
 */
ip_input(mp, ifp)
struct mbuf *mp;
struct ifcb *ifp;
{
	register struct ip *ip, *q;
	register struct ipq *fp;
	register struct mbuf *m;
	register struct mbuf *n;
	register i;
	int hlen;
	struct ip *p, *savq;
	struct proto *r;

	ip = mtod(mp, struct ip *);        /* ->ip hdr */

	/* make sure header does not overflow mbuf */

	if ((hlen = ip->ip_hl << 2) > mp->m_len) {
		printf("ip header overflow\n");
		netlog(mp);
		return;
	}

	/*
	 * Adjust msg length to remove any driver padding.  Make sure that
	 * message length matches ip length.
	 */

	for (i = 0, n = mp; n->m_next != NULL; n = n->m_next) 
		i += n->m_len;
	i -= short_from_net(ip->ip_len) - n->m_len;
	if (i != 0) 
		if (i > (int)n->m_len)
			m_adj(mp, -i);
		else
			n->m_len -= i;

#ifndef mbb
	i = (u_short)ip->ip_sum;
#else
	i = (u_short)short_from_net(ip->ip_sum);
#endif mbb
	ip->ip_sum = 0;

	if (i != (u_short)cksum(mp, hlen)) {     /* verify checksum */
		netstat.ip_badsum++;
		if (!nosum) {
			netlog(mp);
        		return;
		}
	}

	/* make sure this packet is addressed to us */

	if (ip->ip_dst.s_addr != ifp->if_addr.s_addr) {
		netstat.ip_drops++;
		netlog(mp);
		return;
	}

	ip->ip_len = short_from_net(ip->ip_len);
	ip->ip_off = short_from_net(ip->ip_off);
	ip->ip_len -= hlen;     /* length of data */
  	ip->ip_mff = ((ip->ip_off & ip_mf) ? TRUE : FALSE);                
	ip->ip_off <<= 3;                                                 

	/* look for chain on reassembly queue with this header */

	netcb.n_ip_lock++;      /* lock frag reass.q */
	for (fp = netcb.n_ip_head; (fp != NULL && (
			ip->ip_src.s_addr != fp->iqh.ip_src.s_addr ||
			ip->ip_dst.s_addr != fp->iqh.ip_dst.s_addr ||
			ip->ip_id != fp->iqh.ip_id ||
			ip->ip_p != fp->iqh.ip_p)); fp = fp->iq_next);

	if (!ip->ip_mff && ip->ip_off == 0) {    /* not fragmented */

		if (fp != NULL) {               /* free existing reass chain */
		        
			q = fp->iqx.ip_next;    /* free mbufs assoc. w/chain */
			while (q != (struct ip *)fp) {
				p = q->ip_next;
				m_freem(dtom(q));
				q = p;
			}
			ip_freef(fp);           /* free header */
		} 

		if (hlen > sizeof(struct ip))
			ip_opt(ip, hlen);	/* option processing */

	} else {                       

        	/* -> msg buf beyond ip hdr if not first fragment */

		if (ip->ip_off != 0) {
                	mp->m_off += hlen;
                	mp->m_len -= hlen;
		}

		if (fp == NULL) {               /* first fragment of datagram in */

		/* set up reass.q header: enq it, set up as head of frag
		   chain, set a timer value, and move in ip header */

			if ((m = m_get(1)) == NULL) {   /* allocate an mbuf */
				m_freem(mp);
				return;
			}

			fp = (struct ipq *)((int)m + MHEAD);
			fp->iqx.ip_next = fp->iqx.ip_prev = (struct ip *)fp; 
			bcopy(ip, &fp->iqh, 
			      MIN(MLEN-(sizeof(struct ipq)-sizeof(struct ip)), 
			    		hlen));
			fp->iqx.ip_ttl = MAXTTL;
			fp->iq_next = NULL;                   
			fp->iq_prev = netcb.n_ip_tail;
			if (netcb.n_ip_head != NULL) 
				netcb.n_ip_tail->iq_next = fp;
			else 
				netcb.n_ip_head = fp;
			netcb.n_ip_tail = fp;
		} 

		/*
                 * Merge fragment into reass.q
		 *
                 * Algorithm:   Match  start  and  end  bytes  of new
                 * fragment  with  fragments  on  the  queue.   If no
                 * overlaps  are  found,  add  new  frag. to the queue.
                 * Otherwise, adjust start and end of new frag.  so  no
                 * overlap   and   add  remainder  to  queue.   If  any
                 * fragments are completely covered by the new one,  or
                 * if  the  new  one is completely duplicated, free the
                 * fragments.
                 */	              
	        q = fp->iqx.ip_next;    /* -> top of reass. chain */
		ip->ip_end = ip->ip_off + ip->ip_len - 1;

		/* skip frags which new doesn't overlap at end */

		while ((q != (struct ip *)fp) && (ip->ip_off > q->ip_end))
			q = q->ip_next;

		if (q == (struct ip *)fp) {	/* frag at end of chain */
			ip_enq(ip, fp->iqx.ip_prev);
		} else {			/* frag doesn't overlap any */ 
			if (ip->ip_end < q->ip_off) {
				ip_enq(ip, q->ip_prev);

			/* new overlaps beginning of next frag only */

			} else if (ip->ip_end < q->ip_end) {
				if ((i = ip->ip_end-q->ip_off+1) < ip->ip_len) {
        				ip->ip_len -= i;
        				ip->ip_end -= i;
					m_adj(mp, -i);
					ip_enq(ip, q->ip_prev);
				} else
					m_freem(mp);

			/* new overlaps end of previous frag */

			} else {
				savq = q;
				if (ip->ip_off <= q->ip_off) {  

					/* complete cover */

					savq = q->ip_prev;
					ip_deq(q);
					m_freem(dtom(q));

				} else {	/* overlap */
					if ((i = q->ip_end-ip->ip_off+1) 
								< ip->ip_len) {
        					ip->ip_off += i;  
        					ip->ip_len -= i;  
        					m_adj(mp, i);
					} else
						ip->ip_len = 0;
				}

			/* new overlaps at beginning of successor frags */

				q = savq->ip_next;
				while ((q != (struct ip *)fp) &&
					(ip->ip_len != 0) && 
					(q->ip_off < ip->ip_end)) 

					/* complete cover */

					if (q->ip_end <= ip->ip_end) {
						p = q->ip_next;
						ip_deq(q);
						m_freem(dtom(q));
						q = p;

					} else {        /* overlap */
						if ((i = ip->ip_end-q->ip_off+1) 
								< ip->ip_len) {
        						ip->ip_len -= i;
        						ip->ip_end -= i;
        						m_adj(mp, -i);
						} else
							ip->ip_len = 0;
						break;
					}

			/* enqueue whatever is left of new before successors */

				if (ip->ip_len != 0) {
					ip_enq(ip, savq);
				} else
					m_freem(mp);
			}
		}

		/* check for completed fragment reassembly */

		if ((i = ip_done(fp)) == 0) {
			netcb.n_ip_lock = 0;
			return;
		} else {
			p = fp->iqx.ip_next;            /* -> top mbuf */
			mp = dtom(p);
			p->ip_len = i;                  /* total data length */

			/* option processing */

			if ((hlen = p->ip_hl&lt;&lt;2) > sizeof(struct ip))
				ip_opt(p, hlen);       

			ip_mergef(fp);                  /* clean frag chain */

			/* copy src/dst internet address to header mbuf */

			bcopy(&fp->iqh.ip_src, &p->ip_src, 
					2*sizeof(struct socket));
			ip_freef(fp);                   /* dequeue header */
		}
	}

	/* call next level with completed datagram */

	netcb.n_ip_lock = 0;
	switch (ip->ip_p) {

	case TCPROTO:
		if ((r = netcb.n_tcp_proto) != NULL)
			m = m_copy(mp);
		tcp_input(mp, ifp);
		goto dist;

	case ICMPROTO:
		if ((r = netcb.n_icmp_proto) != NULL)
			m = m_copy(mp);
		icmp(mp, ifp);
		goto dist;

	case GGPROTO:
		if ((r = netcb.n_ggp_proto) != NULL)
			m = m_copy(mp);
		ggp(mp, ifp);
		goto dist;

	case UDPROTO:
		if ((r = netcb.n_udp_proto) != NULL)
			m = m_copy(mp);
		udp_input(mp, ifp);

	dist:		/* dispatch to other tcp/icmp/ggp processes */

		if (r != NULL)
			raw_disp(m, r);
		break;

	default:	/* dispatch to raw processes */

		raw_input(mp, &ip->ip_src, &ip->ip_dst, 
			  (short)ip->ip_p, UIP, FALSE);
	}
}

/*
 * Check to see if fragment reassembly is complete
 */
ip_done(p)      
register struct ip *p;
{
	register struct ip *q;
	register next;

	q = p->ip_next;                       

	if (q->ip_off != 0)
		return(0);
	do {
		next = q->ip_end + 1;
		q = q->ip_next;
	} while ((q != p) && (q->ip_off == next));

	if ((q == p) && !(q->ip_prev->ip_mff))        /* all fragments in */
		return(next);                         /* total data length */
	else
		return(0);
}

/*
 * Merge mbufs of fragments of completed datagram 
 */
ip_mergef(p)    
register struct ip *p;
{
	register struct mbuf *m, *n;
	register struct ip *q;
	int dummy;

	q = p->ip_next;                         /* -> bottom of reass chain */
	n = (struct mbuf *)&dummy;              /* dummy for init assignment */

	while (q != p) {        /* through chain */

	        n->m_next = m = dtom(q);
		while (m != NULL) 
			if (m->m_len != 0) {
				n = m;
				m = m->m_next;
			} else                  /* free null mbufs */
				n->m_next = m = m_free(m);
		q = q->ip_next;
	}
}

/*
 * Dequeue and free reass.q header 
 */
ip_freef(fp)	        
register struct ipq *fp;
{
	if (fp->iq_prev != NULL)                               
		(fp->iq_prev)->iq_next = fp->iq_next;  
	else                                           
		netcb.n_ip_head = fp->iq_next;         
	if (fp->iq_next != NULL)                               
		(fp->iq_next)->iq_prev = fp->iq_prev;  
	else
		netcb.n_ip_tail = fp->iq_prev;
	m_free(dtom(fp));
}

/*
 * Process ip options 
 */
ip_opt(ip, hlen)        
struct ip *ip;
int hlen;
{
	register char *p, *q;
	register i, len;
	register struct mbuf *m;
	int optlen;	

	p = q = (char *)((int)ip + sizeof(struct ip));  /* -> at options */
	
	if ((optlen = i = hlen - sizeof(struct ip)) > 0) {       

/*      *** IP OPTION PROCESSING ***

		while (i > 0)

  			switch (*q++) {
			case IP_END_OPT:                 
				i--;
				goto done;
			case IP_NOP_OPT:                 
				i--;
				break;

			default:
				i -= *q;
				q += *q;
			}
*/
done:		q += i;
		m = dtom(q);
		len = (int)m + m->m_off + m->m_len - (int)q;
		bcopy((caddr_t)q, (caddr_t)p, len);    /* remove options */
		m->m_len -= optlen;
	}
}

#ifdef	IPADJ
/*
 * Adjust msg length to remove any driver padding.  Make sure that message
 * length matches ip length.
 */
ip_adj(m, p)
register struct mbuf *m;
register struct ip *p;
{
	register struct mbuf *n;
	register i;

	for (i = 0, n = m; n->m_next != NULL; n = n->m_next) 
		i += n->m_len;
	i -= short_from_net(p->ip_len) - n->m_len;
	if (i != 0) 
		if (i > (int)n->m_len)
			m_adj(m, -i);
		else
			n->m_len -= i;
}
#endif	IPADJ

/*
 * IP fragment reassembly timeout routine.
 */
ip_timeo()      
{
	register struct ip *p, *q;
	register struct ipq *fp;

	if (netcb.n_ip_lock)    /* reass.q must not be in use */
		return;

	/* search through reass.q */

	for (fp = netcb.n_ip_head; fp != NULL; fp = fp->iq_next) 

		if (--(fp->iqx.ip_ttl) == 0) {  /* time to die */

			q = fp->iqx.ip_next;    /* free mbufs assoc. w/chain */
			while (q != (struct ip *)fp) {                         
				p = q->ip_next;                                
				m_freem(dtom(q));   
				q = p;                                
			}                                                      
			ip_freef(fp);           /* free header */              
		}
}
