#include "acc.h"
#ifdef NACC > 0

/*
 * ACC LH/DH IMP interface
 *
 */

#include "../h/param.h"
#include "../h/systm.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/imp.h"
#include "../bbnnet/ifcb.h"
#include "../h/map.h"
#include "../h/pte.h"
#include "../h/buf.h"
#include "../h/ubareg.h"
#include "../h/ubavar.h"
#include "../h/conf.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/accreg.h"


int     accprobe(), accattach(), acc_iint(), acc_oint();
struct  uba_device *accinfo[NACC];
u_short accstd[] = { 0 };
struct  uba_driver accdriver =
	{ accprobe, 0, accattach, 0, accstd, "acc", accinfo };

struct mbuf netflush;		/* spare buffer for doing flushes */

/*
 * Cause the ACC to interrupt.
 */
accprobe(reg)
	caddr_t reg;
{
	register int br, cvec;
	register struct impregs *addr = (struct impregs *)reg;

#ifdef lint
	br = 0; cvec = br; br = cvec;
#endif

	/*
	 * Reset transmitter and receiver and wait for things to settle.
	 */
	addr->istat = ireset;
	DELAY(100000);
	addr->ostat = oreset;
	DELAY(100000);

	/*
	 * Cause transmitter interrupt by doing a null DMA.
	 */
	addr->ostat = obusback;
	DELAY(100000);
	addr->owcnt = 0;
	addr->ostat = oienab | ogo;
	DELAY(100000);
	addr->ostat = 0;
	if (cvec && cvec != 0x200)
		cvec -= 4;              /* transmit -> receive */
	return (1);
}

accattach(ui)
	struct uba_device *ui;
{

	/* no local state to set up */
}

/*
 * Initialize the imp interface.
 */
acc_init(ip)
register struct ifcb *ip;
{	
	register struct impregs *addr;
	register struct uba_device *ui;
	register struct mbuf *m;
	int s, unit, ubano;
	extern lbolt;

	unit = minor(ip->if_dev);
	if (unit >= NACC || (ui = accinfo[unit]) == NULL || ui->ui_alive == 0) {
		printf("acc%d not alive\n", unit);
		return;
	}
	addr = (struct impregs *)ui->ui_addr;

	/*
	 * Do a hard reset first time thru, then try to set host ready
	 * relay: keep closing relay until host ready sets, use timeout
	 * to reschedule until host ready finally sets.  This avoids
	 * hanging up the entire net process while waiting for an errant
	 * interface to set.
	 */
	if (!ip->if_flag&IMPSETTING) {
		/*
		 * Reset the imp interface.
		 */
		s = spl5();
		addr->istat = ireset;
		addr->ostat = oreset;
		addr->ostat = obusback;	     	/* reset host master ready */
		addr->ostat = 0;
		splx(s);
		ip->if_flag |= IMPSETTING;
		addr->istat = ihmasrdy;		/* close the relay */
		sleep((caddr_t)&lbolt, PZERO);
	}
	while (!(addr->istat&ihstrdy) || (addr->istat&(imrdyerr|imntrdy))) {
		/*
		 * Keep turning imrdyerr off.
		 */
		addr->istat = ihmasrdy;
		timeout((caddr_t)acc_init, (caddr_t)ip, HZ);
		return;
	}

	/*
	 * host ready now set, set up uba mappings, flags, queues, put
	 * out the first read, and send three no-ops
	 */
	ip->if_flag &= ~IMPSETTING;
	ip->if_flag |= IMPTRYING;	/* ignore three no-ops from imp */
	ip->if_error = FALSE;
	ip->if_active = FALSE;
	ip->if_flush = FALSE;
	ip->if_blocked = FALSE;

	/*
	 * free uba map regs and info if necessary, and reinit 
	 */
	ubano = ui->ui_ubanum;
	if (ip->if_oaddr)
		ubarelse(ubano, &ip->if_oaddr);
	if (ip->if_iaddr)
		ubarelse(ubano, &ip->if_iaddr);

	ip->if_oaddr = uballoc(ubano, mtophys(netutl), MLEN, 0);
	ip->if_iaddr = uballoc(ubano, mtophys(netutl), MLEN, 0);

	/*
	 * get a buffer to start a read operation 
	 */
	if ((m = m_get(0)) == NULL) {
		printf("acc%d: cannot initialize\n", unit);
		return;
	}
	m->m_off = MHEAD;
	ip->if_inq_msg = m;
	ip->if_inq_cur = m;
	/*
	 * Try to send 3 no-ops to IMP.  If successful, put out a read
	 * and declare i/f available.
	 */
	if (imp_noops(ip)) {
		acc_read(ip, (caddr_t)((int)m + m->m_off), MLEN);
		ip->if_needinit = FALSE;
		ip->if_avail = TRUE;
	}
}

