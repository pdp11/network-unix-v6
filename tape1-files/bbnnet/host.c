#include "../h/param.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"

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
struct host *h_make(np)                 
struct socket *np;
{
	register struct host *h;

	/* look for existing host entry */

	h = h_find(np);

	if (h != NULL) {                /* got one, just bump refct */
		h->h_refct++;
		return(h);

	} else {                        /* have to make one */

		for (h = host+1; h < hostNHOST; h++)  

			/* find an unused entry and mark host up */

			if (h->h_refct == 0) {
				h->h_addr.s_addr = np->s_addr;
				h->h_refct++;
				h->h_rfnm = 0;
				h->h_stat = HUP;
				h->h_outq = NULL;
				return(h);
			}

		return(NULL);
	}
}

/*
 * Free an entry in the host table: decrement reference count and delete if
 * zero
 */
h_free(np)                               
struct socket *np;
{
	register struct host *h;
	register struct mbuf *m;

	if ((h = h_find(np)) == NULL)  /* find the entry */
		return;

	if (--h->h_refct == 0) {       /* decrement ref ct */
		h->h_addr.s_addr = 0;
		h->h_stat = 0;
		h->h_rfnm = 0;		
		while ((m = h->h_outq) != NULL) {
			h->h_outq = m->m_act;
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

#ifndef mbb

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

#else

long long_mbb(lx)
holey_long lx;
{
	long x;
	register int i;
	register char *cp;

	cp = (char *)&lx;
	x = 0;
	for (i=0; i < 4; i++) {
		x <<= 8;
		x |= cp[i] & 0377;
	}
	return(x);
}

holey_long long_1822(lx)
long lx;
{
	holey_long x;
	register int i;
	register char *cp;

	cp = (char *)&x;
	x = 0;
	for (i = 3; i >= 0; i--) {
		cp[i] = lx & 0377;
		lx >>= 8;
	}
	return(x);
}

#endif mbb
