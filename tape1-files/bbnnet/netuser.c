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
#include "../bbnnet/tcp.h"
#include "../bbnnet/ucb.h"
#include "../bbnnet/fsm.h"
#include "../bbnnet/tcp_pred.h"

extern struct user u;

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
	register struct if_local *lp;
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

	if ((snd + rcv + 2) > (NNETPAGES*NMBPG-32-netcb.n_lowat)) {
		u.u_error = ENETBUF;
		return;
	}

	/* allocate a ucb from the connection table */

	for (up = contab; up->uc_proc != NULL && up < conNCON; up++);

	if (up == conNCON) {            /* connection table full */
		u.u_error = ENETCON;
		return;
	}

	/* get user file table entry and point at ucb */

	if ((fp = falloc()) == NULL) {
		return(-1);
	}
	i = u.u_r.r_val1;
	fp->f_flag = (FNET|FREAD|FWRITE);
	fp->f_un.f_ucb = up;
	fp->f_inode = NULL;

	/* init common ucb fields */

	up->uc_proc = u.u_procp;
	up->uc_snd = snd;
	up->uc_rcv = rcv;
	netcb.n_lowat += snd + rcv + 2;
	netcb.n_hiwat = 2 * netcb.n_lowat;
	up->uc_sbuf = NULL;
	up->uc_rbuf = NULL;
	up->uc_timeo = cp->c_timeo;             /* overlays uc_ssize */
	up->uc_rsize = 0;
	up->uc_state = 0;
	up->uc_lo = 0;
	up->uc_hi = 0;

	/* save foreign host address, route will be filled in later */

	up->uc_route = NULL;
	up->uc_host = cp->c_fcon;

	/* if local address is specified, make sure its one of ours and use
	   it if it is, otherwise pick the "best" local address from the
	   local i/f table */

	if (cp->c_lcon.s_addr != NULL) {
		for (lp=locnet; lp < &locnet[nlocnet]; lp++)
			if (lp->if_addr.s_addr == cp->c_lcon.s_addr)
				goto gotaddr;
		u.u_error = ENETRNG;	/* spec local address not in table */
		goto conerr;	
gotaddr:
		/* make sure specified i/f is available */

		if (lp->if_ifcb->if_avail)
			up->uc_srcif = lp;
		else {
			u.u_error = ENETDOWN;
			goto conerr;
		}
	} else {

		/* find default 'preferred' address, look for first 
		   available i/f */

		up->uc_srcif = NULL;
		for (lp=locnet; lp < &locnet[nlocnet]; lp++) {
			if (lp->if_ifcb->if_avail) {
				up->uc_srcif = locnet;	
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

		for (lp=locnet; lp < &locnet[nlocnet]; lp++) 
			if (lp->if_ifcb->if_net==net && lp->if_ifcb->if_avail) {
				up->uc_srcif = lp;
				break;
			}
	}

	up->uc_flags = 0;
	if (cp->c_mode & CONDEBUG)
		up->uc_flags |= UDEBUG;

	/* call protocol open routine for rest of init */

	switch (cp->c_mode & 036) {

	case CONTCP:
		tcpopen(up, cp);
		if (u.u_error)
			goto conerr;
		break;

	case CONRAW:
		mask = URAW;
		up->uc_flags |= cp->c_mode & (RAWCOMP+RAWASIS+RAWERR+RAWRALL);
		goto concom;

	case CONIP:
		mask = UIP;
		up->uc_flags |= cp->c_mode & (RAWFLAG+RAWERR+RAWRALL);
	concom:
		if (raw_range(up, mask, &cp->c_fcon, cp->c_lo, cp->c_hi)) {
        		up->uc_flags |= mask;
        		up->uc_lo = cp->c_lo;
        		up->uc_hi = cp->c_hi;
			break;
		}
		u.u_error = ENETRNG;
		goto conerr;

	case CONCTL:
		up->uc_flags |= UCTL;
		break;

       	default:
		u.u_error = ENETPARM;
	conerr:
		netclose(fp);
		u.u_ofile[i] = NULL;
		return(-1);
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
	tp = (struct tcb *)((int)m + MHEAD);    /* -> tcb */

	/* zero tcb for initialization */

	for (p = (char *)tp, i = MLEN; i > 0; i--)
		*p++ = 0;

	/* fill in ucb and tcb fields */

	tp->t_ucb = up;
	tp->t_fport = cp->c_fport;

	/* if local port is 0, make one up, make sure connection will
	   be unique. */

	if (cp->c_lport == 0) {
		tp->t_lport = (((int)(up - contab) + 1) << 8) | 
				(up->uc_proc->p_pid & 0xff);

		/* keep incrementing port until connection is unique */

		while (!uniqport(up, tp)) {
			if (++tp->t_lport < 0x100)
				tp->t_lport = 0x100;
		}
	} else {
		tp->t_lport = cp->c_lport;
		if (!uniqport(up, tp)) {
			u.u_error = ENETRNG;
			m_free(m);
			return;
		}
	}

	up->uc_tcb = tp;
	up->uc_flags |= UTCP;

	/*  make a user open work request for tcp */

	if ((cp->c_mode & CONACT) == CONACT) 
		w_alloc(IUOPENR, 0, tp, NULL);
	else {
		up->uc_flags |= ULISTEN;
		w_alloc(IUOPENA, 0, tp, NULL);
	}

	/* block until connection has opened or error */

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
			u.u_error = ENETERR;
	} else if (!tp->syn_acked)
		u.u_error = EINTR;
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
	tp = up->uc_tcb;

	if (up->uc_flags & UTCP) {      /* tcp connection */

		/* if connection has not already been closed, by user or
		   system, enqueue work request.  otherwise, if the tcb
		   has been deleted (completed close), then release the ucb */

		if (tp != NULL) {	/* tcb still exists */
			tp->usr_abort = TRUE;	/* says we've closed */
			if (!tp->usr_closed)
				w_alloc(IUCLOSE, 0, tp, up);
		} else
			up->uc_proc = NULL;

	} else {        /* raw connection */

	/* free ucb and lower system buffer allocation */

        	netcb.n_lowat -= up->uc_snd + up->uc_rcv + 2;
        	netcb.n_hiwat = 2 * netcb.n_lowat;
		if (up->uc_route != NULL) {
        		h_free(up->uc_route);
			up->uc_route = NULL;
		}
        	up->uc_proc = NULL;
	}

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

	if (up->uc_state)               /* new status to report */
		u.u_error = ENETSTAT;
	else {
        	switch (up->uc_flags & URMSK) {
        
        	case UTCP:              
        		tcp_read(up);
        		break;
        	case UIP:                                           
        	case URAW:
        		raw_read(up);
        		break;
        	default:
        		u.u_error = ENETPARM;
        	}
	}
}

/*
 * Network write routine. Just call protocol specific write routine.
 */
netwrite(up)
register struct ucb *up;
{
	switch (up->uc_flags & URMSK) {

	case UTCP:              
		tcp_write(up);
		break;
	case UIP:                                           
	case URAW:
		raw_write(up);
		break;
	default:
		u.u_error = ENETPARM;
	}
}       

/*
 * TCP read routine.  Copy data from the receive queue to user space.
 */
tcp_read(up)                     /* read data from the net */
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
	eol = FALSE;
	incount = u.u_count;

	while (u.u_count > 0 && !eol && !u.u_error && !up->uc_state) 

	        if (up->uc_rsize != 0) {   /* data available */

			/* lock recieve buffer */

                        while (up->uc_flags & ULOCK)
                        	sleep(&up->uc_flags, PZERO);
                        up->uc_flags |= ULOCK;
                        
			/* copy the data an mbuf at a time */

			m = up->uc_rbuf;

			/* Must check for bad read buffer count.  Count can
			   be > 0 with no buffers on queue due to a race
			   condition when a foreign close happens. Just
			   correct count and ignore. */

			if (m == NULL) {
				printf("net_read: bad rsize %d ",up->uc_rsize);
				if (up->uc_tcb == NULL)
					printf(" no tcb\n");
				else
					printf(" tcb=%X state=%d\n", up->uc_tcb, up->uc_tcb->t_state);
				up->uc_rsize = 0;
			}

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

			/* if any data was read, return even if the
			 * full read request was not satisfied
			 */
			if (incount != u.u_count)
				return;
		} else {                

			/* if no data left and foreign FIN rcvd, just
			   return, otherwise make work request and wait
			   for more data */

			if (tp->fin_rcvd && rcv_empty(tp))       
				break;

        		w_alloc(IURECV, 0, tp, NULL);
			sleep(up, PZERO + 1);
		}

	/* return outstanding status only if no data was xfered */

	if (!u.u_error && up->uc_state && incount == u.u_count)          
		u.u_error = ENETSTAT;
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
	struct {
		struct mbuf *top;
	} nn;

	if ((tp = up->uc_tcb) == NULL) {                    
		u.u_error = ENETERR;
		return;
	}

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

	if (!u.u_error && up->uc_state)         /* outstanding status */
		u.u_error = ENETSTAT;
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

	while (up->uc_rsize == 0)
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

	switch (up->uc_flags & URMSK) {

	case UIP:
        	ip_raw(up, top, ulen);
		break;

	case URAW:
		(*up->uc_srcif->if_ifcb->if_raw)(up, top, ulen);
		break;

	default:
		m_freem(top);
		u.u_error = ENETPARM;
	}

	if (!u.u_error && up->uc_state)         /* outstanding status */
		u.u_error = ENETSTAT;
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
	struct netstate nstat;
	struct {
		int optlen;
		char *options;
	} opt;

	tp = up->uc_tcb;

	switch (cmd) {

	case NETGETS:                   /* get net status */
		bcopy((caddr_t)&up->uc_lo, (caddr_t)&nstat, sizeof(struct netstate));
		if (tp != NULL) {
			nstat.n_lport = tp->t_lport;
			nstat.n_fport = tp->t_fport;
		}
		nstat.n_fcon = up->uc_host;
		nstat.n_lcon = up->uc_srcif->if_addr;

		if (copyout((caddr_t)&nstat, cmdp, sizeof(struct netstate)))
			u.u_error = EFAULT;
		else
			up->uc_state = 0;
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

	case NETSETD:                   /* set debugging file */
		if (suser()) {
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
		} else
			u.u_error = EPERM;
		break;

	case NETRESET:                  /* forced tcp reset */
		if (suser()) {
			for (up = contab; up < conNCON; up++)
				if ((up->uc_flags & UTCP) && 
				     up->uc_tcb != NULL &&
				     up->uc_tcb == (struct tcb *)cmdp) {
					w_alloc(INCLEAR, 0, cmdp, NULL);
					return;
				}
			u.u_error = ENETRNG;
		} else
			u.u_error = EPERM;
		break;

	case NETDEBUG:                  /* toggle debugging flag */
		for (up = contab; up < conNCON; up++)
			if ((up->uc_flags & UTCP) && up->uc_tcb != NULL &&
			    up->uc_tcb == (struct tcb *)cmdp) {
				up->uc_flags ^= UDEBUG;
				return;
			}
		u.u_error = ENETRNG;
		break;
	
	case NETTCPOPT:			/* copy ip options to tcb */
		if ((up->uc_flags & UTCP) && tp != NULL && cmdp != NULL) {
			
			if (copyin((caddr_t)cmdp, &opt, sizeof opt)) {
				u.u_error = EFAULT;
				return;
			}

			/* maximum option string is 40 bytes */

			if (opt.optlen > 40) {
				u.u_error = ENETRNG;
				return;
			}

			/* get a buffer to hold the option string */

			if ((m = m_get(1)) == NULL) {
				u.u_error = ENETBUF;
				return;
			}
			m->m_off = MHEAD;
			m->m_len = opt.optlen;

			if (copyin((caddr_t)opt.options, 
				(caddr_t)((int)m + m->m_off), opt.optlen)) {
				m_free(m);
				u.u_error = EFAULT;
				return;	
			}
			tp->t_opts = (char *)((int)m + m->m_off);
			tp->t_optlen = opt.optlen;
		}
		break;

	case NETGINIT:			/* reread the gateway file */
		if (suser())
			if (gatinit() < 0)
				u.u_error = ENETRNG;
		else
			u.u_error = EPERM;
		break;

	default:
		u.u_error = ENETPARM;
	}
}

/*
 * Check if specified connection is unique.
 */
uniqport(up, tp)
register struct ucb *up;
register struct tcb *tp;
{
	register struct ucb *q;

	for (q = contab; q < conNCON; q++) {
		if (q->uc_proc != NULL && q != up &&
		    q->uc_tcb->t_lport == tp->t_lport &&
		    q->uc_tcb->t_fport == tp->t_fport &&
		    q->uc_host.s_addr == up->uc_host.s_addr &&
		    q->uc_srcif->if_addr.s_addr ==
		      up->uc_srcif->if_addr.s_addr) 
			return(FALSE);
	}

	return(TRUE);
}