/*
 * Start an output operation on an mbuf.
 */
acc_output(ip)
register struct ifcb *ip;
{
	register struct mbuf *m;
	register struct imp *l;
	register struct impregs *addr;
	register struct uba_device *ui;
        int len, start;
	u_short uaddr;

	if ((ui = accinfo[minor(ip->if_dev)]) == NULL)
		return;
	addr = (struct impregs *)ui->ui_addr;

	start = FALSE;
	/*
	 * If output isn't active, attempt to
	 * start sending a new packet.
	 */
	if (!ip->if_active) {
		if ((m = ip->if_outq_hd) == NULL)
			/*
			 * Nothing left to send.
			 */
			return;
		start = TRUE;
		ip->if_active = TRUE;		/* set myself active */
		ip->if_outq_hd = m->m_act;	/* -> next packet chain */
		ip->if_outq_cur = m;		/* -> current mbuf */
		l = (struct imp *)((int)m + m->m_off);  /* ->1822 leader */
		ip->if_olen = l->i_mlen;	/* length to send */
		l->i_mlen = short_to_net(l->i_mlen) << 3;
	} else {
		m = ip->if_outq_cur;		/* -> next mbuf to send */
		l = (struct imp *)((int)m + m->m_off);
	}

	/*
	 * Map packet to be sent to UNIBUS.
	 */
	uaddr = ubaremap(ui->ui_ubanum, ip->if_oaddr, (caddr_t)l);

	/*
	 * Make sure length of type 0 messages comes from mbuf.  All others
	 * must be 80 bits.
	 */
	if (!start || l->i_type == 0)
		len = MIN(ip->if_olen, m->m_len);
	else
		ip->if_olen = len = 10;

#ifdef IMPDEBUG
	printf("acc%d: ", minor(ip->if_dev));
	imp_prt("SND", l);
	printf("addr= %x ubaddr= %x len= %x\n", l, uaddr, len);
#endif

	/*
	 * Stuff UNIBUS address, word count,
	 * and output status register.
	 * Adjust remaining packet length.
	 */
	addr->spo = (int)uaddr;
	addr->owcnt = -((len + 1) >> 1);
	ip->if_olen -= len;
	if (ip->if_olen <= 0)			/* end of message */
		addr->ostat = ((short) oienab + oenlbit +
                                     ((uaddr&0x30000)>>12) + ogo);
	else
		addr->ostat = ((short) oienab +
                                     ((uaddr&0x30000)>>12) + ogo);
}

/*
 * Read from the imp into a data area.
 */
acc_read(ip, daddr, size)
register struct ifcb *ip;
caddr_t daddr;
int size;
{
	register struct impregs *addr;
	register struct uba_device *ui;
        u_short uaddr;
	int s;

	if ((ui = accinfo[minor(ip->if_dev)]) == NULL)
		return;
	addr = (struct impregs *)ui->ui_addr;

	/*
	 * Remap input buffer; reset device
	 * to receive another packet.
	 */
	uaddr = ubaremap(ui->ui_ubanum, ip->if_iaddr, (caddr_t)daddr);
#ifdef IMPDEBUG
	printf("acc%d: RCV addr= %x ubaddr= %x len= %x\n", minor(ip->if_dev), 
			addr, uaddr, size);
#endif
	s = spl5();
	addr->spi = (int)(uaddr);
	addr->iwcnt = - (size >> 1);
	addr->istat = (ihmasrdy|iienab|iwren| ((uaddr & 0x30000)>> 12) | igo);
	splx(s);
}

