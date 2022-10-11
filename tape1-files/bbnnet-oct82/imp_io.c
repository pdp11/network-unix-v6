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
#include "../bbnnet/fsm.h"

extern struct user u;

/*
 * ARPANET 1822 input routine
 *
 * Called from netmain to interpret 1822 imp-host messages.  Completed messages
 * are placed on the input queue by the driver.  Type 0 messages (non-control)
 * are passed to higher level protocol processors on the basis of link
 * number.  All other type messages (control) are handled here.
 */
imp_rcv(ip)
register struct ifcb *ip;
{
	register struct mbuf *m, *n;
	register struct imp *p;
	register struct proto *q;
	register struct host *hp;
	register struct ucb *up;
	int i;
	int error = FALSE;
	struct socket naddr;
	int netreset();

	/* process all messages on input queue */

	for (;;) {
		spl5();
		if ((m = ip->if_inq_hd) == NULL) {     
			spl0();
			break;
		}
		ip->if_inq_hd = m->m_act;
		spl0();
		m->m_act = NULL;
		p = mtod(m, struct imp *);               

		if (p->i_nff != IMP_NFF)    /* make sure leader is ok */
			goto free;

		switch (p->i_type) {            /* case on msg type */

		case IMP_DATA:			/* normal msg */

			p->i_mlen = short_from_net(p->i_mlen) >> 3;

			/* dispatch on link number, pass to either
			  ip or raw input handler */

			if (p->i_link == ip->if_link) {
        			m->m_off += sizeof(struct imp);
        			m->m_len -= sizeof(struct imp);

#ifdef	IPADJ
				/* remove any IMP message padding */

				ip_adj(m, (struct ip *)((int)p + 
							sizeof(struct imp)));
#endif	IPADJ

				/* if any user IPs, copy message */

				if ((q = netcb.n_ip_proto) != NULL)
					n = m_copy(m);
				ip_input(m, ip);	

				/* dispatch to other IPs */

				if (q != NULL) {
					n->m_off -= sizeof(struct imp);
					n->m_len += sizeof(struct imp);
					raw_disp(n, q);
				}
			} else 
				goto dispraw;
			break;

		 case IMP_LERROR:		/* imp leader error */
			log_err(ip, "leader error");
			h_reset(iptonet(ip->if_addr));
			imp_noops(ip);		 /* send no-ops to clear i/f */
			goto free;

		 case IMP_DOWN:			/* imp going down msg */
			log_err(ip, "going down");
			ip->if_avail = FALSE;
			timeout((caddr_t)netreset, (caddr_t)ip, 30*60);
			goto free;

		case IMP_NOP:			/* no-op */
			/* compare local addr with that in message,
			   warn and reset if no match */
			
			if (p->i_host != ip->if_addr.s_host ||
			     p->i_imp != ip->if_addr.s_imp) {
				ip->if_addr.s_host = p->i_host;
				ip->if_addr.s_imp = p->i_imp;
				printf("%s%d imp: address mismatch, reset to %d/%d\n",
					ip->if_name, ip->if_unit, 
					p->i_host, p->i_impno);
			}
			goto free;

		case IMP_RFNM:			/* RFNM */
		case IMP_INCOMP:		/* incomplete */
			/* record in host table and free */

			p->i_htype = netoint(iptonet(ip->if_addr));
			if ((hp = h_find(&p->i_htype)) != NULL) 
 				if (hp->h_rfnm != 0) {
					hp->h_rfnm--;

					/* send any blocked msgs */

					if ((n = hp->h_outq) != NULL) {
						hp->h_outq = n->m_act;
						imp_snd(n, ip, NULL,0,0,TRUE);

					/* delete any temp host entries if
					   RFNM count is zero */

					} else if (hp->h_rfnm == 0 && 
						   hp->h_stat&HTEMP)
						h_free(hp);
						
				}
			goto disperr;

		case IMP_DEAD:			/* unreachable host */
			/* set status, record in host tab and free */

			p->i_htype = netoint(iptonet(ip->if_addr));
			if ((hp = h_find(&p->i_htype)) != NULL) {

				hp->h_stat &= ~HUP;
				hp->h_rfnm = 0;

				/* signal all conns to this host */

				for (up=netcb.n_ucb_hd; up != NULL; 
							      up = up->uc_next)

					if (up->uc_proc != NULL && 
					    up->uc_route == hp) 

						/* if route addr different from 
						   dest addr, must be dead 
						   gateway */

						if (hp->h_addr.s_addr ==
						    addr_1822(up->uc_host)) {
							up->uc_xstat = p->i_stype;
							to_user(up, 
							        (p->i_stype == 
								 IMP_DEAD_HOST 
								    ? UDEAD
								    : UNRCH));

						/* look for another gateway to
						   continue with */

						} else if (!ip_reroute(up, hp)){
							up->uc_xstat = 0;
							to_user(up, UNRCH);
						}
			}

			goto disperr;

		case IMP_DERROR:			/* imp data error */
			/* clear RFNM status for this host and send no-ops
			   to clear i/f */

			log_err(ip, "data error");
			p->i_htype = netoint(iptonet(ip->if_addr));
			if ((hp = h_find(&p->i_htype)) != NULL) 
				hp->h_rfnm = 0;
			imp_noops(ip);
			goto disperr;

		case IMP_RESET:				/* reset */
			log_err(ip, "reset complete");
			goto free;

		default:				/* illegal type */
			goto free;

		disperr:		
			/* dispatch control messages to raw connections */

			if (p->i_link == ip->if_link && 
			    netcb.n_ip_proto == NULL)
				goto free;
			error = TRUE;
		dispraw:
			/* dipatch control and data messages to raw conns */

			naddr.s_addr = ip->if_addr.s_addr;
			naddr.s_imp = p->i_imp;
			naddr.s_host = p->i_host;
			raw_input(m, &naddr, &ip->if_addr, 
				  (short)p->i_link, URAW, error);    
			break;
		free:
			m_freem(m);     /* free message chain */
		}
	}
}

