#include "../h/param.h"
#include "../h/inode.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/ucb.h"
#include "../bbnnet/fsm.h"
#include "../h/buf.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

extern struct user u;
char netbugf[] = "/etc/net/tcpdebug";

/*
 * This is the mainline of the network input process.  First, we
 * fork off as a separate process within the kernel, initialize the
 * net buffer manager, and timers.  (Net i/fs must be init by NETINIT 
 * system call).  Then, we
 * loop forever, sleeping on net input.  Once awakened by a local net
 * driver, we dispatch to each local interface in turn, to process the
 * input message(s).  After handling the interfaces, we adjust the net
 * buffer allocation, call on the tcp processor to handle any outstanding
 * timer or user events, and...
 */
netmain()
{
	register struct inode *ip;
	register struct proc *p;
	register struct ifcb *lp;
	int schar();
	int fid;

	fid = u.u_r.r_val1;
	fork();		/* returns twice - once in old and once in new  */
	u.u_r.r_val1 = fid;
	if (!u.u_error
              && u.u_r.r_val2) {	/* if child then */

		netcb.n_proc = p = u.u_procp;	/* -> proc table entry */
		p->p_flag |= SSYS;
		netcb.n_iss = 1;
		netcb.n_ip_cnt = 0;

		/* set up debugging file */

		netcb.n_debug = NULL;
		u.u_dirp = &netbugf[0]; 
		ip = namei(schar, 0);
		if (ip != NULL) 
			if ((ip->i_mode & IFMT) != IFREG) 
				iput(ip);
			else {
				netcb.n_debug = ip;
				prele(ip);
			}

		mbufinit();     /* init buf mgmt system */
		gatewayNGATE = gateway;	/* null gateway table */

		/* start timers */

		net_timer();

		for (;;) {      /* body of input process */

			/*
			 * reset local i/f if necessary, otherwise
			 * just handle net input
			 */
			for (lp=netcb.n_ifcb_hd; lp != NULL; lp=lp->if_next) {
process:
				if (lp->if_error && !lp->if_needinit) {
					lp->if_needinit = TRUE;
					netreset(lp);
				} else if (lp->if_avail && lp->if_inq_hd != NULL)
					(*lp->if_rcv)(lp);
			
			/* call TCP processor to handle any user or
			   timer events. */
				
				if (netcb.n_work != NULL)
					tcp_input(NULL, NULL);
				
			/*
			 * expand or shrink the buffer freelist as needed
			 */
	        		if (netcb.n_bufs < netcb.n_lowat)
	        			m_expand();
	        		else if (netcb.n_bufs > netcb.n_hiwat)
	        			m_relse();
			}
	
			/* 
			 * wait for net input (awakened by drivers)
			 */
			for (lp=netcb.n_ifcb_hd; lp != NULL; lp=lp->if_next) 
				if (lp->if_inq_hd != NULL)
					goto process;
			sleep((caddr_t)&netcb.n_ifcb_hd, PZERO-15); 
		}
	}
}

#ifdef BUFSTAT
struct buf_stat bufnull = {0};
#endif BUFSTAT

/*
 * Net timer routine: scheduled every second, calls ip and tcp timeout routines
 * and does any net-wide statistics.
 */
