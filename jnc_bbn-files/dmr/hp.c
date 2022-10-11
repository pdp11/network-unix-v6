#
/*
 * RWP06 driver
 * PRELIMINARY *
 *  modified by A. G. Nemeth - BBN - 18 May 1978 *
 */


#include "../h/param.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/systm.h"
#include "../h/user.h"

struct {
	int	hpcs1;	/* Control and Status register 1 */
	int	hpwc;	/* Word count register */
	int	hpba;	/* UNIBUS address register */
	int	hpda;	/* Desired address register */
	int	hpcs2;	/* Control and Status register 2*/
	int	hpds;	/* Drive Status */
	int	hper1;	/* Error register 1 */
	int	hpas;	/* Attention Summary */
	int	hpla;	/* Look ahead */
	int	hpdb;	/* Data buffer */
	int	hpmr;	/* Maintenance register */
	int	hpdt;	/* Drive type */
	int	hpsn;	/* Serial number */
	int	hpof;	/* Offset register */
	int	hpdc;	/* Desired Cylinder address register*/
	int	hpcc;	/* Current Cylinder */
	int	hper2;	/* Error register 2 */
	int	hper3;	/* Error register 3 */
	int	hpec1;	/* Burst error bit position */
	int	hpec2;	/* Burst error bit pattern */
	int	hpbae;	/* Bus address extension */
	int	hpcs3;	/* Control and status register 3 */
};

#define	HPADDR	0176700
#define	NHP	2	/*number of HP drives */
#define NMDLOG2 4	/* number of meta disks log 2 */
#define MASK	((1<<NMDLOG2)-1)	/* mask for meta disk ID */

struct {
	char	*nblocks;
	int	cyloff;
} hp_sizes[] {
	9614,	0,	/* 0 - hpsx8 */
	9614,  23,	/* 1 - hpsx7 */
	9614,  46,	/* 2 - hpsx6 */
	9614,  69,	/* 3 - hpsx5 */
       63536,  92,	/* 4 - hplx2 */
	9614, 244,	/* 5 - hpsx4 */
	9614, 267,	/* 6 - hpsx3 */
	9614, 290,	/* 7 - hpsx2 */
	9614, 313, 	/* 8 - hpsx1 */
	9614, 336,	/* 9 - hpsx0 */
       63536, 359,	/*10 - hplx0 */
       63536, 511,	/*11 - hplx1 */
       63536, 663,	/*12 - hplx3 */
};


struct	devtab	hptab;
struct	devtab	hputab[NHP];
struct	buf	hpbuf;
char	hpstatus[NHP];
int	hpsioc[NHP];
int	hpsios[NHP];

int	ghper1, ghper2, ghper3, ghpds, ghpcs2;

			/* Drive Commands */
#define	GO	01
#define	UNLOAD	02
#define	SEEK	04
#define DCLR	010
#define	RELEASE	012
#define	PRESET	020
#define	SEARCH	030

#define	ERR	040000	/* hpds - Error */
#define	MOL	010000	/* hpds - Medium online */
#define	DRY	0200	/* hpds - drive ready */
#define VV	0100	/* hpds - volume valid */
#define SC	0100000	/* hpcs1 - Special condition */
#define	TRE	040000	/* hpcs1 - transfer error */
#define DVA	04000	/* hpcs1 - drive available */
#define	IE	0100	/* hpcs1 - interrupt enable */
#define RDY     0200    /* hpcs1 - controller ready */
#define	NED	010000	/* hpcs2 - Nonexistent drive */
#define	WLE	04000	/* hper1 - Write lock error */
#define FMT22	010000	/* hpof - 16 bit /word format */
#define	ECI	04000	/* hpof - ecc inhibit */
/*
 * Use av_back to save track+sector,
 * b_resid for cylinder.
 */

#define	trksec	av_back
#define	cylin	b_resid

hpstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register char *p1, *p2;
	struct devtab *dp;
	int	unit;

	bp = abp;
	p1 = &hp_sizes[bp->b_dev.d_minor&MASK];
	unit = bp->b_dev.d_minor>>NMDLOG2;
	if (unit >= NHP || hpstatus[unit] ||
	    bp->b_blkno >= p1->nblocks) {
		bp->b_flags =| B_ERROR;
		iodone(bp);
		return;
	}
	bp->av_forw = 0;
	bp->cylin = p1->cyloff;
	p1 = bp->b_blkno;
	p2 = lrem(p1, 22);
	p1 = ldiv(p1, 22);
	bp->trksec = (p1%19)<<8 | p2;
	bp->cylin =+ p1/19;
	spl5();
	dp = &hputab[unit];
	hpsioc[unit]++;
	if ((p1 = dp->d_actf)==0)
		dp->d_actf = bp;
	else {
		for (; p2 = p1->av_forw; p1 = p2) {
			if (p1->cylin <= bp->cylin
			 && bp->cylin <  p2->cylin
			 || p1->cylin >= bp->cylin
			 && bp->cylin >  p2->cylin) 
				break;
		}
		bp->av_forw = p2;
		p1->av_forw = bp;
	}
	if (dp->d_active==0)
		hpustart(unit);
	spl0();
}

hpustart(dev)
{
	register struct buf *bp;
	register struct devtab *dp;
	register unit;
	int	search;
	int     i;

	unit = dev;
	dp = &hputab[unit];
	HPADDR->hpcs2.lobyte = unit;
	HPADDR->hpas = 1<<unit;
	/* hpstatus[unit] = 0 was here, moved lower */
	if ((HPADDR->hpcs1&DVA)==0) {
	/* either NED or dual-port not avail */
		printf("NED ");
		goto abort;
	}
	if ((HPADDR->hpds&MOL)==0) {
		if(hpstatus[unit] == 0)   /* check added to prevent */
			printf("MOL ");   /* multiple printfs: 9/25/77:mob */
		goto abort;
	}
	/* check on dp->d_actf was here: moved for error handling 9/23/77:mob */
	hpstatus[unit] = 0;     /* moved from above 9/25/77:mob */
	if (HPADDR->hpds&ERR) {
		printf("SE %d %o\n",unit,HPADDR->hper1);
		HPADDR->hpcs1.lobyte = IE|DCLR|GO;
		if (++dp->d_errcnt > 16) {
			printf("Unit %d unloaded\n",unit);
			HPADDR->hpcs1.lobyte = IE|UNLOAD|GO;
			goto abort;
		}
		if (dp->d_actf)         /* error handling 9/23/77:mob */
			dp->d_active--;
	}
	if ((HPADDR->hpds&VV) == 0) {
		HPADDR->hpcs1.lobyte = IE|PRESET|GO;
		HPADDR->hpof = FMT22|ECI;
	}
	if ((bp = dp->d_actf)==0)       /* error handling (moved) 9/23/77:mob */
		return;                 /*              "               */
	dp->d_active++;
	i = bp->cylin - HPADDR->hpcc;
/*      systm[11] =+ (i > 0 ? i : -i);  /* monitoring 9/22/77:rsk */
	HPADDR->hpdc = bp->cylin;
	search = bp->trksec.lobyte-(HPADDR->hpla>>6)-1;
	if (search<0) search =+ 22;
	if ((bp->cylin!=HPADDR->hpcc || search>6) &&
	    dp->d_active<3) {
		search = bp->trksec;
		search.lobyte =- 4;
		if (search.lobyte<0) search.lobyte =+ 22;
		hpsios[unit]++;
		HPADDR->hpda = search;
		HPADDR->hpcs1.lobyte = IE|SEARCH|GO;
	} else {
		dp->b_forw = 0;
		if (hptab.d_actf == 0)
			hptab.d_actf = dp; else
			hptab.d_actl->b_forw = dp;
		hptab.d_actl = dp;
		if (hptab.d_active == 0)
			hpstart();
	}
	return;
abort:
	if (hpstatus[unit] == 0) {      /* check must be here since we want,
					 * in all, cases, to free pending blocks */
		printf("er1 %o, er2 %o, er3 %o, ds %o, cs1 %o, cs2 %o\n",
		    HPADDR->hper1, HPADDR->hper2, HPADDR->hper3, HPADDR->hpds,
		    HPADDR->hpcs1, HPADDR->hpcs2);      /* helpful debugging info: 9/21/77:mob */
		HPADDR->hpcs1.lobyte = IE|DCLR|GO;      /* error handling: 9/23/77:mob */
	}
	hpstatus[unit]++;
	while(bp = dp->d_actf) {
		bp->b_flags =| B_ERROR;
		dp->d_actf = bp->av_forw;
		iodone(bp);
	}
	dp->d_active = 0;
	dp->d_errcnt = 0;
	printf("RP06 drive %d offline\n",unit);
}

