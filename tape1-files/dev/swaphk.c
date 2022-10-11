#include "../h/param.h"
#include "../h/conf.h"
/*
 * Single rk07 configuration
 *	root on hk00
 *	paging on hk01
 */
dev_t	rootdev	= makedev(3, 0);
dev_t	pipedev	= makedev(3, 0);
dev_t	argdev	= makedev(3, 1);
dev_t	dumpdev = makedev(3, 1);
long	dumplo	= 10032 - 2 * 2048;		/* not enough... */

/*
 * Nswap is the basic number of blocks of swap per
 * swap device, and is multiplied by nswdev after
 * nswdev is determined at boot.
 */
int	nswap = 10032;

struct	swdevt swdevt[] =
{
	makedev(3, 1),	1,		/* hk0b */
	0,		0,
};
