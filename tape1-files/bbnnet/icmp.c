#include "../h/param.h"
#include "../h/systm.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/icmp.h"
#include "../bbnnet/ucb.h"

extern int nosum;

#define NICTYPE	17

/* ICMP message formats */
#define ICBAD	0	/* unimplemented */
#define ICERR	1	/* error format (use header) */
#define ICDAT	2	/* data format (use id) */
#define ICINT	3	/* data format (handle internally) */

char icaction[NICTYPE] = { ICDAT, ICBAD, ICBAD, ICERR, ICERR, ICERR, ICBAD,
			   ICBAD, ICINT, ICBAD, ICBAD, ICERR, ICERR, ICINT,
			   ICDAT, ICINT, ICDAT };

/*
 * Process ICMP messages.  Called directly from ip_input processor.
 */
icmp(mp, ifp)
register struct mbuf *mp;
struct ifcb *ifp;
{
	register struct ip *ip;
	register struct icmp *icp;
	register struct ucb *up;
	unsigned short i;
	int op, ilen;
	struct socket temp;
	struct ucb *icfind();

	ip = (struct ip *)((int)mp + mp->m_off);
	mp->m_off += sizeof(struct ip);
	mp->m_len -= sizeof(struct ip);
	icp = (struct icmp *)((int)ip + sizeof(struct ip));

	ilen = ip->ip_len;

	i = (unsigned short)icp->ic_sum;
	icp->ic_sum = 0;
	if (i != (unsigned short)cksum(mp, ilen)) {
		netstat.ic_badsum++;
		if (nosum)
			goto ignore;
		netlog(mp);
	} else {
ignore:
		/* decode message format, and associate msg with connection */

		if (icp->ic_type < NICTYPE)
			op = icaction[icp->ic_type];
		else
			op = ICBAD;

		switch (op) {

		case ICERR:	/* error format -- get conn from orig hdr */
			
			up = icfind(&icp->ic_data.hdr);
			break;

		case ICDAT:	/* data format -- get conn from id */

			/* Assume we sent the orig packet with id: id
			   is index in connection table (plus one).  Verify 
			   by bounds checking. */

			up = NULL;
			if ((i = icp->ic_misc.ic_iseq.id) != 0) {
				up = contab + (i-1);
				if (up >= conNCON || up->uc_proc == NULL)
					up = NULL;
			}
			break;

		case ICINT:	/* internal format -- don't need conn */

			break;

		default:	/* drop bad messages */
			netstat.ic_drops++;
			goto badret;
		}

		/* Now do any processing.  Internal messages get handled
		   here.  Others may be counted, or cause some other action */

		switch (icp->ic_type) {

		case 3:		/* destination unreachable */
			
			if (up != NULL)
				to_user(up, UNRCH);
			break;

		case 4:		/* source quench */

			netstat.ic_quenches++;
			break;
		
		case 5:		/* redirect */

			if (up != NULL) {

				/* free the old route and insert the new */

				if (up->uc_route != NULL)
					h_free(up->uc_route);
				up->uc_route = h_make(&icp->ic_misc.ic_gaddr);
				netstat.ic_redirects++;
			}
			break;

		case 8:		/* echo */

			/* reply to echo: swap src and dest addrs, change
			   type to zero, recompute checksum */

			icp->ic_type = 0;
			temp = ip->ip_src;
			ip->ip_src = ip->ip_dst;
			ip->ip_dst = temp;
			icp->ic_sum = cksum(mp, ilen);
			mp->m_off -= sizeof(struct ip);
			mp->m_len += sizeof(struct ip);
			icsend(mp, ifp, ip, ilen);
			netstat.ic_echoes++;
			return;

		case 11:		/* time exceeded */
			netstat.ic_timex++;
			break;

		case 13:		/* timestamp */

			/* reply by changing type, swapping addrs, and
			   adding r/t timestamps.  Timestamps are not UT
			   so high order bit is set. */

			if (icp->ic_code == 0) {
				icp->ic_type = 14;
				icp->ic_data.ic_time.trecv = 0x80000000 | 
							     (long)time;
				icp->ic_data.ic_time.ttrans = 0x80000000 | 
							      (long)time;
				temp = ip->ip_src;
				ip->ip_src = ip->ip_dst;
				ip->ip_dst = temp;
				icp->ic_sum = cksum(mp, ilen);
				mp->m_off -= sizeof(struct ip);
				mp->m_len += sizeof(struct ip);
				icsend(mp, ifp, ip, ilen);
			}
			return;

		case 15:		/* info request */

			/* reply by changing type, swapping addrs, and
			   filling in net part of addr */

			if (icp->ic_code == 0) {
				icp->ic_type = 16;
				temp = ip->ip_src;
				ip->ip_src.s_addr = ip->ip_dst.s_addr | 
						    (long)ifp->if_net;
				ip->ip_dst.s_addr = temp.s_addr | 
						    (long)ifp->if_net;
				icp->ic_sum = cksum(mp, ilen);
				mp->m_off -= sizeof(struct ip);
				mp->m_len += sizeof(struct ip);
				icsend(mp, ifp, ip, ilen);
			}
			return;
		}

		/* Dispatch messages to raw connections, if wanted */

		if (up != NULL && up->uc_flags&(UIP+URAW) &&
		    ((op == ICERR && up->uc_flags&RAWERR) || op == ICDAT)) {
			mp->m_off -= sizeof(struct ip);
			mp->m_len += sizeof(struct ip);
			raw_queue(mp, up);
		 } else
badret:
			netlog(mp);
	}
}

