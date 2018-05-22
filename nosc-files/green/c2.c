#
/*
 *      unix configuration table
 */

/* UNIX Identification and Configuration information */
#define ED  6
#define VER 2
#define MAJ 2
#define MIN 3

int unixid  {( (ED<<12) | (VER<<8)|(MAJ<<4)|MIN)};
char id[] "~|^`Green-40 network UNIX Vx.x DN root=/dev/rl0 swap=/dev/rl1\n";

/* to pacify V7 C compiler */

extern nodev(), nulldev();
extern ctclose(), ctopen(), ctwrite();
extern dhclose(), dhopen(), dhread(), dhsgtty(), dhwrite();
extern dqsclose(), dqsopen(), dqsread(), dqswrite();
extern rlopen(), rlread(), rlstrategy(), rltab(), rlwrite();
extern hpopen(), hpread(), hpstrategy(), hptab(), hpwrite();
extern hsread(), hsstrategy(), hstab(), hswrite();
extern htclose(), htmopen(), htopen();
extern htread(), htstrategy(), httab(), htwrite();
extern klclose(), klopen(), klread(), klsgtty(), klwrite();
extern lpclose(), lpopen(), lpsgtty(), lpwrite();
extern mmread(), mmwrite();
extern ncpclose(), ncpopen(), ncpread(), ncpwrite();
extern ptcclose(), ptcopen(), ptcread(), ptcwrite();
extern ptsclose(), ptsopen(), ptsread(), ptswrite(), ptysgtty();
extern rkread(), rkstrategy(), rktab(), rkwrite();
extern syopen(), syread(), sysgtty(), sywrite();
extern tcclose(), tcstrategy(), tctab();
extern  rlpopen(),rlpclose(),rlpwrite();

/* device drivers */
int	(*bdevsw[])()
{
	&nodev,         &nodev,         &nodev,         0,      /*0- rk */
	&rlopen,	&nulldev,	&rlstrategy,	&rltab,	/*1- rl */
	0
};
int nblkdev 2;		/* Must define of block devices here */

int	(*cdevsw[])()
{
	&klopen,   &klclose,  &klread,   &klwrite,  &klsgtty,   /*0- console */
	&rlopen,   &nulldev,  &rlread,   &rlwrite,  &nodev,	/*1- rl */
	&dhopen,   &dhclose,  &dhread,   &dhwrite,  &dhsgtty,   /*2- dh */
	&nulldev,  &nulldev,  &mmread,   &mmwrite,  &nodev,     /*3- mem */
	&syopen,   &nulldev,  &syread,   &sywrite,  &sysgtty,   /*4- tty */
	&ncpopen,  &ncpclose, &ncpread,  &ncpwrite, &nodev,     /*5- imp */
/*	&ptcopen,  &ptcclose, &ptcread,  &ptcwrite, &ptysgtty,  /*6- pty-ctl*/
/*	&ptsopen,  &ptsclose, &ptsread,  &ptswrite, &ptysgtty,  /*7- pty-slv*/
	0
};
int nchrdev 8;		/* Must define of char devices here */

int     rootdev {(1<<8)|0};     /* /dev/rl0  */
int     swapdev {(1<<8)|1};     /* /dev/rl1   */
int     swplo   9000;		/* cannot be zero */
int     nswap   1220;		/* RL01's have 10220 blks */