/*
 * ARPANET 1822 output routine
 * 
 * Called from higher level protocol routines to set up messages for
 * transmission to the imp.  Sets up header for non-asis messages.
 * Enques a message passed as an mbuf chain from the higher level protocol
 * routines.  Does RFNM counting: if more than 8 messages are in flight,
 * hold on a RFNM queue until the i/f clears.
 */
imp_snd(mp, ip, addr, len, link, asis)
register struct mbuf *mp;
register struct ifcb *ip;
struct socket *addr;
register len;
int link, asis;
{
	register struct mbuf *m;
	register struct imp *p;
	register struct host *hp;
	int i;
	struct socket ohost;

	if (!ip->if_avail) {			 /* make sure i/f is up */
		m_freem(mp);
		return(FALSE);
	}

	mp->m_act = NULL;
	if (asis) 
		p = mtod(mp, struct imp *);        /* -> 1822 leader */
	else {
		/* build header in current mbuf */

		mp->m_off -= sizeof(struct imp);
		mp->m_len += sizeof(struct imp);
		p = mtod(mp, struct imp *);
		 
		p->i_host = addr->s_host;
		p->i_impno = addr->s_impno;
		p->i_lh = 0;		/* do not send logical host field */
		p->i_mlen = len + sizeof(struct imp);
		p->i_link = link;

		p->i_nff = IMP_NFF;
		p->i_dnet = 0;
		p->i_lflgs = 0;
		p->i_type = IMP_DATA;
		p->i_stype = 0;
		p->i_htype = 0;
	}

#ifndef IMPLOOP

	/* do RFNM counting for type 0 messages (no more than 8 outstanding
	   to any host)  */ 

	if (p->i_type == 0) {
                ohost.s_addr = ip->if_addr.s_addr;
                ohost.s_host = p->i_host;
                ohost.s_imp = p->i_imp;
        	hp = h_find(&ohost);            /* get host table entry */

		/* if imp would block, put msg on queue until rfnm */

		if (hp != NULL) {
			if (hp->h_rfnm > 8) {
				if ((m = hp->h_outq) == NULL)
					hp->h_outq = mp;
				else {

					/* don't pile up too many messages */

					i = 8;
					while (m->m_act != NULL) {
						if (--i > 0) 
							m = m->m_act;
						else {
							m_freem(mp);
							return(FALSE);
						}
					}
					m->m_act = mp;
				}
				return(TRUE);
			} else
				hp->h_rfnm++;
		} else {

		/* make a temporary host entry if none exists already */

			if ((hp = h_make(&ohost, TRUE)) != NULL) 
				hp->h_rfnm++;
		}
	}

	/* put output message on queue */

        spl5();
	if (ip->if_outq_hd != NULL)
		ip->if_outq_tl->m_act = mp;
	else
		ip->if_outq_hd = mp;
	ip->if_outq_tl = mp;
	spl0();

	/* if no outstanding output, start some */

	if (!ip->if_active)
		(*ip->if_out)(ip);

#else
	/* software looping: put msg chain on input queue */

	if (ip->if_inq_hd != NULL)
		ip->if_inq_tl->m_act = mp;
	else
		ip->if_inq_hd = mp;
	ip->if_inq_tl = mp;

#endif IMPLOOP

	return(TRUE);
}