/*
 * IMP output interrupt handler.
 */
acc_oint(unit)
int unit;
{
	register struct ifcb *ip;
	register struct impregs *addr;
	register struct uba_device *ui;
	int errbits = 0;

	ui = accinfo[unit];
	addr = (struct impregs *)ui->ui_addr;

	/* 
	 * Find ifcb for interrupting device.
	 */
	for (ip = ifcb; ip < &ifcb[nifcb]; ip++)
		if (major(ip->if_dev) == ACCDEV && minor(ip->if_dev) == unit)
			goto gotodev;
	printf("acc%d: oint: can't find ifcb\n", unit);
	return;

gotodev:
	/*
	 * Ignore unexpected errors.
	 */
	if (!ip->if_active) {
		printf("acc%d: phantom output intr ostat=%b olen=%d m=%X\n",
		    unit, addr->ostat, ACC_OBITS, ip->if_olen, ip->if_outq_cur);
		return;
	}

#ifdef IMPDEBUG
	printf("acc%d: write done\n", unit);
	errbits = addr->ostat & (ocomperr|otimout|omrdyerr|obusback|ogo);
	if (errbits || (~addr->ostat & (odevrdy|oienab)))
		printf("acc%d: err ostat=%b\n", unit, addr->ostat, ACC_OBITS);
#endif

	if (addr->ostat & otimout) 
		printf("acc%d: timeout", unit);

	/*
	 * If more to send, free current buf and get next, otherwise
	 * reset active flag to start next msg in imp_output
	 */
	if ((ip->if_outq_cur = m_free(ip->if_outq_cur)) == NULL ||
	    ip->if_olen <= 0) {
		ip->if_active = FALSE;
		if (ip->if_outq_cur != NULL)
			m_freem(ip->if_outq_cur);
	}

	/*
	 * fire up next  output
	 */
	acc_output(ip);	
}

/*
 * IMP input interrupt handler
 */
