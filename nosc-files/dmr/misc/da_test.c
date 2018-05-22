#

#include	"/usr/sys/h/param.h"
#include	"/usr/sys/h/buf.h"
#include	"/usr/sys/h/conf.h"
#include	"/usr/sys/h/user.h"

struct buf	datbuf;

struct buf	*datbp;

int	datcount;

int	daicount;

int	daitab[16][4];

int	daitabx	0;

struct
{
	int	da_drwc;
	int	da_drba;
	int	da_drst;
	int	da_drdb;
};

#define	da_addr	0172410

#define	daebits	060000		/* nex, att */

#define	daxmem	060		/* dr11-b extended memory bits */

struct
{
	char	lo_byte;
};

struct
{
	int	kwcs;
	int	kwcb;
	int	kwct;
};

#define	kw_addr	0172540


datstrategy(bufp)
struct buf	*bufp;
{
	register struct buf	*bp;
	register int	ctwatch;

	bp = bufp;
	datbp = bp;
	datcount = fuword(u.u_base);
	bp->b_addr =+ 2;
	bp->b_wcount++;		/* is negative, so incrementing causes fewer
				   words to be xmitted */
	datstart(bp);
/*
	spl1();
*/
	return;		/*#debug*/
	while ( datcount >= 0 )
		while ( ctwatch = da_addr->da_drwc )
		{
			kw_addr->kwcb = 8;
			kw_addr->kwcs = 1;
			while ( kw_addr->kwcs.lo_byte >= 0 );
			if ( ctwatch == da_addr->da_drwc )
			{
				da_addr->da_drst =& ~0100;
				bp->b_flags =| B_ERROR;
				bp->b_error = EIO;
				iodone(bp);
				return;
			}
		}
}

datstart(bufp)
struct buf	*bufp;
{
	register struct buf	*bp;

	bp = bufp;
	if ( datcount-- == 0 )		/* done? */
	{
		da_addr->da_drst = 010;
		iodone(bp);
		return;
	}
	daicount = 0;
	da_addr->da_drwc = bp->b_wcount;
	da_addr->da_drba = bp->b_addr;
	da_addr->da_drst = ( ( bp->b_xmem & 03 ) << 4 ) | 0111;
		/* x-mem and int enable and go */
}

datintr()
{
	register struct buf	*bp;
	register int	*tp,*ip;

	bp = datbp;
	tp = &daitab[daitabx][0];
	daitabx++;
	daitabx =& 017;
	ip = da_addr;
	*tp++ = *ip++;
	*tp++ = *ip++;
	*tp++ = *ip;
	if ( da_addr->da_drst < 0 )	/* error? */
	{
		if ( ( (da_addr->da_drst & daebits) | da_addr->da_drba) == 0 )
				/* no error bits set and buf addr = 0? */
		{
			daicount++;
			da_addr->da_drba = 0;	/* resets int condition */
			da_addr->da_drst =
				( ( (da_addr->da_drst & daxmem) + 020)
				  & daxmem) | 0111;	/* inc x mem and
							   re-init */
			*tp = 0;
			return;		/* that's all */
		}
		bp->b_flags =| B_ERROR;
		bp->b_error = EIO;
		da_addr->da_drst =& ~0100;
		iodone(bp);
		*tp = 1;
		return;
	}
	datstart(bp);
	*tp = 2;
}

datwrite(dev)
{
	physio(datstrategy,&datbuf,dev,B_WRITE);
}

datsgtty(dev,vec)
int	*vec;
{
	register int	*v;

	v = vec;
	if ( v != 0 )	/* gtty */
	{
		*v++ = da_addr->da_drwc;
		*v++ = da_addr->da_drba;
		*v++ = da_addr->da_drst;
		return;
	}
	da_addr->da_drst = u.u_arg[0];
}

