#include "../h/param.h"
#include "../h/systm.h"
#include "../h/buf.h"
#include "../h/dir.h"
#include "../h/file.h"
#include "../h/inode.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/imp.h"
#include "../bbnnet/ifcb.h"
#include "../bbnnet/ip.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/raw.h"
#include "../bbnnet/udp.h"
#include "../bbnnet/ucb.h"
#include "../bbnnet/fsm.h"

/*
 * Network user i/f routines
 */

/*
 * Open a network connection.  Allocate an open file structure and a network
 * user control block (ucb).  Verify user open parameters and do protocol
 * specific open functions.
 */
netopen(ip, mode)
struct inode *ip;
char *mode;       
{
	register struct ucb *up;
	register struct con *cp;
	register struct file *fp;
	register struct ifcb *lp;
	register struct mbuf *m;
	int snd, rcv, mask, i;
	struct con const;
	net_t net;

	iput(ip);               /* release dev inode since we don't use it */

	/* get user connection structure parm */

	if (copyin(mode, &const, sizeof(const)) < 0) {
		u.u_error = EFAULT;
		return;
	}
	cp = (struct con *)&const;

	/* calculate send and receive buffer allocations */

	snd = cp->c_sbufs << 3;
	rcv = cp->c_rbufs << 3;
	if (snd < 8 || snd > 64)
		snd = 8;
	if (rcv < 8 || rcv > 64)
		rcv = 8;

	/* make sure system has enough buffers for connection */

	if (!(cp->c_mode&CONCTL) && 
	    (snd + rcv + 2) > (NNETPAGES*NMBPG-32-netcb.n_lowat)) {
		u.u_error = ENETBUF;
		return;
	}

	/* allocate file entry and ucb */

	if ((fp = falloc()) == NULL) 
		return;
	i = u.u_r.r_val1;

	if ((m = m_get(1)) == NULL) {
		u.u_error = ENETBUF;
		u.u_ofile[i] = NULL;
		return;
	}
	m->m_off = MHEAD;
	up = mtod(m, struct ucb *);
	up->uc_next = netcb.n_ucb_hd;
	up->uc_prev = NULL;
	if (netcb.n_ucb_hd != NULL)
		netcb.n_ucb_hd->uc_prev = up;
	netcb.n_ucb_hd = up;

	/* get user file table entry and point at ucb */

#ifndef mbb
	fp->f_flag = (FNET|FREAD|FWRITE);
#else
	fp->f_flag = (FTNET|FREAD|FWRITE);
#endif
	fp->f_un.f_ucb = up;
	fp->f_inode = NULL;

	/* init common ucb fields */

	up->uc_proc = u.u_procp;
	up->uc_proto = NULL;
	up->uc_snd = snd;
	up->uc_rcv = rcv;
	up->uc_sbuf = NULL;
	up->uc_rbuf = NULL;
	up->uc_timeo = cp->c_timeo;             /* overlays uc_ssize */
	up->uc_rsize = 0;
	up->uc_state = 0;
	up->uc_xstat = 0;
	up->uc_flags = cp->c_mode & ~(UTCP+ULOCK);

	/* control connections are all done here */

	if (up->uc_flags&UCTL)
		return;

	up->uc_flags ↑= CONACT;
	netcb.n_lowat += snd + rcv + 2;
	netcb.n_hiwat = (netcb.n_lowat * 3) >> 1;

	/* save foreign host address, route will be filled in later */

	up->uc_route = NULL;
	up->uc_host = cp->c_fcon;

	/* if local address is specified, make sure its one of ours and use
	   it if it is, otherwise pick the "best" local address from the
	   local i/f table */

	if (cp->c_lcon.s_addr != NULL) {
		up->uc_local = cp->c_lcon;
		cp->c_lcon.s_addr = addr_1822(cp->c_lcon);	/* GROT! */
		for (lp = netcb.n_ifcb_hd; lp != NULL; lp = lp->if_next) 
			if (lp->if_addr.s_addr == cp->c_lcon.s_addr)
				goto gotaddr;
		u.u_error = ENETRNG;	/* spec local address not in table */
		goto conerr;	
gotaddr:
		/* make sure specified i/f is available */

		if (lp->if_avail)
			up->uc_srcif = lp;
		else {
			u.u_error = ENETDOWN;
			goto conerr;
		}
	} else {

		/* find default 'preferred' address, look for first 
		   available i/f */

		up->uc_srcif = NULL;
		for (lp = netcb.n_ifcb_hd; lp != NULL; lp = lp->if_next) {
			if (lp->if_avail) {
				up->uc_srcif = lp;	
				net = iptonet(cp->c_fcon);
				break;
			}
		}

		/* error if none available */

		if (up->uc_srcif == NULL) {
			u.u_error = ENETDOWN;
			goto conerr;
		}

		/* if foreign host is on a net that we are, use the address
		   of the i/f on that net */

		for (lp = netcb.n_ifcb_hd; lp != NULL; lp = lp->if_next) 
			if (iptonet(lp->if_addr)==net && lp->if_avail) {
				up->uc_srcif = lp;
				break;
			}

		/* use default local addr for active opens, wild card for
		   passive. */

		if (cp->c_mode & CONACT)
			up->uc_local = up->uc_srcif->if_addr;
		else
			up->uc_local.s_addr = NULL;
	}

	/* call protocol open routine for rest of init */

	switch (cp->c_mode & CONMASK) {

	case CONTCP:
		tcpopen(up, cp);
		if (u.u_error)
			goto conerr;
		break;

	case CONUDP:
		up->uc_udp.u_fport = cp->c_fport;
		up->uc_udp.u_lport = uniqport(up, cp->c_lport, cp->c_fport);
		if (u.u_error)
			goto conerr;
		break;

	case CONRAW:
	case CONIP:
		raw_add(up, cp->c_proto);
		if (u.u_error)
			goto conerr;
		break;

       	default:
		u.u_error = ENETPARM;
	conerr:
		netclose(fp);
		u.u_ofile[i] = NULL;
	}
}

