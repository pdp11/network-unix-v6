#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ucb.h"

extern struct user u;

/*
 *	ip_output is called from tcp and passed an mbuf chain of a packet to
 *	send to the local network.  The first mbuf contains a tcp and partial
 *	ip header which is filled in.  After determining a local route for
 *	the packet, it is sent to the local net through ip_send.
 */
ip_output(up, mp, proto, len, optlen, options)
register struct ucb *up;
struct mbuf *mp;
int proto, len, optlen;
char *options;
{
	register struct ip *p, *q;
	int hlen;
	struct socket local;
	struct socket ip_route();

	p = (struct ip *)((int)mp + mp->m_off); /* -> ip header */

	/* Find route for datagram, if one has not been assigned.
	   Make a host table entry for the local network route to
	   the destination */

	if (up->uc_route == NULL) {
		local = ip_route(&p->ip_src, &p->ip_dst);
		if (local.s_addr == NULL || 
		    (up->uc_route = h_make(&local)) == NULL) {
			m_freem(mp);
			return(FALSE);
		} 
	}

	/* copy ip option string to header */

	if (optlen > 0) {
		mp->m_off -= optlen;
		mp->m_len += optlen;
		q = p;
		p = (struct ip *)((int)p - optlen);
		bcopy((caddr_t)q, (caddr_t)p, sizeof(struct ip)); 
		bcopy(options, (caddr_t)((int)p + sizeof(struct ip)), optlen);
	}

	/* fill in ip header fields */

	hlen = sizeof(struct ip) + optlen;
	p->ip_len = len + hlen;
	p->ip_p = proto;
	p->ip_v = IPVERSION;    
	p->ip_hl = hlen >> 2;
	p->ip_off = 0;
	p->ip_ttl = MAXTTL;
	p->ip_id = netcb.n_ip_cnt++;

	return(ip_send(p,up->uc_srcif->if_ifcb,&up->uc_route->h_addr,hlen,FALSE));
}

/*
 *	ip_send is called with a packet with a completed ip header 
 *	(except for checksum) by the raw internet driver.  It fragments the
 *	packet, inserts the ip checksum, and calls the appropriate local net
 *	output routine to send it to the net.
 */
ip_send(p, ip, ap, hlen, asis)
register struct ip *p;
register struct ifcb *ip;
struct socket *ap;
int hlen, asis;
{	 
	register struct mbuf *m, *n;
	register i, rnd;
	struct mbuf *mm;
	int adj, max, len, off;
	unsigned short j;
	unsigned olen;
	
	m = dtom(p);

	if (p->ip_len > ip->if_mtu) {		/* must fragment */
		if (p->ip_off & ip_df) {
			m_freem(m);
			return(FALSE);
		}
		max = ip->if_mtu - hlen;	/* max data length in frag */
		len = p->ip_len - hlen;         /* data length */
		off = 0;                        /* fragment offset */

		while (len > 0) {

			/* correct the header */

			p->ip_off |= off >> 3;

			/* find the end of the fragment */

			i = -hlen;
			while (m != NULL) {
				i += m->m_len;
				if (i > max)
					break;
				n = m;
				m = m->m_next;
			}

			if (i < max || m == NULL) {     /* last fragment */
				p->ip_off = p->ip_off & ~ip_mf;
				p->ip_len = i + hlen;
				break;

			} else {                        /* more fragments */

				/* allocate header mbuf for next fragment */

				if ((mm = m_get(1)) == NULL) {
					m_freem(m);
					return(FALSE);
				}

				p->ip_off |= ip_mf;

				/* terminate fragment at 8 byte boundary 
				   (round down) */

				i -= m->m_len;
        			rnd = i & ~7;           /* fragment length */
				adj = i - rnd;          /* leftover in mbuf */
				p->ip_len = rnd + hlen; 

				/* setup header for next fragment and 
				   append remaining fragment data */

				n->m_next = NULL;                   
				mm->m_next = m;        
				m = mm;
				m->m_off = MSIZE - hlen - adj;
				m->m_len = hlen + adj;

				/* copy old header to new */

				bcopy(p, (caddr_t)((int)m + m->m_off), hlen);

				/* copy leftover data from previous frag */

				if (adj) {
					n->m_len -= adj;
					bcopy((caddr_t)((int)n+n->m_len+n->m_off),
					      (caddr_t)((int)m+m->m_off+hlen), 
							adj);
				}
			}

                        /* finish ip leader by calculating checksum and doing
                           necessary byte-swapping and send to local net */
                        
			if (!asis) {
				p->ip_sum = 0;
				olen = p->ip_len;
				ip_to_net(p);
#ifndef mbb
				p->ip_sum = cksum(m, hlen);     
#else
				j = cksum(m, hlen);
				p->ip_sum = short_to_net(j);
#endif mbb
			}
			(*ip->if_send)(m, ip, ap, olen, IPLINK, asis);

			p = (struct ip *)((int)m + m->m_off);   /* ->new hdr */
			len -= rnd;
			off += rnd;
		}
	} 

	/* complete header, byte swap, and send to local net */

	if (!asis) {
		p->ip_sum = 0;
		olen = p->ip_len;
		ip_to_net(p);
#ifndef mbb
		p->ip_sum = cksum(m, hlen);     
#else
		j = cksum(m, hlen);
		p->ip_sum = short_to_net(j);
#endif mbb
	}
	return((*ip->if_send)(m, ip, ap, olen, IPLINK, asis));     
}

