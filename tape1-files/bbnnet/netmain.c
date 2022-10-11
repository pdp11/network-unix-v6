#include "../h/param.h"
#include "../h/inode.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"
#include "../h/buf.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

extern struct user u;
char netbugf[] = "/etc/net/tcpdebug";
char gatefile[] = "/etc/net/gateway.bin";

/*
 * This is the mainline of the network input process.  First, we
 * fork off as a separate process within the kernel, initialize the
 * net buffer manager, local net interfaces, and timers.  Then, we
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
		gatinit();	/* read gateway file and init table */

		/* initialize all local net i/fs */

		for (lp=ifcb; lp < &ifcb[nifcb]; lp++) { 
			(*lp->if_init)(lp);
		}

		/* start timers */

		tcp_timeo();
		ip_timeo();

		for (;;) {      /* body of input process */

			/*
			 * reset local i/f if necessary, otherwise
			 * just handle net input
			 */
			for (lp=ifcb; lp < &ifcb[nifcb]; lp++) {
				if (lp->if_needinit || lp->if_error) 
					netreset(lp);
				else if (lp->if_avail)
					(*lp->if_rcv)(lp);
			
			/* call TCP processor to handle any user or
			   timer events. */

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
			sleep((caddr_t)&ifcb[0], PZERO-15); 
		}
	}
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

	h_reset(ip->if_net);
	splx(s);
	printf("dev %d/%d: netreset\n", major(ip->if_dev), minor(ip->if_dev));
	
	/* 
	 * now call net driver specific init routine to reinit
	 */
	(*ip->if_init)(ip);
}

/*
 * Read the binary format gateway file into the gateway table.  The file
 * contains gateway structure entries produced from the ASCII gateway
 * table.  Called from netmain or netioctl, returns number of entries
 * successfully read or -1 if error
 */
gatinit()
{
	register i;
	register struct inode *ip;
	register struct ifcb *lp;
	register struct gway *gp;
	struct gway gatent;
	net_t gatnet;
	int schar();

	/* set up to read the gateway file */

	u.u_dirp = gatefile;
	if ((ip = namei(schar, 0)) == NULL) 
		return(-1);

	if ((ip->i_mode & IFMT) != IFREG) {
		iput(ip);
		printf("bad format gateway file: %s\n", gatefile);
		return(-1);
	}
	u.u_offset = 0;
	u.u_segflg = 1;
	u.u_error = 0;

	/* read entries from the file into the table */

	for (gp=gateway, i=NGATE; i > 0; i--, gp++) {
		u.u_base = (caddr_t)&gatent;
		u.u_count = sizeof gatent;
		readi(ip);
		if (u.u_error != 0) {
			printf("error %d reading gateway file: %s\n",
				u.u_error, gatefile);
			goto out;
		}
		if (u.u_count == sizeof gatent) 	/* check EOF */
			break;
		if (u.u_count != 0 || !gatent.g_flags & GWFORMAT) {
			printf("invalid gateway file entry, read %d entries\n",
				NGATE-i+1);
			goto out;
		}

		/* look for i/f corresponding to gateway local net addr */

		gatnet = iptonet(gatent.g_local);
		for (lp=ifcb; lp < &ifcb[nifcb]; lp++)
			if (lp->if_net == gatnet) {
				gatent.g_ifcb = lp;
				goto gotif;
			}
		printf("invalid gateway addr %X, entry %d ignored\n",
			gatent.g_local.s_addr, NGATE-i+1);
		gp--;
gotif:
		*gp = gatent;
	}
	if (u.u_offset < ip->i_size)
		tablefull("gateway");
out:
	gatewayNGATE = gp;
	prele(ip);
	iput(ip);
	return(NGATE-i+1);
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