/*
 * TCP specific open routine.  Allocate a TCB and set up an open work request
 * for the finite state machine.
 */
tcpopen(up, cp)
register struct ucb *up;
register struct con *cp;
{
	register struct mbuf *m;
	register struct tcb *tp;
	register char *p;
	register i;

	/* allocate a buffer for a tcb */

	if ((m = m_get(1)) == NULL) {           /* no buffers */
		u.u_error = ENETBUF;    
		return;
	}
	m->m_off = MHEAD;
	tp = mtod(m, struct tcb *);		/* -> tcb */

	/* zero tcb for initialization */

	for (p = (char *)tp, i = MLEN; i > 0; i--)
		*p++ = 0;

	/* fill in ucb and tcb fields */

	tp->t_ucb = up;
	up->uc_tcb = tp;
	up->uc_flags |= UTCP;
	tp->t_fport = cp->c_fport;
	tp->t_lport = uniqport(up, cp->c_lport, cp->c_fport);

	/* if local port not unique, free tcb */

	if (u.u_error) {
		up->uc_flags &= ~UTCP;
		up->uc_tcb = NULL;
		m_free(m);
		return;
	}

	/*  make a user open work request for tcp */

	w_alloc((cp->c_mode & CONACT ? IUOPENR : IUOPENA), 0, tp, NULL);

	/* block until connection has opened or error */
	
	if (!(up->uc_flags & UNOBLOK))
		awaitconn(up, tp);
}

/*
 * Network close routine.  For TCP connections, issue a close work request to
 * the tcp fsm.  Otherwise, just clean up the ucb and the file structure 
 */