/*
 * Raw ARPANET 1822 output routine.  Called from raw_write with an empty
 * mbuf chained to the user supplied message chain for header construction.
 */
imp_raw(up, m, len)
register struct ucb *up;
register struct mbuf *m;
int len;
{
	register struct proto *p;

	switch (up->uc_flags & RAWMASK) {

	case RAWCOMP:		/* system supplies leader */

		/* leader will be constructed at end of first mbuf */

		m->m_off = MSIZE;
		m->m_len = 0;
		if ((p = up->uc_proto) == NULL) {
			u.u_error = ENETRNG;
			m_freem(m);
		} else if (!imp_snd(m, up->uc_srcif, &up->uc_host, len, 
				     p->pr_num, FALSE)) 
			u.u_error = EBLOCK;
		break;

	case RAWASIS:		/* user supplies leader */

		/* ->user supplied header */

		if ((m = m_free(m)) == NULL)
			u.u_error = ERAWBAD;	
		else if (!imp_snd(m, up->uc_srcif, NULL, 0, 0, TRUE))
			u.u_error = EBLOCK;
		break;	

	default:
		u.u_error = ENETPARM;
		m_freem(m);
	}
}

/*
 * Put three 1822 no-ops at the head of the output queue.  Part of host-imp
 * initialization procedure.
 */
imp_noops(ip)             
register struct ifcb *ip;
{
	register inx;
	register struct mbuf *m;
	register struct imp *l;
	int s;

	for (inx = 0; inx < 3; inx++ ) { 

		/* get a buffer for the no-op */

		if ((m = m_get(0)) == NULL) 
			return(FALSE);
		m->m_off = MHEAD;
		m->m_len = 10;
		l = mtod(m, struct imp *);

		l->i_nff = IMP_NFF;
		l->i_dnet = 0;
		l->i_lflgs = 0;
                l->i_htype = 0;
                l->i_host = 0;
                l->i_imp = 0;
                l->i_link = inx;
                l->i_stype = 0;
                l->i_mlen = 0;
                l->i_type = IMP_NOP;        

		/* put no-op at head of output queue */

		s = spl5();
		if (ip->if_outq_hd == NULL)
			ip->if_outq_tl = m;
		else
			m->m_act = ip->if_outq_hd;
		ip->if_outq_hd = m;
		splx(s);	
	}

	if (!ip->if_active)
		(*ip->if_out)(ip);
	return(TRUE);
}

log_err(ip, s)
register struct ifcb *ip;
char *s;
{
	printf("%s%d imp: %s\n", ip->if_name, ip->if_unit, s);
}
