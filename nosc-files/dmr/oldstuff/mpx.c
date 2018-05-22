#define	CSR	0177570
#include "../../h/param.h"
#include "../../h/conf.h"
#include "../../h/user.h"
#include "../../h/proc.h"

#define	NMINOR	50
#define	OCHAN	20
#define	NCHAN	20

#define	XHIWAT	30
#define	RHIWAT	30
#define	XLOWAT	10
#define	RLOWAT	10

#define	XPRI	5
#define	RPRI	5
#define	CPRI	1

#define	FOPEN	01
#define	FWAIT	02

#define	OOPEN	01
#define	NOPEN	02
#define	XWAIT	04
#define	RWAIT	010
#define	BLOCK	020
#define	RUN	040

struct clist
{
	int	c_cc;
	int	c_cf;
	int	c_cl;
};

/*
 * list of channels that need
 * acks transmitted.
 * list of channels that have
 * data to be transmitted.
 */
struct	clist	ackq, datq;
int	mpconf;
int	mprnext;
int	mpxnext;

/*
 * map of minor device to channel
 */
struct
{
	char	flag;
	char	chano;
} minor[NMINOR];

/*
 * channel structure.
 * and input and output data q's.
 */
struct
{
	char	flag;
	char	chano;
	struct	clist	iq;
	struct	clist	oq;
} ochan[OCHAN], nchan[NCHAN];

mpopen(dev)
{
	register f, *p;
	static first;

	if(first == 0) {
		for(f=0; f<128; f++) {
			p = cptr(f);
			if(p)
				p->chano = f;
		}
		mpinit();
		first++;
	}
	ochan[0].flag =| OOPEN|NOPEN;
	nchan[0].flag =| OOPEN|NOPEN;
	f = dev.d_minor & 0377;
	if(f >= NMINOR)
		goto bad;
	p = &minor[f];
	if(p->flag & FOPEN)
		goto bad;
	p->flag =| FOPEN;
	p->chano = 0;
	return;

bad:
	u.u_error = ENXIO;
}

mpclose(dev)
{
	register f;

	f = dev.d_minor & 0377;
	minor[f].flag = 0;
	mpdis(f);
}

mpsgtty(dev, v)
{
	register f, *p;

	if(v)
		goto bad;
	f = dev.d_minor & 0377;
	if(f >= NMINOR)
		goto bad;
	switch(u.u_arg[0]) {

	default:
	bad:
		u.u_error = ENXIO;
		return;

	case 1: /* connect */
		mpcon(f);
		return;

	case 2: /* disconnect */
		mpdis(f);
		return;

	case 3: /* wait connect */
		p = &minor[f];
		spl5();
		while(p->chano == 0) {
			p->flag =| FWAIT;
			sleep(p, RPRI);
		}
		spl0();
		return;

	case 4: /* establish process group */
		p = u.u_procp;
		p->p_pgrp = p->p_pid;
		return;

	case 100: /* reset driver */
		while(getc(&ackq) >= 0)
			;
		while(getc(&datq) >= 0)
			;
		mpconf = 0;
		mprnext = 0;
		mpxnext = 0;
		for(f=0; f<128; f++) {
			if(f < NMINOR) {
				p = &minor[f];
				p->flag = 0;
				p->chano = 0;
			}
			p = cptr(f);
			if(p) {
				p->flag = 0;
				p->chano = f;
				while(getc(&p->iq) >= 0)
					;
				while(getc(&p->oq) >= 0)
					;
			}
		}
		return;
	}
}

mpwrite(dev)
{
	register d, *p;

	p = mpf(dev);
	if(p == NULL)
		return;
	while(mpisc(p) && (d = cpass()) >= 0) {
		spl5();
		while(p->oq.c_cc > XHIWAT) {
			p->flag =| XWAIT;
			sleep(&p->oq, XPRI);
		}
		putc(d, &p->oq);
		mprun(p);
		spl0();
	}
}

mpread(dev)
{
	register d, *p;

	p = mpf(dev);
	if(p == NULL)
		return;
	do {
		spl5();
		while((d = getc(&p->iq)) < 0) {
			if(!mpisc(p))
				goto out;
			p->flag =| RWAIT;
			sleep(&p->iq, RPRI);
		}
		spl0();
	} while(passc(d) >= 0 && p->iq.c_cc > 0);
out:
	spl5();
	if((p->flag&BLOCK) != 0 && p->iq.c_cc < RLOWAT) {
		p->flag =& ~BLOCK;
		putc(p->chano, &ackq);
		mpxintr();
	}
	spl0();
}

mpisc(pp)
int *pp;
{
	register *p;

	p = pp;
	if(p->flag & NOPEN)
	if(p->flag & OOPEN)
		return(1);
	u.u_error = ENXIO;
	return(0);
}

mpcon(f)
{
	register *p, *q;

	p = &minor[f];
	if(p->chano)
		goto bad;
	spl5();
	while(mpconf)
		sleep(&mpconf, CPRI);
	mpconf = p;
	q = cptr(0);
	putc(u.u_arg[1], &q->oq);
	mprun(q);
	while(mpconf)
		sleep(&mpconf, CPRI);
	spl0();
	if(p->chano != 0)
		return;

bad:
	u.u_error = ENXIO;
}