netclose(fp)
register struct file *fp; 
{
	register struct ucb *up;
	register struct tcb *tp;

	up = fp->f_un.f_ucb;

	if (up->uc_flags & UTCP) {      /* tcp connection */

		tp = up->uc_tcb;

		/* if connection has not already been closed, by user or
		   system, enqueue work request.  otherwise, if the tcb
		   has been deleted (completed close), then release the ucb */

		if (tp != NULL) {	/* tcb still exists */
			tp->usr_abort = TRUE;	/* says we've closed */
			if (!tp->usr_closed)
				w_alloc(IUCLOSE, 0, tp, up);
			if (up->uc_flags & UCWAIT)
				sleep(tp, PZERO+1);
			goto ufree;
		} 

	} else if (up->uc_flags&(UIP+URAW+UUDP)) {        /* raw connection */

	/* free ucb and lower system buffer allocation */

        	netcb.n_lowat -= up->uc_snd + up->uc_rcv + 2;
		netcb.n_hiwat = (netcb.n_lowat * 3) >> 1;
		if (up->uc_route != NULL) {
        		h_free(up->uc_route);
			up->uc_route = NULL;
		}
		if (!(up->uc_flags & UUDP))
			raw_free(up);
	}
	up->uc_proc = NULL;
	if (up->uc_next != NULL)
		up->uc_next->uc_prev = up->uc_prev;
	if (up->uc_prev != NULL)
		up->uc_prev->uc_next = up->uc_next;
	else
		netcb.n_ucb_hd = up->uc_next;
	m_free(dtom(up));
ufree:
	fp->f_flag = 0;
	fp->f_count = 0;
	fp->f_un.f_ucb = NULL;
}  

/*
 * Network read routine. Just call protocol specific read routine.
 */
netread(up)
register struct ucb *up;
{

	switch (up->uc_flags & CONMASK) {

	case UTCP:              
		tcp_read(up);
		break;
	case UIP:                                           
	case URAW:
	case UUDP:
		raw_read(up);
		break;
	default:
		u.u_error = ENETPARM;
	}
}

/*
 * Network write routine. Just call protocol specific write routine.
 */
netwrite(up)
register struct ucb *up;
{
	if (up->uc_state)
		u.u_error = ENETSTAT;		/* new status to report */
	else {
		switch (up->uc_flags & CONMASK) {

		case UTCP:              
			tcp_write(up);
			break;
		case UIP:                                           
		case URAW:
		case UUDP:
			raw_write(up);
			break;
		default:
			u.u_error = ENETPARM;
		}
	}
}       

/*
 * TCP read routine.  Copy data from the receive queue to user space.
 */
tcp_read(up)                     
register struct ucb *up;
{
	register struct mbuf *m;
	register struct tcb *tp;
	register len, eol;
	int incount;

	if ((tp = up->uc_tcb) == NULL) {
		u.u_error = ENETERR;
		return;
	}
	if (!tp->syn_acked)		/* block if not connected */
		awaitconn(up, tp);
	eol = FALSE;
	incount = u.u_count;

	while (u.u_count > 0 && !eol && !u.u_error) 

	        if (up->uc_rbuf != NULL) {	/* data available */

			/* lock recieve buffer */

                        while (up->uc_flags & ULOCK)
                        	sleep(&up->uc_flags, PZERO);
                        up->uc_flags |= ULOCK;
                        
			/* copy the data an mbuf at a time */

			m = up->uc_rbuf;
                        while (m != NULL && u.u_count > 0 && !eol && !u.u_error) {
                        
                        	len = MIN(m->m_len, u.u_count);
                        	iomove((caddr_t)((int)m + m->m_off), len, B_READ);
                        
                        	if (!u.u_error) {

					/* free or adjust mbuf */

                        		if (len == m->m_len) {
        				        eol = (int)m->m_act;
                        			m = m_free(m);
                        			--up->uc_rsize;
                        		} else {
                        			m->m_off += len;
                        			m->m_len -= len;
                        		}
                        	}
                        }
			up->uc_rbuf = m;
			up->uc_flags &= ~ULOCK; /* unlock buffer */
			wakeup(&up->uc_flags);
			/*
			 * If any data was read, return even if the
			 * full read request was not satisfied
			 */
			if (incount != u.u_count)
				return;
		} else if (up->uc_state == 0) {                
			/*
			 * If no data left and foreign FIN rcvd, just
			 * return, otherwise make work request and wait
			 * for more data 
			 */
			if (tp->fin_rcvd && rcv_empty(tp))       
				break;

        		w_alloc(IURECV, 0, tp, NULL);
			sleep(up, PZERO + 1);
		} else {
			/*
			 * Net status to return.
			 */
			u.u_error = ENETSTAT;
			break;
		}
}

