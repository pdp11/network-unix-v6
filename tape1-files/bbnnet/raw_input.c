#include "../h/param.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/ucb.h"

/*
 * Put raw message input on user receive queue.  Called from ip_input or local
 * net input routine.  Dispatches on link/protocol number and source and
 * destination address.
 */
raw_input(mp, ip, src, dst, link, mask, error)               
register struct mbuf *mp;
struct ifcb *ip;
register struct socket *src;
register struct socket *dst;
register link;
int mask, error;
{
	register struct ucb *up;

	/* look for protocol or link number in connection table */

	for (up = contab; up < conNCON; up++) 

		if (up->uc_proc != NULL && (up->uc_flags & mask) && 
		      link >= up->uc_lo && link <= up->uc_hi &&
		      (up->uc_host.s_addr == NULL || 
			src->s_addr == up->uc_host.s_addr) &&
		      (up->uc_flags&RAWRALL ||
		        (dst == NULL && ip == up->uc_srcif->if_ifcb) ||
			(dst != NULL &&
			 dst->s_addr == up->uc_srcif->if_addr.s_addr))) { 

			/* drop if error message and not looking for errors */

			if (error && !(up->uc_flags&RAWERR))
				break;

			raw_queue(mp, up);
			return;
		}

	netstat.imp_drops++;
	m_freem(mp);
}

raw_queue(mp, up)
register struct mbuf *mp;
register struct ucb *up;
{
	register count;
	register struct mbuf *m;

	/* drop if no buffer space availble for conn */
	/* have buffer space, chain to user rcv buf */

	for (m = mp, count = 0; m != NULL; m = m->m_next)
		count++;
	if (count + up->uc_rsize <= up->uc_rcv) {

		mp->m_act = NULL;

		while (up->uc_flags & ULOCK)
			sleep(&up->uc_flags, PZERO);
		up->uc_flags |= ULOCK;

		up->uc_rsize += count;
		if ((m = up->uc_rbuf) == NULL)
			up->uc_rbuf = mp;
		else {
			while (m->m_act != NULL) 
				m = m->m_act;
			m->m_act = mp;
		}
		up->uc_flags &= ~ULOCK;
		wakeup(&up->uc_flags);

		/* wake up raw read */

		wakeup(up);
	} else {
		to_user(up, UDROP);
		m_freem(mp);
	}
}

/*
 * Verify raw link or protocol number.
 */
raw_range(p, mask, fp, lo, hi)         
register struct ucb *p;
int mask, lo, hi;
register struct socket *fp;
{
	register struct ucb *up;

	/* make sure link or proto is valid */

	if (lo < 0 || lo > 255 || hi < 0 || hi > 255 || hi < lo || 
		(mask == UIP && lo <= TCPROTO && hi >= TCPROTO) ||
		(mask == URAW && lo <= IPLINK && hi >= IPLINK))
		return(FALSE);

	/* check to see if anyone else is using link or proto slot */

	for (up = contab; up < conNCON; up++)
		if (up->uc_proc != NULL && (up->uc_flags & mask) && 
		       ((up->uc_lo <= lo && up->uc_hi >= lo) ||
			(up->uc_lo <= hi && up->uc_hi >= hi) ||
			(up->uc_lo >= lo && up->uc_hi <= hi)) &&
			(up->uc_host.s_addr == NULL || 
			 p->uc_host.s_addr == NULL || 
			 up->uc_host.s_addr == fp->s_addr) &&
			(up->uc_flags&RAWRALL || p->uc_flags&RAWRALL ||
			  up->uc_srcif->if_addr.s_addr == 
				p->uc_srcif->if_addr.s_addr)) 
			return(FALSE);

	return(TRUE);
}
