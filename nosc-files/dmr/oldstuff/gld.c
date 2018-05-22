#

/*	gould 4800 printer driver   -   11/50 interface
	expects a three word header on each packet
	1.  first word has function to be put in csr.
	2.  seconf word has number of raster lines to space down 
	    after each write.
	3.  third word is the number of bytes to transfer to the gould.

*/
#include "../../h/param.h"
#include "../../h/buf.h"
#include "../../h/conf.h"
#include "../../h/user.h"

struct	{
	int	csr;
	int	count;
	int	bar;
	};

struct devtab gldtab;
struct buf    gldbuf;


int gldline;
int gldbusy;

#define GLDCUT		02
#define GLDEF		0212
#define LPADDR		0164020
#define IENABLE		6
#define BIT89		001400
#define BIT895_0	001477
#define WORKING		040000
#define GLDACK		1
#define GLDNR		0400

gldstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	bp = abp;
	if( bp->b_flags & B_READ )  
		iodone(bp);
	else
	{
		gldwait();
		LPADDR->csr = GLDACK;
		LPADDR->bar = bp->b_addr+2;
		LPADDR->csr = GLDEF;
		bp->b_resid = 0;
		iodone( bp );
		if(++gldline == 72)
		{
			gldline = 0;
			gldwait();
			LPADDR->csr = GLDCUT;
		}
	}
}
gldwait()
{
	int i;
	i = 010000;
	while(i--);
}

gldint()
{
	/*  since interrupts from the gould dont wwk not needed  */
	int i;
	i = 0;
}

gldread(dev)
{
	physio(&gldstrategy, &gldbuf, dev, B_READ);
}

gldwrite(dev)
{
	physio(&gldstrategy, &gldbuf, dev, B_WRITE);
}

gldopen()
{
	if (gldbusy)
		u.u_error = ENXIO;
	else
	{
		while(LPADDR->csr & GLDNR);
		gldline = 0;
		gldbusy = 1;
	}
}

gldclose()
{
	gldbusy = 0;
}