/*
 * TCP write routine.  Copy data from user space to a net buffer chain.  Put
 * this chain on a send work request for the tcp fsm.
 */
tcp_write(up)                    
register struct ucb *up;
{
	register struct tcb *tp;
	register struct mbuf *m, *n;
	register len, bufs;
	int incount;
	struct {
		struct mbuf *top;
	} nn;

	if ((tp = up->uc_tcb) == NULL) {                    
		u.u_error = ENETERR;
		return;
	}
	if (!tp->syn_acked)		/* block if not connected */
		awaitconn(up, tp);
	incount = u.u_count;

	while (u.u_count > 0 && !u.u_error && up->uc_state == 0) {

		/* #bufs can send now */

		bufs = (int)up->uc_snd - (int)up->uc_ssize;       

		/* top of chain to send */

		nn.top = NULL;                          
		n = (struct mbuf *)&nn;

		/* copy data from user to mbuf chain for send work req */

        	while (bufs-- > 0 && u.u_count > 0) {
        
			/* set up an mbuf */

        		if ((m = m_get(1)) == NULL) {
				u.u_error = ENETBUF;
				return;
			}
        		m->m_off = MHEAD;

			/* copy user data in */

        		len = MIN(MLEN, u.u_count);
        		iomove((caddr_t)((int)m + m->m_off), len, B_WRITE);

			/* chain to work request mbufs or clean up if error */

        		if (!u.u_error) {
        			m->m_len = len;
        			n->m_next = m;
        			n = m;
        		} else {
        			m_freem(nn.top);
				return;
			}
        	}
        
        	w_alloc(IUSEND, 0, tp, nn.top);         /* make work req */

		/* if more to send, sleep on more buffers */

		if (u.u_count > 0)
			sleep(up, PZERO+1);
	}
}

/*
 * Raw IP and local net read routine.  Copy data from raw read queue to user
 * space.
 */
raw_read(up)                     
register struct ucb *up;
{
	register struct mbuf *m, *next;
	register len;
	int incount;

	incount = u.u_count;
	while (up->uc_rbuf == NULL && up->uc_state == 0)
		sleep(up, PZERO+1);

	/* lock recieve buffer */

        while (up->uc_flags & ULOCK)
        	sleep(&up->uc_flags, PZERO);
        up->uc_flags |= ULOCK;
        
	/* copy the data an mbuf at a time */

        if ((m = up->uc_rbuf) != NULL)
		next = m->m_act;
	else
		next = NULL;

        while (m != NULL && u.u_count > 0 && !u.u_error) {
        
        	len = MIN(m->m_len, u.u_count);
        	iomove((caddr_t)((int)m + m->m_off), len, B_READ);
        
        	if (!u.u_error) {

			/* free or adjust mbuf */

        		if (len == m->m_len) {
        			m = m_free(m);
        			--up->uc_rsize;
        		} else {
        			m->m_off += len;
        			m->m_len -= len;
        		}
        	}
        }

	if (m == NULL)
		up->uc_rbuf = next;
	else {
		m->m_act = next;
		up->uc_rbuf = m;
	}
	up->uc_flags &= ~ULOCK; /* unlock buffer */
	wakeup(&up->uc_flags);

	if (incount == u.u_count && up->uc_state != 0)
		u.u_error = ENETSTAT;
}

/*
 * Raw IP or local net write routine.  Copy data from user space to a network
 * buffer chain.  Call the appropriate format routine to set up a packet
 * header and send the data.
 */
