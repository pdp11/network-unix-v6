#
/*
	DR-11a driver for Diablo printer
	version 1: copies data one byte at a time
	from user space and sends 16-bit words
	to the interface
	when bit 14 of a data word is set
	(i.e. w<0) then the succeeding word may be given
	to the interface without waiting for the interrupt
	for its predecessor.  the "extra" word(s) are sent
	by dlpstart();
 */


#include "../../h/param.h"
#include "../../h/conf.h"
#include "../../h/user.h"

#define DRADDR1	0167770
#define	DRADDR2	0177550	/* Paper tape punch address */

#define DONE	0200
#define IENABLE	0100
#define	IE2	0040

#define DLPPRI	10
#define DLPLWAT	50
#define DLPHWAT	100

struct {
	char	lobyte;
	char hibyte;
};

struct regs {
	int	dlpsr;
	int	dlpb1;
	int	dlpb2;
};

struct	dlp {
	int	cc;
	int	cf;
	int	cl;
	int	flag;
	int	mcc;
	int	ccc;
	int	mlc;
} dlp[2];

char *dlpaddrs [] {DRADDR1,DRADDR2};
#define	OPEN	04

dlpopen(dev, flag)
{
	register struct dlp *dlpp;

	dlpp = &dlp [dev.d_minor];
	if (dlpp->flag & OPEN ) {
		u.u_error = EIO;
		return;
	}
	dlpp->flag = OPEN;
}

dlpclose(dev,flag)
{
	register struct dlp *dlpp;

	dlpp = &dlp[dev.d_minor];
	dlpp->flag = 0;
	if (dlpp->cc < 2) 
		dlpaddrs[dev.d_minor]->dlpsr =&  ~ IENABLE; /* hidden tilda*/
}

dlpwrite(dev)
{
	register int c;

	while ((c=cpass() ) >=0 )
		dlpoutput(c, dev);
}

dlpstart(dev)
{
	register int c;
	register struct regs *draddr;
	register struct dlp *dlpp;

	draddr = dlpaddrs[dev.d_minor];
	dlpp = &dlp[dev.d_minor];
	while ((draddr->dlpsr & DONE)
	  &&  ((c = dlpgetw (dev)) != 0))
	{
		draddr->dlpb1 =c;
		while ((c & 040000) != 0){
			if ((c = dlpgetw (dev)) != 0)
			{
				draddr->dlpb1 = c;
			}
		}
	}
	if ((!dlpp->flag) && (dlp->cc < 2)) {
		draddr->dlpsr =& ~ IENABLE; /* hidden tilda */
		return;
	}
	draddr->dlpsr =| IENABLE ;
}

dlpint(dev)
{
	register int c;
	register struct dlp *dlpp;
	register struct regs *draddr;

	dlpp = &dlp[dev.d_minor];
	draddr = dlpaddrs [dev.d_minor];
	if (draddr-> dlpsr <0)
	{
		if (dev.d_minor == 0) {
			printf ("Dblo crg\n");
			return;
		}else{
			printf ("dlp=%o %o\n"
				,draddr->dlpsr
				,draddr->dlpb2);
		}
	}
	if ((draddr->dlpsr & DONE) == 0) return;
	dlpstart(dev);
	if (dlpp->cc == DLPLWAT || dlpp->cc == 0)
		wakeup(dlpp);
}

dlpoutput(c, dev)
{
	register struct dlp *dlpp;

	dlpp = &dlp[dev.d_minor];
	if (dlpp->cc >= DLPHWAT)
		sleep(dlpp,DLPPRI);
	putc(c,dlpp);
	spl5(); /* could be 4, but jumper on plato needs changing */
	if ((dlpp->cc & 1) == 0)
		dlpstart (dev);
	spl0();
}

dlpgetw(dev)
{
	int i;
	register struct dlp *dlpp;

	dlpp = &dlp[dev.d_minor];
	i=0;
	if (dlpp->cc < 2) return (0);
	i.lobyte = getc (dlpp);
	i.hibyte = getc(dlpp);
	return(i);
}
dlprint(dev)
{
}
