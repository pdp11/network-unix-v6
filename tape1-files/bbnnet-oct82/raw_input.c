#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/icmp.h"
#include "../bbnnet/raw.h"
#include "../bbnnet/udp.h"
#include "../bbnnet/ucb.h"

extern struct user u;

/*
 * Put raw message input on user receive queue.  Called from ip_input or local
 * net input routine.  Dispatches on link/protocol number and source and
 * destination address.
 */
raw_input(mp, src, dst, link, mask, error)               
register struct mbuf *mp;
struct socket *src;
struct socket *dst;
short link;
int mask, error;
{
	register struct proto *p;
	register struct mbuf *m;
	register struct ucb *up;
	int copied, queued;

	copied = TRUE;
	queued = FALSE;

	/* get proto dispatch queue header */

	p = (struct proto *)&protab[PRHASH(link)];

	while ((p = p->pr_next) != NULL) {
		m = dtom(p);
		up = (struct ucb *)m->m_act;

		/* dispatch on link number, and src/dst address */

		if (p->pr_num == link && up != NULL && 
		      (up->uc_flags & mask) && 
		      (up->uc_host.s_addr == NULL || 
			src->s_addr == up->uc_host.s_addr) &&
		      (up->uc_local.s_addr == NULL ||
			 dst->s_addr == up->uc_local.s_addr) &&
		      (!error || (error && up->uc_flags&RAWERR))) {

			/* if conns for this proto remain, copy the message,
			   otherwise just put it on ucb read queue */

			copied = FALSE;
			if (p->pr_next != NULL) 
				if ((m = m_copy(mp)) != NULL)
					copied = TRUE;

			/* if last copy was successful, enqueue the
			   message, otherwise tell user something
			   was dropped */

			if (mp != NULL) 
				queued = raw_queue(mp, up);
			else
				to_user(up, UDROP);
			mp = m;
		} 
	}

	if (copied)		/* drop anything left over */
		m_freem(mp);
	if (queued)
		netstat.net_drops++;
}

/*
 * Dispatch message to other IP/TCP/ICMP/GGP modules.  For each requestor,
 * copy raw message and place it on user input queue.
 */
raw_disp(m, p)
register struct mbuf *m;
register struct proto *p;
{
	register struct mbuf *n, *mm;
	register struct ucb *up;

	while (p != NULL)  {
		mm = dtom(p);
		if ((up = (struct ucb *)mm->m_act) == NULL)
			return;
		p = p->pr_next;

		/* no mbufs for copy, just tell user;
		   otherwise copy and dispatch */

		if (m == NULL) 
			to_user(up, UDROP);
		else {
			if (p != NULL)
				n = m_copy(m);
			raw_queue(m, up);
			m = n;
		}
	}
}

/*
 * Put raw message chain on specified ucb's read queue.  Make sure buffer
 * limit is not exceeded.  Message will either all fit, or be dropped.
 */
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
		return(TRUE);
	} else {
		to_user(up, UDROP);
		m_freem(mp);
		return(FALSE);
	}
}

/*
 * Add protocol entry to ucb proto block, adding new blocks as needed.  Insert
 * proto entry to global proto header chain.
 */
raw_add(up, prnum)
register struct ucb *up;
short prnum;
{
	register struct mbuf *m, *n;
	register struct proto *p, *q;
	register i;

	/* first, make sure we don't already have this prnum */

	p = profind(up, prnum);
	while ((p = p->pr_next) != NULL) {
		m = dtom(p);
		if (p->pr_num == prnum && up == (struct ucb *)m->m_act)
			return;
	}

	p = up->uc_proto;
	n = m = dtom(p);
getnew:
	/* find first non-full proto block on ucb list */

	while (m != NULL && m->m_len == NPRMB) {
		n = m;
		m = m->m_next;
	}

	/* no proto block or all full */

	if (p == NULL || m == NULL) {

		/* get new proto block */

		if ((m = m_get(1)) == NULL) {
			u.u_error = ENETBUF;
			return;
		}
		m->m_next = NULL;
		m->m_act = (struct mbuf *)up;
		m->m_len = 0;

		/* mark free all entries in proto block */

		p = (struct proto *)((int)m + MHEAD);
		for (q=p,i=NPRMB; i > 0; i--,q++) {
			q->pr_next = NULL;
			q->pr_flag = 0;
		}

		/* chain new proto block to ucb */

		if (n == NULL) 
			up->uc_proto = p;
		else 
			n->m_next = m;
	}

	/* look for first free entry in proto block */

	for (i=NPRMB; i > 0; i--, p++) {
		if (p->pr_flag == 0) {
			p->pr_num = prnum;
			p->pr_flag |= PRUSED;
			m->m_len++;

			/* find proto header, and add entry to chain */

			q = profind(up, prnum);
			while (q->pr_next != NULL)
				q = q->pr_next;
			q->pr_next = p;
			return;
		}
	}

	/* just in case # of free proto entries lies, make full and retry */

	m->m_len = NPRMB;
	goto getnew;
}

