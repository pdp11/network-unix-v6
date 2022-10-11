#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/raw.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ucb.h"

/*
 *	ip_send is called from tcp and passed an mbuf chain of a packet to
 *	send to the local network.  The first mbuf contains a tcp and partial
 *	ip header which is filled in.  After determining a local route for
 *	the packet, it is fragmented (if necessary) and sent to the local net 
 *	through the local net send routine.
 */
ip_send(up, mp, proto, len, optlen, options, asis)
register struct ucb *up;
register struct mbuf *mp;
int proto, len, optlen;
char *options;
int asis;
{
	register struct ifcb *ip;
	register struct ip *p, *q;
	register struct socket *ap;
	int hlen;
	u_short j;
	struct socket *ip_route();

	p = mtod(mp, struct ip *); 		/* -> ip header */

	/* Find route for datagram, if one has not been assigned.
	   Make a host table entry for the local network route to
	   the destination */

	ip = up->uc_srcif;
	if (up->uc_route == NULL) {
		if ((ap = ip_route(&p->ip_src, &p->ip_dst)) == NULL ||
		    (up->uc_route = h_make(ap, FALSE)) == NULL) {
			m_freem(mp);
			return(FALSE);
		} 
	} else
		ap = &up->uc_route->h_addr;

	/* copy ip option string to header */

	if (optlen > 0) {
		mp->m_off -= optlen;
		mp->m_len += optlen;
		q = p;
		p = (struct ip *)((int)p - optlen);
		bcopy((caddr_t)q, (caddr_t)p, sizeof(struct ip)); 
		bcopy(options, (caddr_t)((int)p + sizeof(struct ip)), optlen);
	}
	hlen = sizeof(struct ip) + optlen;

	/* fill in ip header fields */

	if (!(asis&(RAWASIS+RAWVER))) {
		p->ip_len = len + hlen;
		p->ip_p = proto;
		p->ip_v = IPVERSION;    
		p->ip_hl = hlen >> 2;
		p->ip_off = 0;
		p->ip_ttl = MAXTTL;
		p->ip_id = netcb.n_ip_cnt++;
	}
	/*
	 * let ip_frag do the send if needed, otherwise do it directly.
	 */
	if (p->ip_len > ip->if_mtu) 
		return(ip_frag(p, ip, ap, hlen, asis));
	else {

		/* complete header, byte swap, and send to local net */

		len = p->ip_len;
		p->ip_len = short_to_net(p->ip_len);
		p->ip_off = short_to_net(p->ip_off);
		if (!(asis&RAWASIS)) {
			p->ip_sum = 0;
#ifndef mbb
			p->ip_sum = cksum(mp, hlen);     
#else
			j = cksum(mp, hlen);
			p->ip_sum = short_to_net(j);
#endif mbb
		}
		return((*ip->if_send)(mp, ip, ap, len, ip->if_link, FALSE));     
	}
}

/*
 *	ip_frag is called with a packet with a completed ip header 
 *	(except for checksum).  It fragments the
 *	packet, inserts the ip checksum, and calls the appropriate local net
 *	output routine to send it to the net.  
 */