net_timer()
{
	register struct mbuf *m, *n;
	register struct ucb *up;
	register struct tcb *tp;
	register struct th *t;
	register struct ifcb *lp;
	register struct ipq *ip;
	register struct ip *i;
	register struct host *h;
	register struct work *w;

	ip_timeo();
	tcp_timeo();

#ifdef BUFSTAT
	bufstat = bufnull;
	for (up = netcb.n_ucb_hd; up != NULL; up = up->uc_next) {
		if (up->uc_flags & UTCP) {
			if ((tp = up->uc_tcb) == NULL)
				continue;
			bufstat.b_tcbs++;
			if (tp->t_state != ESTAB)
				bufstat.b_cons--;
			if ((t = tp->t_rcv_next) == NULL)
				continue;
			for (; t != (struct th *)tp; t=t->t_next) {
				m = dtom(t);
				while (m != NULL) {
					bufstat.b_tseq++;
					m = m->m_next;
				}
			}
			for (m=tp->t_rcv_unack; m != NULL; m=m->m_act) {
				n = m;
				do {
					bufstat.b_tuna++;
					n = n->m_next;
				} while (n != NULL);
			}
		}
		bufstat.b_cons++;
		bufstat.b_useq += up->uc_rsize + up->uc_ssize;
	}
	for (ip=netcb.n_ip_head; ip != NULL; ip=ip->iq_next) 
		for (i=ip->iqx.ip_next; i != (struct ip *)ip; i=i->ip_next) {
			m = dtom(i);
			while (m != NULL) {
				bufstat.b_ipfr++;
				m = m->m_next;
			} 
		}
	for (lp = netcb.n_ifcb_hd; lp != NULL; lp = lp->if_next) {
		for (m=lp->if_outq_hd; m != NULL; m = m->m_act) {
			n = m;
			do {
				bufstat.b_devq++;
				n = n->m_next;
			} while (n != NULL);
		}
		for (m=lp->if_outq_cur; m != NULL; m = m->m_next)
			bufstat.b_devq++;
		for (m=lp->if_inq_hd; m != NULL; m = m->m_act) {
			n = m;
			do {
				bufstat.b_devq++;
				n = n->m_next;
			} while (n != NULL);
		}
		for (m=lp->if_inq_msg; m != NULL; m = m->m_next)
			bufstat.b_devq++;
	}
	for (h=host; h < hostNHOST; h++)
		if (h->h_refct > 0) 
			for (m = h->h_outq; m != NULL; m = m->m_act) {
				n = m;
				do {
					bufstat.b_rfnm++;
					n = n->m_next;	
				} while (n != NULL);
			}
	for (w=netcb.n_work; w != NULL; w = w->w_next)
		if (w->w_type == INRECV || w->w_type == IUSEND)
			for (m=dtom(w->w_dat); m != NULL; m = m->m_next) 
				bufstat.b_work++;
#endif BUFSTAT

	timeout(net_timer, 0, HZ);
}

/*
 * Generic local net reset:  clear input and output queues and call driver
 * init routine.
 */
netreset(ip)
register struct ifcb *ip;
{
	register struct mbuf *m, *n;
	int s;	

	s = spl5();
	ip->if_avail = FALSE;

	/*
	 * clear all i/f queues
	 */
	for (m=ip->if_inq_hd; m != NULL; m=n) {
		n = m->m_act;
		m_freem(m);
	}
	ip->if_inq_hd = NULL;
	ip->if_inq_tl = NULL;

	for (m=ip->if_outq_hd; m != NULL; m=n) {
		n = m->m_act;
		m_freem(m);
	}
	ip->if_outq_hd = NULL;
	ip->if_outq_tl = NULL;

	if (ip->if_inq_msg != NULL) {
		m_freem(ip->if_inq_msg);
		ip->if_inq_msg = NULL;
		ip->if_inq_cur = NULL;
	}

	if (ip->if_outq_cur != NULL) {
		m_freem(ip->if_outq_cur);
		ip->if_outq_cur = NULL;
	}

	h_reset(iptonet(ip->if_addr));
	splx(s);
	printf("%s%d: netreset\n", ip->if_name, ip->if_unit);
	ip->if_resets++;
	
	/* 
	 * now call net driver specific init routine to reinit
	 */
	(*ip->if_init)(ip);
}

/*
 * Save anomolous packets for debugging purposes.  If limit is non-zero,
 * save up to limit packets.  Otherwise just free.
 */
netlog(mp)
register struct mbuf *mp;
{
	register struct mbuf *m;
		
	if (netcb.n_loglim > 0) {

		/* free all but header */

		if (mp->m_next != NULL) {
			m_freem(mp->m_next);
			mp->m_next = NULL;
		}

		/* add to end */

		mp->m_act = NULL;
		if (netcb.n_log_hd != NULL)
			netcb.n_log_tl->m_act = mp;
		else
			netcb.n_log_hd = mp;
		netcb.n_log_tl = mp;
		netcb.n_logct++;
		
		/* if limit exceeded, remove from head */

		while (netcb.n_logct > netcb.n_loglim && 
		    (m = netcb.n_log_hd) != NULL) {
			netcb.n_log_hd = m->m_act;
			m_free(m);
			netcb.n_logct--;
		}
	} else
		m_freem(mp);
}