raw_write(up)                    
register struct ucb *up;
{
	register struct mbuf *m, *n, *top;
	register len, bufs, ulen;

	/* get header mbuf */

	if ((top = n = m_get(1)) == NULL) {
		u.u_error = ENETBUF;
		return;
	}

	bufs = up->uc_snd;                      /* #bufs can send now */
	ulen = u.u_count;

	/* copy data from user to mbuf chain for send */

        while (bufs-- > 0 && u.u_count > 0) {
        
		/* set up an mbuf */

        	if ((m = m_get(1)) == NULL) {
			u.u_error = ENETBUF;
			return;
		}
        	m->m_off = MHEAD;
			
		/* copy user data in */

        	len = MIN(MLEN, u.u_count);
        	iomove((caddr_t)((int)m + m->m_off), len, B_WRITE);

		/* chain to header mbuf or clean up if error */

        	if (!u.u_error) {
        		m->m_len = len;
        		n->m_next = m;
        		n = m;
        	} else {
        		m_freem(top);
			return;
		}
        }

	/* only send raw message if all of it could be read */

	if (u.u_count > 0) {
		u.u_error = ERAWBAD;
		m_freem(top);
		return;
	}

	/* send to net */

	switch (up->uc_flags & CONMASK) {

	case UUDP:
		udp_output(up, top, ulen);
		break;

	case UIP:
        	ip_raw(up, top, ulen);
		break;

	case URAW:
		(*up->uc_srcif->if_raw)(up, top, ulen);
		break;

	default:
		m_freem(top);
		u.u_error = ENETPARM;
	}
}

/*
 * Network control functions
 */
