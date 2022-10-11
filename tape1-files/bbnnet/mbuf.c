#include "../h/param.h"
#include "../h/pte.h"
#include "../h/cmap.h"
#include "../h/map.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/tcp.h"
#include "../bbnnet/ip.h"
#include "../h/vm.h"

/*
 * Mbuffer utility routines
 *
 * These routines allocate and free small mbufs from/to previously
 * allocated page frames on the freelist.
 */

/*
 * Simple mbuf allocator 
 */
struct mbuf *m_get(type)            
int type;
{
	register i,j;
	register struct pfree *p;
	register struct mbuf *m, *n;
	int s;

	if (--netcb.n_bufs <= 0) {
		netcb.n_bufs = 0;
		if (type == 0 || !m_expand()) {      /* no free bufs */
			netstat.m_drops++;
			return(NULL); 		 
		}
	}

	s = spl5();
	j = netcb.n_free;
	p = freetab+j;                   
	n = pftom(j);
	m = n + p->p_off;
#ifdef MDEBUG
	printf("**mget: page %d buf %d addr=%X\n", j, p->p_off, m);
#endif
	m->m_off = 0;           /* mark mbuf as allocated */
	m->m_act = NULL;               
	m->m_next = NULL;                

	if (--p->p_free != 0) {  /* next free mbuf in this page */

		for (i=0, j=p->p_off; i<NMBPG-1; i++) {                   
			j = (j+1) & (NMBPG-1);
			if ((n+j)->m_off == 0xff) {                   
				p->p_off = j;                   
				splx(s);
				return(m);                      
			}                                       
		}
		panic("m_get: bad freelist");                   

	} else {                        /* look for next free page */

		i = p->p_next;
		while (i != j) {
			if ((freetab+i)->p_free != 0) {
				netcb.n_free = i;
				break;
			}
			i = (freetab+i)->p_next;
		}
		splx(s);
		if (i == j)
			panic("m_get: no free");
		else
			return(m);
	} 
}

/*
 * Simple mbuf free 
 */
struct mbuf *m_free(m)          
register struct mbuf *m;                /* return ->next mbuf in chain */
{
	register struct pfree *p;
	register i;

	if (m == NULL)
		return(NULL);
	if (m->m_off == 0xff)
		panic("m_free: already free");
	m->m_off = 0xff;        /* mark mbuf as free */
	i = mtopf(m);           /* index of mbuf's page frame in freelist */
	p = freetab + i;
	p->p_off = mtoff(m);
#ifdef MDEBUG
	printf("**mfree: page %d buf %d addr=%X\n", i, p->p_off,m);
#endif
	if (++p->p_free >= NMBPG) {
		if (p->p_free > NMBPG)
        		panic("m_free: bad freelist");
		m_insert(i,p);  /* insert free page entry at top of freelist */
	}
	netcb.n_bufs++;
	return(m->m_next);
}

/*
 * Free mbuf chain headed by m 
 */
m_freem(m)                      
register struct mbuf *m;                /* returns # mbufs freed */
{
	register struct pfree *p;
	register struct mbuf *n;
	register i, j;

	if (m == NULL)
		return(0);

	j = 0;
	do {
		n = m->m_next;
		if (m->m_off == 0xff)                                   
			panic("m_free: already free");                  
		m->m_off = 0xff;        /* mark mbuf as free */         
		i = mtopf(m);                                         
		p = freetab + i;                                 
		p->p_off = mtoff(m);                            
#ifdef MDEBUG
	printf("**mfreem: page %d buf %d addr=%X\n", i, p->p_off,m);
#endif
        	if (++p->p_free >= NMBPG) {                                   
        		if (p->p_free > NMBPG)                               
                		panic("m_free: bad freelist");                
        		m_insert(i,p);  /* insert free entry at top of list */
        	}                                                             
		netcb.n_bufs++; j++;                                          
		m = n;
	} while(m != NULL);                                                   
	return(j);                                                            
}                                                                             
                                                                              
/*
 * Put a free page entry at the top of the freelist 
 */