hpstart()
{
	register struct buf *bp;
	register struct devtab *dp;

	if ((dp = hptab.d_actf) == 0)
		return;
	bp = dp->d_actf;
	HPADDR->hpcs2.lobyte = bp->b_dev.d_minor >> NMDLOG2;
	hptab.d_active++;
	HPADDR->hpdc = bp->cylin;
	rhstart(bp, &HPADDR->hpda, bp->trksec, &HPADDR->hpbae);
}

hpintr()
{
	register struct buf *bp;
	register struct devtab *dp;
	register unit;

	if (hptab.d_active) {	/* data transfer underway */
	dp = hptab.d_actf;
	bp = dp->d_actf;
	unit = bp->b_dev.d_minor>>NMDLOG2;
	HPADDR->hpcs2.lobyte = unit;
	if (HPADDR->hpcs1 & TRE) {                        /* error bit */
		deverror(bp, HPADDR->hper1, HPADDR->hpcs2);
		ghper1 = HPADDR->hper1;
		ghper2 = HPADDR->hper2;
		ghper3 = HPADDR->hper3;
		ghpds = HPADDR->hpds;
		ghpcs2 = HPADDR->hpcs2;
		HPADDR->hpcs1 = TRE|IE|DCLR|GO;
		if (++hptab.d_errcnt > 16 ||
		  ghper1&WLE ) {
			bp->b_flags =| B_ERROR;
		} else
			hptab.d_active = 0;
	}

	/* check not only for data transfer but also be sure this is
	 * really the interrupt for it and not some seek interrupt.
	 */

	if (hptab.d_active && (HPADDR->hpcs1&RDY)) {    /* mob:10/07/77 */
		hptab.d_active = 0;
		hptab.d_errcnt = 0;
		hptab.d_actf = dp->b_forw;
		dp->d_active = 0;
		dp->d_errcnt = 0;
		dp->d_actf = bp->av_forw;
		bp->b_resid = HPADDR->hpwc;
		iodone(bp);
		HPADDR->hpcs1 = IE|RELEASE|GO;
		hpustart(unit);
	}
	if (hptab.d_active==0)
		hpstart();
	} else
		HPADDR->hpcs1.hibyte = TRE>>8;
	for (unit=0; unit<NHP; unit++) {
		if (HPADDR->hpas&(1<<unit))
			hpustart(unit);
	}
	if ((HPADDR->hpcs1&IE)==0) {
		HPADDR->hpcs1 = IE;
		if (HPADDR->hpcs1&TRE)
			HPADDR->hpcs1.hibyte = TRE>>8;
	}
}

hpread(dev)
{

	if(hpphys(dev))
	physio(hpstrategy, &hpbuf, dev, B_READ);
}

hpwrite(dev)
{

	if(hpphys(dev))
	physio(hpstrategy, &hpbuf, dev, B_WRITE);
}

hpphys(dev)
{
	register c;

	c = lshift(u.u_offset, -9);
	c =+ ldiv(u.u_count+511, 512);
	if(c > hp_sizes[dev.d_minor & MASK].nblocks) {
		u.u_error = ENXIO;
		return(0);
	}
	return(1);
}
