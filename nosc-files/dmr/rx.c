#
/*
 * RX floppy disk driver
 * Ron Broersma -- Naval Ocean Systems Center, San Diego, California
 */

#include "param.h"
#include "buf.h"
#include "conf.h"
#include "user.h"
#include "seg.h"

#ifndef RXADDR
#define	RXADDR	0177170
#endif not RXADDR
#define NRX	2
#define	NRXBLK	500

#define GO	1
#define FILL	0
#define EMPTY	2
#define WRITE	4
#define READ	6
#define RDSTAT	012
#define RDERR	016
#define IENABLE	0100
#define	TR	0200
#define DRVRDY	0200
#define DONE	040
#define INIT	040000
#define INITDONE 4

#define WAIT while(!(RXADDR->rxcs&(TR | DONE)))

struct {
    int rxcs;
    int rxdb;
};

struct devtab rxtab;
struct buf rrxbuf;
struct rxadr {
    char sector;
    char track;
};

rxstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	if (bp->b_blkno >= NRXBLK) {
		if (bp->b_blkno==NRXBLK)
			bp->b_wcount = max(bp->b_wcount, -128);
					/* so we don't get errors when */
					/* trying to read the last 2 sectors */
		else {
			bp->b_flags =| B_ERROR;
			iodone(bp);
			return;
		}
	}
	bp->b_resid = -bp->b_wcount; /* used to tell how far to go */
	bp->b_wcount = 0; /* used to tell how far we are (in bytes!) */
	bp->av_forw = 0;
	spl5();
	if (!(rxtab.d_actf))
		rxtab.d_actf = bp;
	else
		rxtab.d_actl->av_forw = bp;
	rxtab.d_actl = bp;
	if (!(rxtab.d_active)) rxstart();
	spl0();
}

rxaddr(abp)
struct buf *abp;
{
	register int sect, track;
	register struct buf *bp;
	int t1, t2, dtyp;

	bp = abp;
	dtyp = (bp->b_dev.d_minor&6) >> 1;
	sect = (bp->b_blkno<<2) + (bp->b_wcount>>7);
	if (!dtyp)
		sect = sect==36?NRXBLK*4:sect==NRXBLK*4?36:sect;
	t1 = sect/26;
	if ((!dtyp) || dtyp == 2) {
		t2 = sect%26;
		track = t1 + (dtyp?1:0);
		sect = 6*t1 + t2*2 + (t2>=13?1:0);
	}
	else track = t1;
	return((track << 8) + sect%26 + 1);
}

rxto(word)
{
	WAIT;
	RXADDR->rxdb.lobyte = word.lobyte;
	WAIT;
	RXADDR->rxdb.lobyte = word.hibyte;
}

rxfrom()
{
	int word;

	WAIT;
	word.lobyte = RXADDR->rxdb.lobyte;
	WAIT;
	word.hibyte = RXADDR->rxdb.lobyte;
	return(word);
}

char sbuf[128];

rxdata(bp, command, wcount)
struct buf *bp;
{
	register char *cp;
	register int nbytes;
	register int bn;
	int tn[2];
#ifndef UCBUFMOD
	int sa, sps;
#endif not UCBUFMOD

	cp = bp;	/* for speed and size */
	tn[0] = cp->b_xmem;  tn[1] = cp->b_addr;
	dpadd(tn, cp->b_wcount);
	bn = lshift(tn, -6);
	tn[1] =& 077;
	RXADDR->rxcs = command | GO;
	nbytes = wcount<<1;
	if (command == FILL) {
#ifndef UCBUFMOD
		cp = ka6-6;  sa = cp->integ;
		sps = PS->integ;  PS->integ = 030340;
		cp->integ = bn;
		copyin(tn[1], sbuf, nbytes);
		cp->integ = sa;  PS->integ = sps;
#endif not UCBUFMOD
#ifdef UCBUFMOD
		btk(bn, tn[1], sbuf, wcount);
#endif UCBUFMOD
		cp = sbuf;
		do {
			WAIT;
			RXADDR->rxdb.lobyte = *cp++;
		} while --nbytes;
		while(!(RXADDR->rxcs&DONE)) rxto(0);
	}
	else {
		cp = sbuf;
		do {
			WAIT;
			*cp++ = RXADDR->rxdb.lobyte;
		} while --nbytes;
#ifndef UCBUFMOD
		cp = ka6-6;  sa = cp->integ;
		sps = PS->integ;  PS->integ = 030340;
		cp->integ = bn;
		copyout(sbuf, tn[1], wcount<<1);
		cp->integ = sa;  PS->integ = sps;
#endif not UCBUFMOD
#ifdef UCBUFMOD
		ktb(bn, tn[1], sbuf, wcount);
#endif UCBUFMOD
		while(!(RXADDR->rxcs&DONE)) rxfrom();
	}
}

rxstart()
{
	register struct buf *bp;
	register cmd;

	if (!(bp = rxtab.d_actf))
		return;
	cmd = WRITE | GO | IENABLE | ((bp->b_dev.d_minor&1)<<4);
	if (bp->b_flags&B_READ) 
		cmd =| READ;
		else rxdata(bp, FILL, min(bp->b_resid, 64));
	rxtab.d_active++;
	RXADDR->rxcs = cmd;
	rxto(rxaddr(bp));
}

rxintr()
{
	register struct buf *bp;
	int ersave;

	if (!(rxtab.d_active))
		return;
	rxtab.d_active = 0;
	bp = rxtab.d_actf;
	if (RXADDR->rxcs < 0) {
		RXADDR->rxcs = RDSTAT | GO | ((bp->b_dev.d_minor&1)<<4);
		while (!(RXADDR->rxcs&DONE)); 
		if ((ersave = RXADDR->rxdb) & DRVRDY) {
			RXADDR->rxcs = RDERR;
			while(!(RXADDR->rxcs&DONE));
			deverror(bp, ersave, RXADDR->rxdb);
			RXADDR->rxcs = INIT;
			while (!(RXADDR->rxdb&INITDONE));
			if (++rxtab.d_errcnt <= 10) {
				rxstart();
				return;
			}
		}
		bp->b_flags =| B_ERROR;
	}
	if (bp->b_flags&B_READ) 
		rxdata (bp, EMPTY, min(bp->b_resid, 64));
	bp->b_resid =- min(bp->b_resid, 64);
	bp->b_wcount =+ 128;
	if (bp->b_resid && !(bp->b_flags&B_ERROR)) {
		rxstart();
		return;
	}
	rxtab.d_errcnt = 0;
	rxtab.d_actf = bp->av_forw;
	iodone(bp);
	rxstart();
}

rxread(dev)
{
	physio(rxstrategy, &rrxbuf, dev, B_READ);
}

rxwrite(dev)
{
	physio(rxstrategy, &rrxbuf, dev, B_WRITE);
}
