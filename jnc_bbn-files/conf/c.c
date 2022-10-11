#

int	(*bdevsw[])()
{
/* 0 */ &nulldev,       &nulldev,       &rkstrategy,    &rktab, /* rk */
	0
};

int	(*cdevsw[])()
{
/* 0 console */ &klopen,   &klclose,  &klread,   &klwrite,  &klsgtty,  &klttyawt, &klttycap,
/* 1 IMP11a  */ &impopen,  &impclose, &impread,  &impwrite, &impsgtty, &impawt,   &impcap,
/* 2 mem */     &nulldev,  &nulldev,  &mmread,   &mmwrite,  &nodev,    &nocap,    &nullcap,
/* 3 rk */      &nulldev,  &nulldev,  &rkread,   &rkwrite,  &nodev,    &nocap,    &nocap,
/* 4 ports  */  &portopen, &nodev,    &portread, &portwrite,&portstty, &portawt,  &portcap,
/* 5 tty */     &nodev,    &nodev,    &nodev,    &nodev,    &nodev,    &nocap,    &nocap,
/* 6 pty ctl */ &ptcopen,  &ptcclose, &ptcread,  &ptcwrite, &ptcsgtty, &ptcawt,   &ptccap,
/* 7 pty slv */ &ptsopen,  &ptsclose, &ptsread,  &ptswrite, &ptssgtty, &ptsawt,   &ptscap,
	0
};




int     portdev {(4<<8)|0}; /* For port.c */

int	rootdev	{(0<<8)|0};
int	swapdev	{(0<<8)|0};
int	swplo	4000;	/* cannot be zero */
int	nswap	872;