acc_iint(unit)
int unit;
{
	register struct ifcb *ip;
	register struct imp *l;
    	register struct mbuf *m, *n;
	register struct impregs *addr;
	register struct uba_device *ui;
	int trying;

	/* 
	 * Find ifcb for interrupting device.
	 */
	for (ip = ifcb; ip < &ifcb[nifcb]; ip++)
		if (major(ip->if_dev) == ACCDEV && minor(ip->if_dev) == unit)
			goto gotidev;
	printf("acc%d: iint: can't find ifcb\n", unit);
	return;

gotidev:
	if ((m = ip->if_inq_cur) == NULL)
		return;
	ui = accinfo[unit];
	addr = (struct impregs *)ui->ui_addr;

#ifdef IMPDEBUG
	printf("acc%d: read done ", unit);
	l = (struct imp *)((int)m +  m->m_off);
	imp_prt("RCV", l);
	printf("\n");
#endif

	trying = ip->if_flag & IMPTRYING;
	if (addr->istat & iendmsg) {
		ip->if_flag |= IMPENDMSG;
		if (trying)
			ip->if_flag = (ip->if_flag & ~IMPTRYING) | --trying;
	}
	ip->if_error = (addr->istat & (icomperr|itimout|imntrdy|imrdyerr|igo)) 
			? TRUE : FALSE;
	if (ip->if_error ||
	    (~addr->istat & (ihstrdy|iwren|idevrdy|iienab|ihmasrdy))) {
		printf("acc%d: input error csri=%b\n", unit,
		    addr->istat, ACC_IBITS);
		/*
		 * ignore errors for init no-ops
		 */
		if (trying) {
			ip->if_error = FALSE;
			addr->istat = ihmasrdy|iwren|iienab;
			acc_read(ip, (caddr_t)((int)m + m->m_off), MLEN);
			return;
		}
	}

	addr->istat = ihmasrdy|iwren|iienab;

	/*
	 * Get length of message.
	 * Make sure message length does not exceed maximum.
	 * (Could be imp trying to send us garbage.)
	 */
	m->m_len = MLEN + (addr->iwcnt << 1);
	ip->if_ilen += m->m_len;
	if (ip->if_ilen > ip->if_mtu && !ip->if_flush) {
		printf("acc%d: ilen error ilen=%X mtu=%X\n", 
				unit, ip->if_ilen, ip->if_mtu);
		ip->if_error = TRUE;
	}

	if (ip->if_flag & IMPENDMSG) {
		/*
		 * Queue good packet for input processing and awaken net
		 * input process
		 */
		if (!ip->if_flush) {
			if (ip->if_inq_hd != NULL)
				ip->if_inq_tl->m_act = ip->if_inq_msg;
			else
				ip->if_inq_hd = ip->if_inq_msg;
			ip->if_inq_tl = ip->if_inq_msg;
			ip->if_inq_msg = NULL;
			wakeup((caddr_t)&ifcb[0]);	
		}
		
		/*
		 * Get buffer for next packet to be read.
		 */
		if ((n = m_get(0)) != NULL) {
			/*
			 * Got a buffer; fill it in.
			 */
			ip->if_flush = FALSE;
			ip->if_ilen = 0;
			n->m_off = MHEAD;
			ip->if_inq_msg = n;
			ip->if_inq_cur = n;
		} else {
			/*
			 * No more buffers; flush and clear RFNM status for
			 * this i/f
			 */
			ip->if_flush = TRUE;
			ip->if_inq_msg = n = (struct mbuf *)&netflush;
			h_reset(ip->if_net);
			netstat.imp_flushes++;
		}
		ip->if_flag &= ~IMPENDMSG;
	} else
		/*
		 * Not end of packet; if not flushing
		 * try to get more buffers.  If there is
		 * an error or no more buffers free partial message.
		 */
		if (!ip->if_flush)
			if (!ip->if_error && (n = m_get(0)) != NULL) {
				m->m_next = n;
				n->m_off = MHEAD;
				ip->if_inq_cur = n;
			} else {
				m_freem(ip->if_inq_msg);
				ip->if_flush = TRUE;
				n = (struct mbuf *)&netflush;
				ip->if_inq_cur = n;
				ip->if_inq_msg = n;
				h_reset(ip->if_net);
				netstat.imp_flushes++;
			}
	/*
	 * Finally, set up the next read.
	 */
	acc_read(ip, (caddr_t)((int)n + n->m_off), MLEN);
}	

/*
 * Reset driver in case of uba init.
 */
accreset(uban)
int uban;
{
	register i;
	register struct ifcb *ip;
	register struct uba_device *ui;

	/* look for all acc's on selected uba */

	for (i = 0; i < NACC; i++)
		if ((ui = accinfo[i]) != NULL && ui->ui_ubanum == uban) {
			for (ip = ifcb; ip < &ifcb[nifcb]; ip++)
				if (minor(ip->if_dev) == i) 
					netreset(ip);
		}
}

#ifdef IMPDEBUG
imp_prt(s, l)
register char *l;
	char *s;
{

	printf("<%s> ",s);
	prt_byte(l, 12);
	prt_byte(l+12, 20);
	prt_byte(l+32, 20);
}

prt_byte(s, ct)
register char *s;
	int ct;
{
	register i, j, c;

	for (i=0; i<ct; i++) {
		c = *s++;
		for (j=0; j<2 ; j++)
			putchar("0123456789abcdef"[(c>>((1-j)*4))&0xf]);
		putchar(' ');
	}
	putchar('\n');
}
#endif IMPDEBUG
#endif NACC
