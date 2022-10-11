#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/udp.h"
#include "../bbnnet/ucb.h"

extern int nosum;
/*
 *  Process incoming udb messages.  Called directly from ip_input.
 *  User sees udp header with pseudo-header which overlays ip header
 *  (defined in udp.h).
 */
udp_input(mp, ip)
register struct mbuf *mp;
struct ifcb ip;
{
	register struct udp *p;
	register struct ucb *up;
	register u_short lport, fport, i, ulen;

	p = mtod(mp, struct udp *);
	mp->m_off += sizeof p->u_x;
	mp->m_len -= sizeof p->u_x;
	p->u_x1 = 0;
	p->u_ilen = p->u_len;
	ulen = short_from_net(p->u_len);
	lport = short_from_net(p->u_dst);
	fport = short_from_net(p->u_src);
	/*
	 * Do checksum calculation.  Assumes pseudo-header passed up from
	 * IP level and finished above.
	 */
	if ((i = p->u_sum) != 0) {
#ifdef mbb
		i = short_from_net(i);
#endif
		p->u_sum = 0;
		if (i != (u_short)cksum(mp, ulen + UDPCKSIZE)) {
			netstat.u_badsum++;
			if (nosum)
				goto ignore;
			netlog(mp);
			return;
		}
	} 
ignore:
			
	/* find a ucb for incoming message (exact match) */

	for (up = netcb.n_ucb_hd; up != NULL &&
		   (!(up->uc_flags & UUDP) ||
		    up->uc_udp.u_lport != lport ||
		 /* up->uc_udp.u_fport != fport ||  spec only calls for dst */
		    up->uc_host.s_addr != p->u_s.s_addr ||
		    up->uc_local.s_addr!= p->u_d.s_addr); up = up->uc_next);
	
	/* if no exact match, try wild card match */

	if (up == NULL)
		for (up = netcb.n_ucb_hd; up != NULL &&
			(!(up->uc_flags & UUDP) ||
			 up->uc_udp.u_lport != lport ||
		/*	 (up->uc_udp.u_fport != fport && 
			  up->uc_udp.u_fport != 0) ||  spec only calls for dst*/
			 (up->uc_host.s_addr != p->u_s.s_addr &&
			  up->uc_host.s_addr != 0) ||
			 (up->uc_local.s_addr != p->u_d.s_addr &&
			  up->uc_local.s_addr != 0)); up = up->uc_next);
		
	/* if a user is found, queue the data, otherwise drop it */

	if (up != NULL) {
		mp->m_off -= sizeof p->u_x;
		mp->m_len += sizeof p->u_x;
		raw_queue(mp, up);
	} else {
		netstat.u_drops++;
		m_freem(mp);
	}
}

/*
 * Output a udp message.  Called from netuser in the manner of the raw
 * message interface.  Passed an mbuf chain whose first buffer is empty
 * and used for leader construction.
 */
udp_output(up, mp, len)
struct ucb *up;
register struct mbuf *mp;
int len;
{
	register struct udp *p, *q;
	register struct mbuf *m;
	int i;

	switch (up->uc_flags & RAWMASK) {

	case RAWCOMP:
		/*
		 * Compose header in first mbuf.  Get addresses and ports
		 * from ucb, add in pseudo-header fields for checksum.
		 */
		mp->m_off = MSIZE - sizeof(struct udp);
		mp->m_len = sizeof(struct udp);
		p = mtod(mp, struct udp *);
		p->u_len = short_to_net(len+UDPSIZE);
		p->u_s = up->uc_local;
		p->u_d = up->uc_host;
		p->u_src = short_to_net(up->uc_udp.u_lport);
		p->u_dst = short_to_net(up->uc_udp.u_fport);
		break;

	case RAWVER:
	case RAWASIS:
		/*
		 * Header built by user.  Copy the header into the first
		 * mbuf so lower levels can add their leaders.  Assumes
		 * the user had used the udp struct that overlays an ip
		 * header.  The source and destination addresses of the
		 * pseudo-header are assumed, everything else filled in here.
		 */
		m = mp->m_next;
		if (m->m_len < sizeof(struct udp)) {
			u.u_error = ERAWBAD;
			m_freem(mp);
			return;
		}
		q = mtod(m, struct udp *);
		mp->m_off = MSIZE - sizeof(struct udp);
		mp->m_len = sizeof(struct udp);
		p = mtod(mp, struct udp *);
		bcopy((caddr_t)q, (caddr_t)p, sizeof(struct udp));
		m->m_off += sizeof(struct udp);
		m->m_len -= sizeof(struct udp);
		len -= sizeof(struct udp);	/* want data length only here */
		break;
	
	default:
		m_freem(mp);
		u.u_error = ENETPARM;
		return;
	}
	/*
	 * Fill in remainder of pseudo-header.
	 */
	p->u_x1 = 0;
	p->u_pr = UDPROTO;
	p->u_ilen = p->u_len;			/* redundant! */
	/*
	 * Do checksum for all but ASIS.  Include pseudo header.
	 */
	if (!(up->uc_flags&RAWASIS)) {
		mp->m_off += sizeof p->u_x;
		mp->m_len -= sizeof p->u_x;
		p->u_sum = 0;
#ifndef mbb
		p->u_sum = cksum(mp, len + sizeof(struct udp) - sizeof p->u_x);
#else
		i = cksum(mp, len + sizeof(struct udp) - sizeof p->u_x);
		p->u_sum = short_to_net(i);
#endif
		mp->m_off -= sizeof p->u_x;
		mp->m_len += sizeof p->u_x;
	}
	/*
	 * Now send the packet via IP.
	 */
	if (!ip_send(up, mp, UDPROTO, len+UDPSIZE, 0, NULL, FALSE))
		u.u_error = ERAWBAD;
}