ip_frag(p, ip, ap, hlen, asis)
register struct ip *p;
register struct ifcb *ip;
struct socket *ap;
int hlen, asis;
{	 
	register struct mbuf *m, *n;
	register i, rnd;
	struct mbuf *mm;
	int adj, max, len, off;
	u_short j;
	unsigned olen;
	
	m = dtom(p);

	if (p->ip_off & ip_df) {	/* can't fragment */
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
			m = dtom(p);
			break;
		} else {                        /* more fragments */

			/* allocate header mbuf for next fragment */

			if ((mm = m_get(1)) == NULL) {
				m_freem(m);
				return(NULL);
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
		
		n = dtom(p);
		olen = p->ip_len;
		p->ip_len = short_to_net(p->ip_len);
		p->ip_off = short_to_net(p->ip_off);
		if (!(asis&RAWASIS)) {
			p->ip_sum = 0;
#ifndef mbb
			p->ip_sum = cksum(n, hlen);     
#else
			j = cksum(n, hlen);
			p->ip_sum = short_to_net(j);
#endif mbb
		}
		(*ip->if_send)(n, ip, ap, olen, ip->if_link, FALSE);

		p = mtod(m, struct ip *);   /* ->new hdr */
		len -= rnd;
		off += rnd;
	}

	/* complete header, byte swap, and send to local net */

	olen = p->ip_len;
	p->ip_len = short_to_net(p->ip_len);
	p->ip_off = short_to_net(p->ip_off);
	if (!(asis&RAWASIS)) {
		p->ip_sum = 0;
#ifndef mbb
		p->ip_sum = cksum(m, hlen);     
#else
		j = cksum(m, hlen);
		p->ip_sum = short_to_net(j);
#endif mbb
	}
	return((*ip->if_send)(m, ip, ap, olen, ip->if_link, FALSE));     
} 

/*
 * Find a route to this destination.  Given the source and destination 
 * addresses, it returns a local net address
 * to send to (either the address of the destination itself or a gateway).
 */
struct socket *ip_route(src, dst)
struct socket *src;
struct socket *dst;
{
	register struct ifcb *ip;
	register struct gway *gp;
	net_t snet, dnet;	

	/* get network parts of src and dest addresses */

	snet = iptonet(*src);
	dnet = iptonet(*dst);	

	/* first, look for dest net in ifcb table */

	if (snet == dnet)
		for (ip = netcb.n_ifcb_hd; ip != NULL; ip = ip->if_next) 
			if (dnet == iptonet(ip->if_addr) && ip->if_avail) 
				return(dst);

	/* no local access to dest net, search gateway table */

	for (gp = gateway; gp < gatewayNGATE; gp++)
		if (snet == gp->g_lnet && dnet == gp->g_fnet && 
		    gp->g_ifcb->if_avail) 
			return(&gp->g_local);

	/* no direct gateway access, pick a smart gateway */

	for (gp = gateway; gp < gatewayNGATE; gp++)
		if (gp->g_flags & GWROUTE && snet == gp->g_lnet &&
		    gp->g_ifcb->if_avail) 
			return(&gp->g_local);

	/* can't get theah from heah */

	return (NULL);
}


/*
 * Look for a smart (routing) gateway, other than the one currently in use,
 * and use that for subsequent packet routing.  Called from lower level
 * protocol when a local gateway is known to be unreachable 
 */
ip_reroute(up, hp)
register struct ucb *up;
register struct host *hp;
{
	register struct gway *gp;
	net_t lnet;

	/* look for smart gateway */

	lnet = iptonet(hp->h_addr);
	for (gp=gateway; gp < gatewayNGATE; gp++) {
		if (gp->g_flags&GWROUTE && gp->g_ifcb->if_avail &&
		    lnet == gp->g_lnet &&
		    gp->g_local.s_addr != hp->h_addr.s_addr) {

			/* redirect */

			h_free(hp);
			if ((up->uc_route=h_make(&gp->g_local, FALSE)) == NULL)
				return(FALSE);
			else
				return(TRUE);
		}
	}
	return(FALSE);
}

/*
 * Raw IP output routine.  Called from raw_write with the user's message
 * chained to an empty mbuf for header construction.  There are three
 * possibilities:  1) header is constructed by ip_send in the empty mbuf,
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
	struct socket *ap;
	struct proto *r;
	int hlen;
	net_t net;
	struct socket *ip_route();

	switch (up->uc_flags & RAWMASK) {

	case RAWCOMP:			/* compose header */

		/* build header in bottom of first mbuf */

		m->m_off = MSIZE - sizeof(struct ip);
		m->m_len = sizeof(struct ip);
		p = mtod(m, struct ip *);

		/* insert source and destination addrs from ucb */

		p->ip_src = up->uc_local;
		p->ip_dst = up->uc_host;

		/* call ip routine to compose rest of header */

		if ((r = up->uc_proto) == NULL)
			goto rawfree;
		if (!ip_send(up, m, r->pr_num, len, 0, NULL, FALSE))
			goto rawbad;
		break;
	
	case RAWASIS:			/* send header asis */
	case RAWVER:			/* send asis but insert checksum */

		/* copy header into first mbuf */

		if ((n = m->m_next) == NULL)
			goto rawbad;
		p = mtod(n, struct ip *);
		hlen = p->ip_hl << 2;
		if (hlen > MSIZE || hlen > n->m_len) 
			goto rawfree;
		m->m_off = MSIZE - hlen;
		m->m_len = hlen;
		q = mtod(m, struct ip *);
		bcopy((caddr_t)p, (caddr_t)q, hlen);
		n->m_off += hlen;
		n->m_len -= hlen;
		/*
		 * Free any old routing entry, and make a new one for
		 * this datagram.
		 */
		if (up->uc_route != NULL)
			h_free(up->uc_route);
		if ((ap = ip_route(&p->ip_src, &p->ip_dst)) == NULL ||
		    (up->uc_route = h_make(ap, TRUE)) == NULL) 
			goto rawfree;
		/*
		 * Determine an interface to send datagram out on.
		 */
		net = iptonet(*ap);
		for (ip = netcb.n_ifcb_hd; ip != NULL; ip = ip->if_next) 
			if (iptonet(ip->if_addr) == net)
				break;
		if ((up->uc_srcif = ip) == NULL)
			goto rawfree;
		if (!ip_send(up, m, (short)q->ip_p, len, 0, NULL, up->uc_flags))
			goto rawbad;
		break;

	default:
		m_freem(m);
		u.u_error = ENETPARM;
	}
	return;
rawfree:
	m_freem(m);
rawbad:
	u.u_error = ERAWBAD;
}