m_insert(i, p)          
register i;       
register struct pfree *p;
{                                                                             
	register short top, j;

	top = netcb.n_free;     /* top of freelist */                         
	                                                                      
	/* make sure entry isn't already in the free page area (top)        
	   of the freelist. if it isn't move it there */
                                                                              
	if (i != top && (freetab+p->p_prev)->p_free != NMBPG &&  
		(freetab+p->p_next)->p_free != NMBPG) {	/* need to move */    
                                                                              
		/* unlink the entry */                                        
                                                                              
		(freetab+p->p_prev)->p_next = p->p_next;                        
		(freetab+p->p_next)->p_prev = p->p_prev;                        
                                                                              
		/* link right after top element */                            
                                                                              
		p->p_prev = top;                                              
		p->p_next = j = (freetab+top)->p_next;                          
		(freetab+top)->p_next = i;
		(freetab+j)->p_prev = i;                  
	                                                                      
        /* entry would be top */

	} else if ((freetab+p->p_next)->p_free == NMBPG)          
		netcb.n_free = i;
}

/*
 * Adjust length of mbuf chain 
 */
struct mbuf *m_adj(mp, len)             
struct mbuf *mp;
register len;
{
	register struct mbuf *m, *n;

	if ((m = mp) == NULL)
		return;

	if (len >= 0) {                 /* adjust from top of msg chain */
		while (m != NULL && len > 0) {
			if (m->m_len <= len) {          /* free this mbuf */
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {                        /* adjust mbuf */
				m->m_len -= len;
				m->m_off += len;
				break;
			}
		}

	} else {                        /* adjust from bottom of msg chain */
		len = -len;
		while (len > 0 && m->m_len != 0) {

			/* find end of chain */

			while (m != NULL && m->m_len != 0) {             
				n = m;
				m = m->m_next;
			}
			if (n->m_len <= len) {          /* last mbuf */
				len -= n->m_len;
				n->m_len = 0;
				m = mp;
			} else {                        /* adjust length */
				n->m_len -= len;
				break;
			}
		}
	}
}

/*
 * Initialize network buffer management system (called from netmain) 
 */
mbufinit()                      
{
	register struct mbuf *m;
	register i;


	m = (struct mbuf *)&netutl[0];  /* ->start of buffer virt mem */

	/* allocate the zeroth page of buffers,  this page remains
	   allocated for utility use, since malloc will not return
	   a zero index.  */

	vmemall(&Netmap[0], 2, netcb.n_proc, CSYS);
	vmaccess(&Netmap[0], m, 2);

	for (i=0; i < NMBPG; i++)       /* mark all mbufs in the page free */
		(m++)->m_off = 0xff;

	/* initialize freelist entry zero as top of list */

	freetab->p_off = 0;
	freetab->p_next = 0;
	freetab->p_prev = 0;
	freetab->p_free = 8;
	netcb.n_free = 0;

	/* now allocate three more pages for minimum network allocation */

	pg_alloc(3);
	netcb.n_pages = 4;
	netcb.n_bufs = 32;
	netcb.n_lowat = 16;
	netcb.n_hiwat = 32;
}

/*
mbufclr()                       
{
	register i;

	for (i=netcb.n_free; (freetab+i)->p_next != netcb.n_free; i=(freetab+i)->p_next) {
		memfree(&Netmap[i<<1], 2, TRUE);
		rmfree(netmap, 1, i);
	}

	memfree(&Netmap[0], 2, TRUE);
	netcb.n_pages = 0;
	netcb.n_bufs = 0;
	netcb.n_free = 0;
	freetab->p_prev = 0;
	freetab->p_next = 0;
	freetab->p_free = 0;
	freetab->p_off = 0;
}
*/

/*
 * Free page manipulation routines
 *
 * These routines steal page frames from the system cmap, and
 * maintain them on a circular freelist.  These pages are sub-
 * divided into message buffers (mbufs) which are allocated/freed
 * by the network routines.
 */

/*
 * Allocate page frames and add to freelist 
 */
pg_alloc(n)              
register int n;
{
	register i, j, k;     
	register struct mbuf *m;
	int bufs;

	k = n << 1;
	if ((i = rmalloc(netmap, n)) == 0)      /* find n unallocated entries */
		return(0);

	j = i<<1;       /* #phys pages in a free page */
	m = pftom(i);   /* virtual address of first new page */

	/* allocate free pages in the network map (netutl) */
	/* (vmalloc means this may block for pageout daemon) */
	/* and validate the mapping for these physical pages */
	/* N.B., these pages are LOCKED */

	vmemall(&Netmap[j], k, netcb.n_proc, CSYS);
	vmaccess(&Netmap[j], (caddr_t)m, k);

	/* mark all mbufs in pages as free */

	bufs = n << 3;
	for (j=0; j < bufs; j++)
		m++->m_off = 0xff;

	/* link together allocated pages for freelist */

	for (j=i; j < n+i; j++) {
	        (freetab+j)->p_off = 0;
		(freetab+j)->p_free = 8;
		if (n > 1) {
        		(freetab+j)->p_next = j+1;             
        		(freetab+j)->p_prev = j-1;                  
		}
	}

	/* insert page after the top of the free list */

	(freetab+(--j))->p_next = k = (freetab+netcb.n_free)->p_next;
	(freetab+i)->p_prev = netcb.n_free;
	(freetab+netcb.n_free)->p_next = i;
	(freetab+k)->p_prev = j;

	netcb.n_pages += n;
	netcb.n_bufs += bufs;
	return(1);
}

/*
 * Release page at ndx 
 */
pg_relse(ndx)                   
register ndx;
{
	register i,j;

	if (ndx == 0)                           /* page zero cannot be freed */
		return;

	memfree(&Netmap[ndx<<1], 2, TRUE);      /* free page frames */
	rmfree(netmap, 1, ndx);                 /* return to map */

	/* remove entry from free list */

	i = (freetab+ndx)->p_prev;
	j = (freetab+ndx)->p_next;
	(freetab+i)->p_next = j;
	(freetab+j)->p_prev = i;
	if (netcb.n_free == ndx)                /* freeing top of list */
		netcb.n_free = j;

	netcb.n_pages--;
	netcb.n_bufs -= NMBPG;
}

/*
 * Expand the free list 
 */
m_expand()                      
{
	register i;
	register struct ipq *fp;
	register struct ip *p, *q;
	register struct mbuf *m, *n;
	register struct tcb *tp;
	int need, needp, needs;

	needs = need = netcb.n_hiwat - netcb.n_bufs;    /* #bufs to add */
	needp = need >> 3;                              /* #pages to add */

	/* first try to allocate needed pages contiguously from system */

	if (pg_alloc(needp) == 0) {

		/* now try to allocate singly from the system */

		for (i=0; i < needp; i++, need-=8)

			if (needp == 1 || pg_alloc(1) == 0) {

			/* can't allocate from system, now try to steal
			   back pages from end of fragment reassembly and 
			   unacked message queues */

				fp = netcb.n_ip_tail;           
				while (need > 0 && fp) {
					q = fp->iqx.ip_next;    

					/* free mbufs on reass q */

					while (q != (struct ip *)fp) {        
						p = q->ip_next;                
						need -= m_freem(dtom(q));      
						q = p;                
					}                                     
					ip_freef(fp);	/* free header */              
					fp = netcb.n_ip_tail;
				}

				tp = netcb.n_tcb_tail;          /* ->tcbs */
				while (need > 0 && tp != NULL) {

					m = tp->t_rcv_unack;
					while (m != NULL) {
						n = m->m_act;
						need -= m_freem(m);
						m = n;
					}
					tp->t_rcv_unack = NULL;
					tp = tp->t_tcb_prev;
				}                       
				break;
			}
		return(need < needs);
	}
	return(1);
}

/*
 * Return excess free pages to system 
 */
m_relse()                       
{
	register free, i, j;

	free = (netcb.n_bufs - netcb.n_hiwat) >> 3;    /* # excess free pages */
	for (i=(freetab+netcb.n_free)->p_next; free > 0 && 
					      i != netcb.n_free &&
					      (freetab+i)->p_free == NMBPG; i=j) {
		j = (freetab+i)->p_next;
		if (i > 0) {
          		pg_relse(i);
	        	free--;
		}
	}
}

/*
 * Convert mbuf virtual to physical addr for uballoc 
 */
mtophys(m)              
register struct mbuf *m;
{
	register i;
	register unsigned long addr;
	register struct pte *pte;

	i = (((int)m & ~PGOFSET) - (int)netutl) >> PGSHIFT;
	pte = &Netmap[i];
	addr = (pte->pg_pfnum << PGSHIFT) | ((int)m & PGOFSET);
	return(addr);
}