/*
 * Process GGP messages.
 */
ggp(mp, ifp)
register struct mbuf *mp;
struct ifcb *ifp;
{
	register struct ip *ip;
	register struct ggp *gp;
	register struct ucb *up;
	unsigned short i;
	int ilen;
	struct socket temp;
	struct ucb *icfind();

	ip = (struct ip *)((int)mp + mp->m_off);
	gp = (struct ggp *)((int)ip + sizeof(struct ip));

	ilen = ip->ip_len;
	
	switch (gp->gg_type) {

	case 3:		/* destination unreachable */
			
		if ((up = icfind(&gp->gg_data.gg_hdr)) != NULL)
			to_user(up, UNRCH);
		break;

	case 4:		/* source quench */

		up = icfind(&gp->gg_data.gg_hdr);
		netstat.ic_quenches++;
		break;
		
	case 5:		/* redirect */

		if ((up = icfind(&gp->gg_data.gg_redir.rhdr)) != NULL) {

		/* free the old route and insert the new */

			if (up->uc_route != NULL)
				h_free(up->uc_route);
			up->uc_route = h_make(&gp->gg_data.gg_redir.raddr);
			netstat.ic_redirects++;
		}
		break;

	case 8:		/* echo */

		/* reply to echo: swap src and dest addrs, change
		   type to zero, recompute checksum */

		gp->gg_type = 0;
		temp = ip->ip_src;
		ip->ip_src = ip->ip_dst;
		ip->ip_dst = temp;
		icsend(mp, ifp, ip, ilen);
		netstat.ic_echoes++;
		return;

	default:
		netstat.ic_drops++;
	}

	/* Dispatch messages to raw connections, if wanted */

	if (up != NULL && up->uc_flags&(UIP+URAW) &&
	    ((i == ICERR && up->uc_flags&RAWERR) || i == ICDAT)) 
		raw_queue(mp, up);
	else
		netlog(mp);
}

/*
 * Given pertinent fields from the original datagram header portion of
 * a GGP or ICMP packet, find the corresponding connection that originated
 * it (if any).
 */
struct ucb *icfind(hdr)
register struct th *hdr;
{
	register struct ucb *up;
	register struct tcb *tp;
	unsigned short src, dst;

	src = short_from_net(hdr->t_src);
	dst = short_from_net(hdr->t_dst);

	/* match ip/tcp header fields with open connection */

	for (up=contab; up < conNCON; up++) {

		if (up->uc_proc == NULL)
			continue;

		if (up->uc_flags & UTCP) {	/* tcp connection */
			
			if ((tp = up->uc_tcb) == NULL)
				continue;

			if (tp->t_lport == src &&
			    tp->t_fport == dst &&
			    up->uc_host.s_addr == hdr->t_d.s_addr &&
			    up->uc_srcif->if_addr.s_addr == hdr->t_s.s_addr &&
			    hdr->t_pr == TCPROTO)
				return(up);

		} else if (up->uc_flags & UIP) {	/* ip connection */

			if ((up->uc_host.s_addr == hdr->t_d.s_addr ||
			     up->uc_host.s_addr == NULL) &&
			    (up->uc_flags & RAWRALL ||
			     up->uc_srcif->if_addr.s_addr == hdr->t_s.s_addr) &&
			    hdr->t_pr >= up->uc_lo &&
			    hdr->t_pr <= up->uc_hi)
				return(up);

		} else if (up->uc_flags & URAW) {	/* raw connection */

			if ((up->uc_host.s_addr == hdr->t_d.s_addr ||
			     up->uc_host.s_addr == NULL) &&
			    (up->uc_flags & RAWRALL ||
			     up->uc_srcif->if_addr.s_addr == hdr->t_s.s_addr) &&
			    up->uc_lo <= IPLINK && up->uc_hi >= IPLINK)
				return(up);
		}
	}

	netstat.ic_drops++;
	return(NULL);	/* not found */
}

/*
 * Turn around an ICMP packet.
 */
icsend(mp, ifp, ip, ilen)
register struct mbuf *mp;
register struct ifcb *ifp;
register struct ip *ip;
register ilen;
{
	register hlen;
	struct socket temp;
	struct socket ip_route();

	/* fix ip header and recompute ip checksum */

	hlen = ip->ip_hl << 2;
	ilen += hlen;
	ip->ip_len = short_to_net(ilen);
	ip->ip_off = 0;
	ip->ip_id = short_to_net(ip->ip_id);
	ip->ip_sum = cksum(mp, hlen);

	/* pass packet to local net driver */

	temp = ip_route(&ip->ip_src, &ip->ip_dst);
	(*ifp->if_send)(mp, ifp, &temp, ilen, IPLINK, TRUE);
}