netioctl(up, cmd, cmdp)
register struct ucb *up;
int cmd;
caddr_t cmdp;
{
	register struct inode *ip;
	register struct tcb *tp;
	register struct mbuf *m;
	register struct gway *gp;
	register struct ifcb *lp;
	char *p, *q;
	struct host *route;
	short proto;
	int len;
	struct socket addr;
	struct netstate nstat;
	struct netopt opt;
	struct netinit init;

	tp = up->uc_tcb;

	switch (cmd) {

	case NETGETS:                   /* get net status */
		bcopy((caddr_t)&up->uc_snd, (caddr_t)&nstat, sizeof(struct netstate));
		if (up->uc_flags & UTCP && tp != NULL) {
			nstat.n_lport = tp->t_lport;
			nstat.n_fport = tp->t_fport;
		} else if (up->uc_flags & UUDP) {
			nstat.n_lport = up->uc_udp.u_lport;
			nstat.n_fport = up->uc_udp.u_fport;
		}

		nstat.n_fcon = up->uc_host;
		nstat.n_lcon = up->uc_local;

		if (copyout((caddr_t)&nstat, cmdp, sizeof(struct netstate)))
			u.u_error = EFAULT;
		else {
			up->uc_xstat = 0;
			up->uc_state = 0;
		}
		break;

	case NETSETU:                   /* set urgent mode */
		up->uc_flags |= UURG;
		break;

	case NETRSETU:                  /* reset urgent mode */
		up->uc_flags &= ~UURG;
		break;

	case NETSETE:                   /* set eol mode */
		up->uc_flags |= UEOL;
		break;

	case NETRSETE:                  /* reset eol mode */
		up->uc_flags &= ~UEOL;
		break;

	case NETCLOSE:                  /* tcp close but continue to rcv */
		if ((up->uc_flags & UTCP) && tp != NULL && !tp->usr_closed) 
			w_alloc(IUCLOSE, 0, tp, up);
		break;

	case NETABORT:                  /* tcp user abort */
		if ((up->uc_flags & UTCP) && tp != NULL)
			w_alloc(IUABORT, 0, tp, up);
		break;

	case NETOWAIT:			/* tcp wait until connected */
		if (up->uc_flags&UTCP && tp != NULL && !tp->syn_acked)
			awaitconn(up, tp);
		break;

	case NETSETD:                   /* set debugging file */
		if (!suser()) {
			u.u_error = EPERM;
			break;
		}
		if (cmdp == NULL) {
			if (netcb.n_debug) {
				plock(netcb.n_debug);
				iput(netcb.n_debug);
				netcb.n_debug = NULL;
			}
		} else {
			if (netcb.n_debug != NULL) {
				u.u_error = EBUSY;
				break;
			}
			u.u_dirp = cmdp;
			if ((ip = namei(uchar, 0)) != NULL) {
				if ((ip->i_mode & IFMT) != IFREG) {
					u.u_error = EACCES;
					iput(ip);
				} else {
					netcb.n_debug = ip;
					prele(ip);
				}
			}
		}
		break;

	case NETRESET:                  /* forced tcp reset */
		if (!suser()) {
			u.u_error = EPERM;
			break;
		}
		for (up = netcb.n_ucb_hd; up != NULL; up = up->uc_next)
			if ((up->uc_flags & UTCP) && 
			     up->uc_tcb != NULL &&
			     up->uc_tcb == (struct tcb *)cmdp) {
				w_alloc(INCLEAR, 0, cmdp, NULL);
				return;
			}
		u.u_error = EINVAL;
		break;

	case NETDEBUG:                  /* toggle debugging flag */
		for (up = netcb.n_ucb_hd; up != NULL; up = up->uc_next) 
			if ((up->uc_flags & UTCP) && up->uc_tcb != NULL &&
			    up->uc_tcb == (struct tcb *)cmdp) {
				up->uc_flags ↑= UDEBUG;
				return;
			}
		u.u_error = EINVAL;
		break;
	
	case NETTCPOPT:			/* copy ip options to tcb */
		if ((up->uc_flags & UTCP) && tp != NULL && cmdp != NULL) {
			
			if (copyin((caddr_t)cmdp, &opt, sizeof opt)) {
				u.u_error = EFAULT;
				break;
			}

			/* maximum option string is 40 bytes */

			if (opt.nio_len > IPMAXOPT) {
				u.u_error = EINVAL;
				break;
			}

			/* get a buffer to hold the option string */

			if ((m = m_get(1)) == NULL) {
				u.u_error = ENETBUF;
				break;
			}
			m->m_off = MHEAD;
			m->m_len = opt.nio_len;

			if (copyin((caddr_t)opt.nio_opt, 
				(caddr_t)((int)m + m->m_off), opt.nio_len)) {
				m_free(m);
				u.u_error = EFAULT;
				break;	
			}
			tp->t_opts = mtod(m, char *);
			tp->t_optlen = opt.nio_len;
		}
		break;

	case NETGINIT:			/* reread the gateway file */
		if (!suser()) {
			u.u_error = EPERM;
			break;
		}
		if (cmdp == NULL)	/* reset the table */
			gatewayNGATE = gateway;
		else {
			gp = gatewayNGATE;
			/*
			 * too many entries
			 */
			if (ngate <= gp - gateway) {
				u.u_error = ETABLE;
				break;
			}
			if (copyin((caddr_t)cmdp, (caddr_t)gp, 
				   sizeof(struct gway))) {
				u.u_error = EFAULT;
				break;
			}
			/*
			 * can we use this entry?
			 */
			if (gp->g_flags&GWFORMAT) {
				for (lp=netcb.n_ifcb_hd; 
						  lp != NULL; lp=lp->if_next) 
					if (iptonet(lp->if_addr) == 
					    gp->g_lnet) {
						gp->g_ifcb = lp;
						gatewayNGATE = ++gp;
						return;
					}
			}
			u.u_error = EINVAL;
		}
		break;

	case NETPRADD:			/* add protocol numbers to ucb chain */
	case NETPRDEL:			/* delete protocol numbers from chain */
		/* get arg pointer and arg length and verify */

		if (copyin((caddr_t)cmdp, &opt, sizeof opt)) {
			u.u_error = EFAULT;
			break;
		}

		/* add or delete each proto number on ucb chain */

		for (len=opt.nio_len; len > 0; len -= sizeof(short)) {

			/* get next proto number from user */

			if (copyin((caddr_t)opt.nio_opt, proto, sizeof(short))){
				u.u_error = EFAULT;
				break;
			}
			if (cmd == NETPRADD)	
				raw_add(up, proto);
			else
				raw_del(up, proto);
			opt.nio_opt += sizeof(short);
		}
		break;

	case NETPRSTAT:			/* return current list of proto nos. */
		/* get arg pointer and arg length */

		if (copyin((caddr_t)cmdp, &opt, sizeof opt)) {
			u.u_error = EFAULT;
			break;
		}
		raw_stat(up, opt.nio_opt, opt.nio_len);
		break;

	case NETROUTE:			/* change ip route */
		route = up->uc_route;
		if (cmdp == NULL) 
			up->uc_route = NULL;
		else {
			if (copyin((caddr_t)cmdp, &addr, sizeof addr)) {
				u.u_error = EFAULT;
				break;
			}

			/* substitute new route */

			up->uc_route = h_make(&addr, FALSE);
		}	

		/* free old entry */

		if (route != NULL)
			h_free(route);
		break;

	case NETINIT:			/* init net i/f */
	case NETDISAB:			/* disable net i/f */		
		/* 
		 * Must be super-user!
		 */
		if (!suser()) {
			u.u_error = EPERM;
			break;
		}
		/*
		 * Copy and verify arguments.
		 */
		if (copyin((caddr_t)cmdp, &init, sizeof init)) {
			u.u_error = EFAULT;
			break;
		}
		/*
		 * Look for ifcb to init.
		 */
		for (lp = netcb.n_ifcb_hd; lp != NULL; lp = lp->if_next) {
			p = init.ni_name;
			q = lp->if_name;
			if (init.ni_unit == lp->if_unit) {
				len = 0;
				while (*p == *q && *q && len++ < NIMAX) {
					p++; q++;
				}
				if (len == NIMAX || *p == *q) {
				/*
				 * For init copy address and call dev init
				 * routine directly.  For disable, set
				 * flag for driver, and call netreset to
				 * clear queues and call dev init routine.
				 */
					if (cmd == NETINIT && !lp->if_avail) {
						lp->if_disab = FALSE;
						lp->if_addr = init.ni_addr;
						(*lp->if_init)(lp);
					} else if (cmd == NETDISAB) {
						lp->if_disab = TRUE;
						netreset(lp);
						/*
						 * Tell users on this i/f
						 */
						for (up=netcb.n_ucb_hd;
						     up != NULL; 
						     up = up->uc_next)
							if (up->uc_srcif == lp)
								to_user(up,
									UABORT);
					}
					return;
				}
			}
		}
		u.u_error = EINVAL;
		break;

	default:
		u.u_error = ENETPARM;
	}
}

