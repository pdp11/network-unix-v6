#define	LIBN
#include "netlib.h"

/*
 * hash a network address
 *	return small integer modulo N
 */
hashost(a, n)
netaddr	a;
int	n;
{
	register int i, h;

	h = 0;
	for (i = 0; i < sizeof a._na_b; ++i)
		h += a._na_b[i] & _NABM;
	return h % n;
}
