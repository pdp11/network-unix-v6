#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/imp.h"
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
	register struct host *hp;
	register struct if_local *lp;
	register struct ucb *cp;
	int error = FALSE;
	struct socket naddr;
	int netreset();

	/* process all messages on input queue */

	while ((m = ip->if_inq_hd) != NULL) {     

		ip->if_inq_hd = m->m_act;
		m->m_act = NULL;
		p = (struct imp *)((int)m + m->m_off);               

		if (p->i_nff != 0xf)    /* make sure leader is ok */
			goto free;

		switch (p->i_type) {            /* case on msg type */

		case 0:                         /* normal msg */

			p->i_mlen = short_from_net(p->i_mlen) >> 3;

			/* dispatch on link number, pass to either
			  ip or raw input handler */

			if (p->i_link == IPLINK) {
        			m->m_off += sizeof(struct imp);
        			m->m_len -= sizeof(struct imp);
				ip_input(m, ip);
			} else 
				goto dispraw;
			break;

		 case 1:                         /* imp leader error */
			log_err(ip, "leader error");
			h_reset(ip->if_net);
			imp_noops(ip);		 /* send no-ops to clear i/f */
			goto free;

		 case 2:                         /* imp going down msg */

			switch (p->i_link & IMPWHYDOWN) {
			case 0:		
				log_err(ip, "imp going down in 30 secs");
				ip->if_avail = FALSE;
				timeout((caddr_t)netreset, (caddr_t)ip, 30*60);
				goto free;
			case 1:
				log_err(ip, "imp going down for hardware PM");
				goto free;
			case 2:
				log_err(ip, "imp going down for software reload");
				goto free;
			case 3:
				log_err(ip, "imp emergency restart");
				goto free;

			}

		case 4:                         /* no-op */

			/* compare local addr with that in message,
			   warn and reset if no match */
			
			for (lp=locnet; lp < &locnet[nlocnet]; lp++) 
				if (lp->if_ifcb == ip && 
				    (p->i_host != lp->if_addr.s_host ||
				     p->i_impno != lp->if_addr.s_impno)) {
					lp->if_addr.s_host = p->i_host;
					lp->if_addr.s_imp = p->i_imp;
			log_err("warning: ifcb address does not match imp's:"); 
			printf("    reset to %d/%d\n",  p->i_host, p->i_impno);
				}
			goto free;

		case 5:                         /* RFNM */
		case 9:                         /* incomplete */
			/* record in host table and free */

			p->i_htype = netoint(ip->if_net);
			if ((hp = h_find(&p->i_htype)) != NULL) 
 				if (hp->h_rfnm != 0) {
					hp->h_rfnm--;

					/* send any blocked msgs */

					if ((n = hp->h_outq) != NULL) {
						hp->h_outq = n->m_act;
						imp_snd(n, ip, FALSE);
					}
				}
			goto disperr;

		case 6:                         /* dead host status */
			/* record in host table and free */

			p->i_htype = netoint(ip->if_net);
			if ((hp = h_find(&p->i_htype)) != NULL)
				hp->h_stat |= (p->i_stype & 0xf) << 4;
			goto free;

		case 7:                         /* unreachable host */
			/* set status, record in host tab and free */

			p->i_htype = netoint(ip->if_net);
			if ((hp = h_find(&p->i_htype)) != NULL) {

				hp->h_stat &= ~HUP;
				hp->h_rfnm = 0;

				/* signal all conns to this host */

				for (cp = contab; cp < conNCON; cp++)
					if (cp->uc_proc != NULL && 
					    cp->uc_route == hp)
						to_user(cp, UDEAD);
			}

			goto disperr;

		case 8:				/* imp data error */
			/* clear RFNM status for this host and send no-ops
			   to clear i/f */

			log_err(ip, "data error");
			p->i_htype = netoint(ip->if_net);
			if ((hp = h_find(&p->i_htype)) != NULL) 
				hp->h_rfnm = 0;
			imp_noops(ip);
			goto disperr;

		case 10:                        /* reset */
			log_err(ip, "imp reset complete");
			goto free;

		default:                        /* illegal type */
			/* record in status and free */

			netstat.imp_drops++;
			goto free;

		disperr:		
			/* dispatch control messages to raw connections */

			error = TRUE;
			if (p->i_link != IPLINK) {

		dispraw:
			/* dipatch control and data messages to raw conns */

				naddr.s_addr = (long)ip->if_net;
				naddr.s_imp = p->i_imp;
				naddr.s_host = p->i_host;
				raw_input(m, ip, &naddr, NULL, p->i_link,
					  URAW, error);    
			} else
		free:
				m_freem(m);     /* free message chain */
		}
	}

}

