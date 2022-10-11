/*	param.h	4.12	81/06/11	*/

/*
 * 28 May 82 Hartwell at Stanford
 *	Increased the max no. of mountable filesystems to 25 (from 15).
 * 2 Dec 81 Erik Hedberg at Stanford
 *	Added group bit array stuff from CCA
 */

/*
 * Tunable variables which do not usually vary per system.
 *
 * The sizes of most system tables are configured
 * into each system description.  The file system buffer
 * cache size is assigned based on available memory.
 * The tables whose sizes don't vary often are given here.
 */

#define	NMOUNT	25		/* number of mountable file systems */
#define	MSWAPX	15		/* pseudo mount table index for swapdev */
#define	MAXUPRC	25		/* max processes per user */
#define	SSIZE	4		/* initial stack size (*512 bytes) */
#define	SINCR	4		/* increment of stack (*512 bytes) */
#define	NOFILE	20		/* max open files per process */
/* NOFILE MUST NOT BE >= 31; SEE pte.h */
#define	CANBSIZ	256		/* max size of typewriter line */
#define	NCARGS	10240		/* # characters in exec arglist */

/*
 * priorities
 * probably should not be
 * altered too much
 */

#define	PSWP	0
#define	PINOD	10
#define	PRIBIO	20
#define	PRIUBA	24
#define	PZERO	25
#define	PPIPE	26
#define	PWAIT	30
#define	PSLEP	40
#define	PUSER	50

#define	NZERO	20

/*
 * signals
 * dont change
 */

#ifndef	NSIG
#include "signal.h"
#endif

/*
 * Return values from tsleep().
 */
#define	TS_OK	0	/* normal wakeup */
#define	TS_TIME	1	/* timed-out wakeup */
#define	TS_SIG	2	/* asynchronous signal wakeup */

/*
 * fundamental constants of the implementation--
 * cannot be changed easily.
 */

#define	NBBY		8		/* number of bits in a byte */
#define	NBPW		sizeof(int)	/* number of bytes in an integer */
#define	NBPG		512
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		9		/* LOG2(NBPG) */

#define	UPAGES	8		/* pages of u-area */
#define	NULL	0
#define	CMASK	0		/* default mask for file creation */
#define	NODEV	(dev_t)(-1)
#define	ROOTINO	((ino_t)2)	/* i number of all roots */
#define	SUPERB	((daddr_t)1)	/* block number of the super block */
#define	DIRSIZ	14		/* max characters per directory */
#ifdef	CCA_GID
#define	NGRPS	256		/* max number of groups  CCA_GID */
#endif	CCA_GID

/*
 * Clustering of hardware pages on machines with ridiculously small
 * page sizes is done here.  The paging subsystem deals with units of
 * CLSIZE pte's describing NBPG (from vm.h) pages each... BSIZE must
 * be CLSIZE*NBPG in the current implementation, that is the paging subsystem
 * deals with the same size blocks that the file system uses.
 *
 * NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE
 */
#define	CLSIZE	2
#define	CLOFSET		(CLSIZE*NBPG-1)	/* for clusters, like PGOFSET */

/* give the base virtual address (first of CLSIZE) */
#define	clbase(i)	((i) &~ (CLSIZE-1))

/* round a number of clicks up to a whole cluster */
#define	clrnd(i)	(((i) + (CLSIZE-1)) &~ (CLSIZE-1))

#define	BSIZE	1024
#define	INOPB	16
#define	BMASK	01777
#define	BSHIFT	10
#define	NMASK	0377
#define	NSHIFT	8
#define	NICINOD	100
#define	NICFREE	178

#ifndef INTRLVE
/* macros replacing interleaving functions */
#define	dkblock(bp)	((bp)->b_blkno)
#define	dkunit(bp)	(minor((bp)->b_dev) >> 3)
#endif

/* inumber to disk address and inumber to disk offset */
#define	itod(x)	((daddr_t)((((unsigned)(x)+2*INOPB-1)/INOPB)))
#define	itoo(x)	((int)(((x)+2*INOPB-1)%INOPB))

/* file system blocks to disk blocks and back */
#define	fsbtodb(b)	((b)*CLSIZE)
#define	dbtofsb(b)	((b)/CLSIZE)

#define	NINDIR	(BSIZE/sizeof(daddr_t))

#define	CBSIZE	28		/* number of chars in a clist block */
#define	CROUND	0x1F		/* clist rounding; sizeof(int *) + CBSIZE -1*/

/*
 * Macros for fast min/max
 */
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))

/*
 * Some macros for units conversion
 */
/* Core clicks (512 bytes) to segments and vice versa */
#define	ctos(x)	(x)
#define	stoc(x)	(x)

/* Core clicks (512 bytes) to disk blocks */
#define	ctod(x)	(x)

/* clicks to bytes */
#define	ctob(x)	((x)<&lt;9)

/* bytes to clicks */
#define	btoc(x)	((((unsigned)(x)+511)>>9))

/*
 * Machine-dependent bits and macros
 */
#define	UMODE	PSL_CURMOD		/* usermode bits */
#define	USERMODE(ps)	(((ps) & UMODE) == UMODE)

#define	BASEPRI(ps)	(((ps) & PSL_IPL) != 0)

/*
 * Provide about n microseconds of delay
 */
#define	DELAY(n)	{ register int N = (n); while (--N > 0); }