mpdis(f)
{
	register c, *p;

	p = &minor[f];
	c = p->chano;
	if(c) {
		spl5();
		p->chano = 0;
		p = cptr(c);
		if(p) {
			p->flag =& ~(OOPEN|XWAIT|RWAIT);
			while(getc(&p->iq) >= 0)
				;
			wakeup(&p->iq);
			wakeup(&p->oq);
			mprun(p);
		}
		spl0();
	}
}

mprun(pp)
{
	register *p;

	p = pp;
	if((p->flag&RUN) == 0) {
		p->flag =| RUN;
		putc(p->chano, &datq);
		mpxintr();
	}
}

mpf(dev)
{
	register c, *p;

	c = dev.d_minor & 0377;
	if(c >= NMINOR)
		goto bad;
	c = minor[c].chano;
	if(c == 0)
		goto bad;
	p = cptr(c);
	if(p)
		return(p);

bad:
	u.u_error = ENXIO;
	return(NULL);
}

/*
 * return pointer to channel
 * structure, given chan number.
 *	ochans are 1 thru OCHAN-1
 *	nchans are 64 thru NCHAN+63
 */
cptr(c)
{
	if(c < 64) {
		if(c < 0 || c >= OCHAN)
			return(NULL);
		return(&ochan[c]);
	}
	c =- 64;
	if(c < 0 || c >= NCHAN)
		return(NULL);
	return(&nchan[c]);
}

/*
 * convert to/from
 * orig and non-orig chan
 */
invert(c)
{

	if(c >= 64)
		return(c-64);
	return(c+64);
}

mpexec(cc, d)
{
	register i, c, *p;

	c = d;
	/*
	 * reply to connect
	 */
	if(cc == 0) {
		if(mpconf)
		if(c && c != 64) {
			p = cptr(c);
			if(p) {
				p->flag =| OOPEN|NOPEN;
				p = mpconf;
				p->chano = c;
			}
		}
		mpconf = 0;
		wakeup(&mpconf);
		return;
	}
	/*
	 * discon
	 */
	if(c >= 128) {
		c =- 128;
		p = cptr(c);
		if(p) {
			p->flag =& ~(RUN|NOPEN|RWAIT|XWAIT);
			while(getc(&p->oq) >= 0)
				;
			wakeup(&p->iq);
			wakeup(&p->oq);
		}
		return;
	}
	/*
	 * request connect
	 */
	if(c >= NMINOR)
		goto bad;
	p = &minor[c];
	if(p->chano)
		goto bad;
	for(i=1; i<NCHAN; i++)
		if(nchan[i].flag == 0) {
			nchan[i].flag =| OOPEN|NOPEN;
			goto found;
		}
bad:
	i = 0;
	goto out;

found:
	p->chano = invert(i);
	if(p->flag & FWAIT) {
		p->flag =& ~FWAIT;
		wakeup(p);
	}

out:
	p = cptr(64);
	putc(i, &p->oq);
	mprun(p);
}

mpx()
{
	register c, d, *p;

	/*
	 * transmit 2nd half of 2 char mesg
	 */
	if(mpxnext) {
		d = ~mpxnext;
		mpxnext = 0;
		return(d);
	}

	/*
	 * transmit ack
	 */
	c = getc(&ackq);
	if(c >= 0)
		return(invert(c)+128);

	/*
	 * transmit 2 char (chan,data) message
	 */
loop:
	c = getc(&datq);
	if(c >= 0) {
		p = cptr(c);
		if(p == NULL)
			goto loop;
		d = getc(&p->oq);
		if(d < 0) {
			p->flag =& ~RUN;
			if((p->flag & OOPEN) != 0)
				goto loop;
			d = invert(c) + 128;
			c = 0;
		} else
		if(c == 0 || c == 64)
			putc(c, &datq);
		mpxnext = ~d;
		if((p->flag & XWAIT) && p->oq.c_cc < XLOWAT) {
			p->flag =& ~XWAIT;
			wakeup(&p->oq);
		}
		return(invert(c));
	}

out:
	return(-1);
}

mpr(d)
{
	register c, *p;

	if(CSR->integ == 0314)
		printf("%o\n", d);
	/*
	 * if 2nd half of incomming message,
	 * put data on in q and put chan on ack q.
	 */
	if(mprnext) {
		c = ~mprnext;
		mprnext = 0;
		p = cptr(c);
		if(p == NULL)
			return;
		if(c == 64 || c == 0) {
			mpexec(c, d);
			return;
		}
		if(p->flag&OOPEN)
			putc(d, &p->iq);
		if(p->flag & RWAIT) {
			p->flag =& ~RWAIT;
			wakeup(&p->iq);
		}
		if(p->iq.c_cc > RHIWAT) {
			p->flag =| BLOCK;
			return;
		}
		putc(c, &ackq);
		mpxintr();
		return;
	}

	/*
	 * recieve ack message.
	 * put channel back on data q.
	 */
	if(d >= 128) {
		putc(d-128, &datq);
		mpxintr();
		return;
	}
	mprnext = ~d;
}
