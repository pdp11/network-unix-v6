#include "mh.h"

m_delete(key)
char *key;
{
	register struct node *np, *npprev;

	m_getdefs();
	for(np = &m_defs; npprev = np; )  {
		np = np->n_next;
		if(uleq(np->n_name, key)) {
			npprev->n_next = np->n_next;
			cfree(np->n_name);
			cfree(np->n_field);
			free(np);
			def_flags =| DEFMOD;
			return(0);
		}
	}
	return(1);
}