/*
 * ARPANET 1822 output routine
 * 
 * Called from higher level protocol routines to set up messages for
 * transmission to the imp.  Sets up the header and calls imp_snd to
 * enqueue the message for the interface driver.
 */
imp_output(m, ip, addr, len, link, asis)
register struct mbuf *m;
struct ifcb *ip;
struct socket *addr;
register len;
int link, asis;
{
	register struct imp *p;

	/* build header in current mbuf */

	m->m_off -= sizeof(struct imp);
	m->m_len += sizeof(struct imp);
	p = (struct imp *)((int)m + m->m_off);
	 
	p->i_host = addr->s_host;
	p->i_impno = addr->s_impno;
	p->i_lh = 0;		/* do not send logical host field */
	p->i_mlen = len + sizeof(struct imp);
	p->i_link = link;

	p->i_nff = 0xf;
	p->i_dnet = 0;
	p->i_lflgs = 0;
	p->i_type = 0;
	p->i_stype = 0;
	p->i_htype = 0;

	/* send message to i/f driver */

	imp_snd(m, ip, asis);
}

/* 
 * Enque a message passed as an mbuf chain from the higher level protocol
 * routines.  Do RFNM counting: if more than 8 messages are in flight,
 * hold on a RFNM queue until the i/f clears.
 */
imp_snd(mp, ip, asis)             
register struct mbuf *mp;
register struct ifcb *ip;
int asis;
{
	register struct imp *p;
	register struct host *hp;
	register struct mbuf *m;
	int i;
	struct socket ohost;

	if (!ip->if_avail) {			 /* make sure i/f is up */
		m_freem(mp);
		return(FALSE);
	}

	p = (struct imp *)((int)mp + mp->m_off);        /* -> 1822 leader */
	mp->m_act = NULL;               

#ifndef IMPLOOP

	/* do RFNM counting for type 0 messages (no more than 8 outstanding
	   to any host)  */ 

	if (p->i_type == 0 && !asis) {

                ohost.s_addr = (long)ip->if_net;
                ohost.s_host = p->i_host;
                ohost.s_imp = p->i_imp;
        	hp = h_find(&ohost);            /* get host table entry */

		/* if imp would block, put msg on queue until rfnm */

		if (hp != NULL)
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
 * Either call imp_output to supply an 1822 leader and send, or assume a
 * user supplied leader and send direct to imp_snd.
 */
imp_raw(up, m, len)
register struct ucb *up;
register struct mbuf *m;
int len;
{

	switch (up->uc_flags & RAWFLAG) {

	case RAWCOMP:		/* system supplies leader */

		/* leader will be constructed at end of first mbuf */

		m->m_off = MSIZE;
		m->m_len = 0;
		if (!imp_output(m, up->uc_srcif->if_ifcb, &up->uc_host,
				len, up->uc_lo, FALSE))
			u.u_error = ERAWBAD;
		break;

	case RAWASIS:		/* user supplies leader */

		m = m_free(m);		/* ->user supplied header */
		if (!imp_snd(m, up->uc_srcif->if_ifcb, TRUE))
			u.u_error = ERAWBAD;
		break;	

	default:
		u.u_error = ERAWBAD;
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

	for (inx = 0; inx < 3; inx++ ) { 

		/* get a buffer for the no-op */

		if ((m = m_get(0)) == NULL) 
			return(FALSE);
		m->m_off = MHEAD;
		m->m_len = 10;
		l = (struct imp *)((int)m + m->m_off);

		l->i_nff = 0xf;
		l->i_dnet = 0;
		l->i_lflgs = 0;
                l->i_htype = 0;
                l->i_host = 0;
                l->i_imp = 0;
                l->i_link = inx;
                l->i_stype = 0;
                l->i_mlen = 0;
                l->i_type = 4;        

		/* put no-op at head of output queue */

		spl5();
		if (ip->if_outq_hd == NULL)
			ip->if_outq_tl = m;
		else
			m->m_act = ip->if_outq_hd;
		ip->if_outq_hd = m;
		spl0();	
	}

	if (!ip->if_active)
		(*ip->if_out)(ip);
	return(TRUE);
}

log_err(ip, s)
struct ifcb *ip;
char *s;
{
	printf("dev %d/%d: impio: %s\n", major(ip->if_dev),minor(ip->if_dev),s);
}