/*
 *  Free all proto entries and blocks on this ucb.
 */
raw_free(up)
register struct ucb *up;
{
	register i;
	register struct proto *p;
	register struct mbuf *m;

	if ((p = up->uc_proto) == NULL) 
		return;
	up->uc_proto = NULL;

	m = dtom(p);
	while (m != NULL) {
		for (i=NPRMB; i > 0; i--,p++)
			if (p->pr_flag & PRUSED)
				prodel(up, p);
		m = m_free(m);
	}
}

/*
 * Find specified proto entry and delete from ucb proto block.
 */
raw_del(up, prnum)
register struct ucb *up;
register short prnum;
{
	register struct proto *p;
	register struct mbuf *m;

	p = profind(up, prnum);
	while ((p = p->pr_next) != NULL)
		if (p->pr_num == prnum) {
			m = dtom(p);
			if (up == (struct ucb *)m->m_act) {
				prodel(up, p);
				return;
			}
		}
	u.u_error = ENETRNG;
}


/*
 * Fill array with protocol numbers from ucb proto block
 */
raw_stat(up, protp, protlen)
struct ucb *up;
register short *protp;
register protlen;
{
	register i;
	register struct proto *p;
	register struct mbuf *m;

	p = up->uc_proto;
	m = dtom(p);
	while (m != NULL) {
		for (i = NPRMB; i > 0; i--,p++) {
			if (p->pr_flag & PRUSED) {
				if (protlen > 0) {
					if (copyout((caddr_t)&p->pr_num,
						    (caddr_t)protp,
						    sizeof(short))) {
						u.u_error = EFAULT;
						return;
					}
					protlen -= sizeof(short);
					protp++;
				} else
					return;
			}
		}
		m = m->m_next;
		p = (struct proto *)((int)m + MHEAD);
	}
}

/*
 * Delete proto entry from ucb proto block and corresponding global proto chain 
 */
prodel(up, p)
struct ucb *up;
register struct proto *p;
{
	register struct proto *q, *r;
	register short prnum;
	register struct mbuf *m;

	if (p == NULL)
		return;
	prnum = p->pr_num;
	r = profind(up, prnum);
	q = r->pr_next;

	while (q != NULL) {
		if (q->pr_num == prnum && q == p) {
			r->pr_next = p->pr_next;
			break;
		}
		r = q;
		q = q->pr_next;
	}
	
	p->pr_next = NULL;
	p->pr_flag = 0;
	m = dtom(p);
	if (m->m_len != 0)
		m->m_len--;
}

/* 
 * Find correct proto chain header.
 */
struct proto *profind(up, prnum)
register struct ucb *up;
register short prnum;
{
	register struct proto *p;

	/* check "built-in" proto headers first, then proto hash table 
	   for header */

	if (prnum == TCPROTO && up->uc_flags&UIP)
		p = (struct proto *)&netcb.n_tcp_proto;
	else if (prnum == up->uc_srcif->if_link && up->uc_flags&URAW)
		p = (struct proto *)&netcb.n_ip_proto;
	else if (prnum == ICMPROTO && up->uc_flags&UIP)
		p = (struct proto *)&netcb.n_icmp_proto;
	else if (prnum == GGPROTO && up->uc_flags&UIP)
		p = (struct proto *)&netcb.n_ggp_proto;
	else if (prnum == UDPROTO && up->uc_flags&UIP)
		p = (struct proto *)&netcb.n_udp_proto;
	else
		p = (struct proto *)&protab[PRHASH(prnum)];

	return(p);
}
