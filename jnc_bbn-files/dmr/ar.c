#
/*  Driver for AR11.
 *	Device name: /dev/ar0
 *	Stty arguments to set sampling rate, number of channels,
 *		and unipolar/bipolar analog.
 *	Read A/D converted values (first word is sample number - unsigned
 *		integer value, with 16 bit wrap-around
 *	Write D/A values, "intensified" on loading of Y. If an odd number
 *		of words are written, trailing values will be ignored.
 */

#include "../h/param.h"
#include "../h/user.h"

#define AR	0170400

/* A/D status */
#define UNIPOLAR	020000
#define CHAN		07400
#define CHANONE 	00400
#define DONE		0200
#define IENABLE 	0100
#define OVENABLE	040
#define EXTENABLE	020
#define GO		01

/* Clock status */
#define EXTINPUT	0100000
#define EXTENABLE	040000
#define REPEAT		0400
#define OVERFL		0200
#define OVERENABLE	0100
#define ALWAYS		020	/* Always set for AR11 */
#define RATE		070
#define CLENABLE	01

#define YMODE		010	/* "intensify" on loading of Y */

struct
{	int vals[17];	/* vals[0] is sample number */
	int admode;
	char nchans;
	char arflags;
} ar_data;

struct
{	int adstat;
	int adbuf;
	int clkstat;
	int clkpre;
	int dispstat;
	int xbuffer;
	int ybuffer;
	int clkctr;
};

struct {char lobyte, hibyte;};

/* arflags */
#define OPEN	01

aropen(dev,flag)
int dev, flag;
{	/* Default rate is 0.  Until first stty call, return 0s on reads */
	int i;
	if (ar_data.arflags&OPEN)
		u.u_error = EACCES;
	else
	{	ar_data.arflags =| OPEN;
		for (i=17; i--;) ar_data.vals[i] = 0;
		AR->dispstat = YMODE;
	}
}

arclose(dev)
int dev;
{	ar_data.arflags = 0;
	AR->clkstat = AR->adstat = 0;
}

arsgtty(dev,v)
int dev, v;
/* Arguments are:
 * word       data
 *  1	Sampling period --
 *		hi byte is count, lo byte is minus exponent of clock rate
 *		(2 for 100 hz, 3 for 1khz, ..., 6 for 1mhz)
 *		Example: 5<<8 | 3  is 5 milliseconds between samples
 *  2	Lo byte is highest numbered channel to be sampled.
 *		Example: 3 to sample channels 0, 1, 2, and 3.
 *  2	Hi byte is non-zero if A/D range is unipolar (0-5 volts), zero if
 *		bipolar (-2.5 to 2.5 volts).
 *  3	Reserved
 * Note: The total number of samples per second must not exceed 20000, i.e.,
 *	the sampling period divided by the number of channels must be at least
 *	50 microseconds.  Also, the sampling period must be less than 2.56
 *	seconds.  If the given rate cannot be achieved, no sampling will
 *	occur.
 */
/*      Modified 7/18/75 to limit to 0.01 second period to reduce time
 *      consumable by AR11.
 */
{	register int n, m, t;
	int mm;
	if (v)
		u.u_error = EBADF;
	else
	{	ar_data.nchans = (u.u_arg[1]&017);
		if (n = (u.u_arg[0].hibyte & 0377))
		{	m = u.u_arg[0].lobyte&0377;
			while (n<=25)
			{	n =* 10;
				m++;
			}
			if (m<2) goto badtime;  /* 2.55 secs is max */
			/* Verify sufficient time per sample
			t = (100*n+ar_data.nchans)/(ar_data.nchans+1); */
			/* Multiply by 100 for precision */
			t = 100*n;
			mm = m;
			while (t<=25)
			{	t =* 10;
				mm++;
			}
			while (t>255)
			{       t =/ 10;
				mm--;
			}
			/* 25 < t <= 255, and time per sample is
			 *  t*10**(-mm-2)
			 */
		/*      if (mm>=5 || ((mm==4) && (t<50))) goto badtime; */
			if (mm>=4 || ((mm==3) && (t<200))) goto badtime;
			/* Make clock rate between 100hz and 1mhz */
			while (m<2)
			{	n =* 10;
				m++;
			}
			while ((n!=0) && (m>6))
			{	if ((n%10) != 0) goto badtime;
				n =/ 10;
				m--;
			}
		}
		else
badtime:	{	m = 7;
			u.u_error = EINVAL;
		}
		AR->clkpre = -n;
		AR->clkstat = (REPEAT|ALWAYS|CLENABLE) | ((7-m)<<1);
		AR->adstat = ar_data.admode =
			(u.u_arg[1].hibyte ? UNIPOLAR : 0) |
				(IENABLE|OVENABLE);
	}
}

arrint()        /* Runs at priority level 6 */
{	register int *vp, i;
	vp = ar_data.vals;
	(*vp++)++;
	AR->adstat =& ~(IENABLE|OVENABLE);
	*vp++ = AR->adbuf;
	for (i=  ar_data.nchans; i--;)
	{       AR->adstat =+ CHANONE;
		AR->adstat =| GO;
		while ((AR->adstat&DONE) == 0) ;
		*vp++ = AR->adbuf;
	}
	AR->adstat = ar_data.admode;
}

arread(dev)
int dev;
{       register char *vp, i;
	i = 34;  /* 16 channels + time */
	vp = ar_data.vals;
	spl6();
	while (i-- && passc(*vp++) == 0) ;
	spl0();
}

arwrite(dev)
int dev;
{	register int i, c;
	register char *cp;
	int xy[2];
	for (;;)
	{	cp = xy;
		for (i=4; i--;)
			if ( (c = cpass()) != -1)
				*cp++ = c;
			else
				break;
		AR->xbuffer = xy[0];
		AR->ybuffer = xy[1];
		return;
	}
}



arclint()	/* Clock interrupt.  (In case we ever use it.) */
{
}

ardsint()       /* Display interrupt (In case we ever use it.) */
{
}