/*
 * Find a route to this destination.  Given the source and destination 
 * addresses, it returns a local net address
 * to send to (either the address of the destination itself or a gateway).
 */
struct socket ip_route(src, dst)
struct socket *src;
struct socket *dst;
{
	register struct if_local *lp;
	register struct gway *gp;
	struct socket nullsock;
	net_t snet, dnet;	

	nullsock.s_addr = 0;

	/* get network parts of src and dest addresses */

	snet = iptonet(*src);
	dnet = iptonet(*dst);	

	/* first, look for dest net in local nets table */

	for (lp = locnet; lp < &locnet[nlocnet]; lp++) 
		if (dnet == lp->if_ifcb->if_net && snet == dnet &&
		    lp->if_ifcb->if_avail) 
			return(*dst);

	/* no local access to dest net, search gateway table */

	for (gp = gateway; gp < gatewayNGATE; gp++)
		if (snet == gp->g_lnet && dnet == gp->g_fnet && 
		    gp->g_ifcb->if_avail) 
			return(gp->g_local);

	/* no direct gateway access, pick a smart gateway */

	for (gp = gateway; gp < gatewayNGATE; gp++)
		if (gp->g_flags & GWSMART && gp->g_ifcb->if_avail) 
			return(gp->g_local);

	/* can't get theah from heah */

	return (nullsock);
}

/*
 * Raw IP output routine.  Called from raw_write with the user's message
 * chained to an empty mbuf for header construction.  There are three
 * possibilities:  1) header is constructed by ip_output in the empty mbuf,
 * 2) header is supplied by the user, but the checksum is generated in
 * ip_send, or 3) header is supplied by the user including checksum.  In
 * cases (2) and (3), the user supplied header is copied into the first
 * mbuf which accomodates the local net header as well.
 */
ip_raw(up, m, len)
register struct ucb *up;
register struct mbuf *m;
int len;
{
	register struct ip *p, *q;
	register struct ifcb *ip;
	register struct mbuf *n;
	int hlen;
	net_t net;
	struct socket route;
	struct socket ip_route();

	switch (up->uc_flags & RAWFLAG) {

	case RAWCOMP:			/* compose header */

		/* build header in bottom of first mbuf */

		m->m_off = MSIZE - sizeof(struct ip);
		m->m_len = sizeof(struct ip);
		p = (struct ip *)((int)m + m->m_off);

		/* insert source and destination addrs from ucb */

		p->ip_src = up->uc_srcif->if_addr;
		p->ip_dst = up->uc_host;

		/* call ip routine to compose rest of header */

		if (!ip_output(up, m, up->uc_lo, len, 0, NULL))
			u.u_error = ERAWBAD;
		break;
	
	case RAWASIS:			/* send header asis */
	case RAWVER:			/* send asis but insert checksum */

		/* copy header into first mbuf */

		n = m->m_next;
		p = (struct ip *)((int)n + n->m_off);
		hlen = p->ip_hl << 2;
		if (hlen > MSIZE) 
			goto bad;
		m->m_off = MSIZE - hlen;
		m->m_len = hlen;
		q = (struct ip *)((int)m + m->m_off);
		bcopy((caddr_t)p, (caddr_t)q, hlen);
		n->m_off += hlen;
		n->m_len -= hlen;

		/* find an i/f to send this out on */

		route = ip_route(&q->ip_src, &q->ip_dst);
		if (route.s_addr == NULL)
			goto bad;
		net = iptonet(route);
		for (ip=ifcb; ip < &ifcb[nifcb]; ip++)
			if (ip->if_net == net)
				break;
		if (ip == &ifcb[nifcb])
			goto bad;
		if (!ip_send(q, ip, &route, hlen, up->uc_flags&RAWASIS))
			u.u_error = ERAWBAD;
		break;
	}
	return;

bad:
	m_freem(m);
	u.u_error = ERAWBAD;
}

/*
 * Take the non-byte data in the ip header as it is stored internally, and
 * convert it to the form that goes out over the network (i.e., either byte
 * swap or add holes).
 */
ip_to_net(p)
register struct ip *p;
{
	p->ip_len = short_to_net(p->ip_len);
	p->ip_id = short_to_net(p->ip_id);
	p->ip_off = short_to_net(p->ip_off);
}
