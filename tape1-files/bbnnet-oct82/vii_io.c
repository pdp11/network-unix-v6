#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/imp.h"
#include "../bbnnet/raw.h"
#include "../bbnnet/ucb.h"
#include "../bbnnet/vii.h"

/*
 * V2LNI input routine.  Takes messages off the driver input queue, corrects
 * the length, and passes to IP level.
 *
 */
vii_rcv(ip)
register struct ifcb *ip;
{
	register struct mbuf *m, *n;
	register struct vii *v;
	register struct proto *q;
	register i;
	struct socket naddr;

	/* process all messages on input queue */

	for (;;) {
		spl4();
		if ((m = ip->if_inq_hd) == NULL) {
			spl0();
			break;
		}
		ip->if_inq_hd = m->m_act;
		spl0();
		m->m_act = NULL;
		v = mtod(m, struct vii *);

		/* verify our address */

		if (v->v_dst != ip->if_addr.s_vhst) {
			ip->if_addr.s_vhst = v->v_dst;
			printf("%s%d: address mismatch, reset to %d\n",
				ip->if_name, ip->if_unit, ip->if_addr.s_vhst);
		}

		if (v->v_pr == ip->if_link) {
			m->m_off += sizeof(struct vii);
			m->m_len -= sizeof(struct vii);

#ifdef	IPADJ
			/* adjust message length for actual length (if odd) */

			ip_adj(m, (struct ip *)((int)v+sizeof(struct vii)));
#endif	IPADJ

			/* if any user IPs, copy message */

			if ((q = netcb.n_ip_proto) != NULL)
				n = m_copy(m);
			ip_input(m, ip);	

			/* dispatch to other IPs */

			if (q != NULL) {
				n->m_off -= sizeof(struct vii);
				n->m_len += sizeof(struct vii);
				raw_disp(n, q);
			}
		} else {
			/* dispatch non-ip messages to raw connections */

			naddr.s_addr = ip->if_addr.s_addr;
			naddr.s_vhst = v->v_src;
			raw_input(m, &naddr, &ip->if_addr, (short)v->v_pr,
				  URAW, FALSE);
		}
	}
}

/*
 * V2LNI output routine.  Take one internet address byte for ring address and
 * set up the internal header length field.  Queue for output.
 */
vii_snd(m, ip, addr, len, link, asis)
register struct mbuf *m;
register struct ifcb *ip;
struct socket *addr;
register len;
int link, asis;
{
	register struct vii *v;

	if (!ip->if_avail) {
		m_freem(m);
		return(FALSE);
	}

	if (asis)
		v = mtod(m, struct vii *);
	else {
		m->m_off -= sizeof(struct vii);
		m->m_len += sizeof(struct vii);
		v = mtod(m, struct vii *);
		v->v_src = ip->if_addr.s_vhst;
		v->v_dst = addr->s_vhst;
		v->v_len = len + sizeof(struct vii);
		v->v_pr = link;
		v->v_ver = VIIVERSION;
	}

	m->m_act = NULL;
	spl4();
	if (ip->if_outq_hd != NULL)
		ip->if_outq_tl->m_act = m;
	else
		ip->if_outq_hd = m;
	ip->if_outq_tl = m;
	spl0();

	/* if no outstanding output, start some */

	if (!ip->if_active)
		(*ip->if_out)(ip);
	return(TRUE);
}

/*
 * V2LNI raw output.
 */
vii_raw(up, m, len)
register struct ucb *up;
register struct mbuf *m;
int len;
{
	register struct proto *p;

	switch (up->uc_flags & RAWMASK) {
	
	case RAWCOMP:			/* system supplies leader */
		/* construct header at end of first mbuf */

		m->m_off = MSIZE;
		m->m_len = 0;
		if ((p = up->uc_proto) == NULL) {
			u.u_error = ENETRNG;
			m_freem(m);
		} else if (!vii_snd(m, up->uc_srcif, &up->uc_host, len,
				     p->pr_num, FALSE));
			u.u_error = EBLOCK;
		break;

	case RAWASIS:			/* user supplies leader */
		if ((m = m_free(m)) == NULL)
			u.u_error = ERAWBAD;
		else if (!vii_snd(m, up->uc_srcif, NULL, 0, 0, TRUE))
			u.u_error = EBLOCK;
		break;
	
	default:
		u.u_error = ENETPARM;
		m_freem(m);
	}
}
