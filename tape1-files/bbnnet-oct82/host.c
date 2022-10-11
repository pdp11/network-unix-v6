#include "../h/param.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/imp.h"

/*
 * Find an entry in the host table 
 */
struct host *h_find(np)                 
struct socket *np;
{
	register struct host *h;

	for (h = host; h < hostNHOST; h++) 
	        if (h->h_addr.s_addr == np->s_addr)    
			return(h);
	return(NULL);
}

/*
 * Make an entry in the host table
 */
struct host *h_make(np, temp)                 
struct socket *np;
int temp;
{
	register struct host *h;
	struct socket n;

	/* zero lh byte for ARPA-like net addrs */

	n.s_addr = addr_1822(*np);	

	/* For non-temp entries, look for existing entry.  We already know
	   there is no entry when we ask for a temp. */

	if (!temp) {

		/* look for existing host entry */

		h = h_find(&n);

		/* got one, if temporary entry make perm, otherwise 
		   just bump refct */

		if (h != NULL) {                
			if (h->h_stat&HTEMP) 
				h->h_stat &= ~HTEMP;
			else
				h->h_refct++;
			return(h);
		} 
	}

	for (h = host; h < hostNHOST; h++)  

		/* find an unused entry and mark host up */

		if (h->h_refct == 0) {
			h->h_addr.s_addr = n.s_addr;
			h->h_refct++;
			h->h_rfnm = 0;
			h->h_stat = HUP + (temp? HTEMP : 0);
			h->h_outq = NULL;
			return(h);
		}

	return(NULL);
}

/*
 * Free an entry in the host table: decrement reference count and delete if
 * zero
 */
h_free(hp)                               
register struct host *hp;
{
	register struct mbuf *m;

	if (hp == NULL)
		return;

	if (--hp->h_refct == 0) {       /* decrement ref ct */
		hp->h_addr.s_addr = 0;
		hp->h_stat = 0;
		hp->h_rfnm= 0;		
		while ((m = hp->h_outq) != NULL) {
			hp->h_outq = m->m_act;
			m_freem(m);
		}
	}
}

/*
 * Reset host status for a given network
 */
h_reset(net)	    
net_t net;
{
	register struct host *hp;
	register struct mbuf *m;

	for (hp = host; hp < hostNHOST; hp++) {
		if (iptonet(hp->h_addr) == net) {
        		hp->h_rfnm = 0;
        		hp->h_stat = 0;
			while ((m = hp->h_outq) != NULL) {
				hp->h_outq = m->m_act;
				m_freem(m);
			}
		}
	}
}

#ifdef old

unsigned short ntohs(ls)
register unsigned short ls;
{ 
	register unsigned short a;

	a = (ls & 0xff) << 8;
	a |= (ls & 0xff00) >>8;
	return(a);
}

unsigned long ntohl(lx)
register unsigned long lx;
{
	register unsigned long a;

	a = (lx & 0xff) << 24;
	a |= (lx & 0xff00) << 8;
	a |= (lx & 0xff0000) >> 8;
	a |= (lx & 0xff000000) >> 24;
	return(a);
}

#endif old