/*
 * Generate default local port number, if necessary and check if specified 
 * connection is unique.
 */
uniqport(up, lport, fport)
register struct ucb *up;
register u_short lport, fport;
{
	register struct ucb *q;
	register i;
	int deflt = FALSE;

	/* if local port is zero, make up a default */

	if (lport == 0) {
		for (q = netcb.n_ucb_hd, i = 1; q != NULL && q != up;
						q = q->uc_next, i++);
		lport = (i << 8) | (up->uc_proc->p_pid & 0xff);
		deflt++;
	}
		
	/* make sure port number is unique, for defaults, just increment
	   original choice until it is unique. */

tryagain:
	for (q = netcb.n_ucb_hd; q != NULL; q = q->uc_next) 
		if (q != up && q->uc_host.s_addr == up->uc_host.s_addr &&
		    q->uc_local.s_addr == up->uc_local.s_addr &&  
		    ((q->uc_flags & UTCP && q->uc_tcb != NULL &&
		      q->uc_tcb->t_lport == lport &&
		      q->uc_tcb->t_fport == fport) ||
		     (q->uc_flags & UUDP &&
		      q->uc_udp.u_lport == lport &&
		      q->uc_udp.u_fport == fport))) {
			if (deflt)  {
				if (++lport < 0x100)
					lport = 0x100;
				goto tryagain;
			}
			break;
		}
		
	/* if connection is unique, return local port, otherwise set error */

	if (q == NULL)
		return(lport);
	else {
		u.u_error = ENETRNG;
		return(0);
	}
}

/*
 * Block until TCP connection is open or error.
 */
awaitconn(up, tp)
register struct ucb *up;
register struct tcb *tp;
{

	while (up->uc_state == 0 && !tp->syn_acked)
          	sleep(up, PZERO+1);

	if (up->uc_state != 0) {
		if (up->uc_state & UINTIMO)
			u.u_error = ENETTIM;
		else if (up->uc_state & URESET)
			u.u_error = ENETREF;
		else if (up->uc_state & UNRCH)
			u.u_error = ENETUNR;
		else if (up->uc_state & UDEAD)
			u.u_error = ENETDED;
		else
			u.u_error = ENETSTAT;
	} else if (!tp->syn_acked)
		u.u_error = EINTR;
}
