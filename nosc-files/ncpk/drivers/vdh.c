#include	"param.h"
#include	"buf.h"
#include	"net_net.h"
#include	"net_netbuf.h"
#include	"net_ncp.h"
#include	"net_vdhstat.h"
#include	"net_vdh_imp.h"
#include	"net_vdh.h"

char vdhlock -1;

v_pan (msg)
char *msg;
{
	register t;
	struct vdhregs sv_regs;
	register int *p, *q;

	t = PS->integ;
	spl7();
	p = &sv_regs.wco;  q = &VDH->wco;
	do {			/* save registers at time of panic */
		*p-- = *q--;
	} while q >= &VDH->csri;
	VDH->csri =& ~ V_INTENB;
	VDH->csro =& ~ (V_INTENB|V_TIMRUN);
	panic (msg);
}

imp_init ()
{
	register	t, *ptr;
	register int	oldps;

	oldps = PS->integ;
	spl_imp ();

	vdhichn = 0;
	prvictl = 0;
	vstatebits = VDHILAST;
	impldrd ();

	hellonext = 2*VDH_T*VDH_R;
	vdhosnd = -1;
	vdhofill = 0;
	vdhoack = V_HTOI;
	for(t = 0; t < NCHNS; t++)
		vdhouse[t] = -1;
	ptr = &VDH->csro;
	if ((*ptr & V_INTENB) == 0)
	{
		*ptr++ = V_TIMSEL;
		*ptr = 12500/(HZ/10);	/* 125000 too big for one word */
		*--ptr = V_TIMRUN | V_INTENB;
	}

	oimp.o_type = ih_nop;
	for ( t = 3; --t>=0; )
		sndnetbytes (&oimp,4,0,0,1); 
	PS->integ = oldps;
}

#ifndef UCBUFMOD
imp_output()
{
#endif UCBUFMOD
#ifdef UCBUFMOD
imp_output(msg)
struct netbuf *msg;
{
	register sps;

	sps = PS->integ;
	spl_imp();
	msg_q(&impoq, 0, 0, msg);
#endif UCBUFMOD
	if (((vstatebits ^ UP) & (OUTBUSY|UP)) == 0)
	{
		vdhostart ();
	}
#ifdef UCBUFMOD
	PS->integ = sps;
#endif UCBUFMOD
}

vdhiint ()
{
	register int	t;
	register int	*p;
	register int	*state;
	int	*q;
	int	xorrot;

	if (++vdhlock)
		v_pan ("VDH-i");
#ifdef	PARANOID
	v_bufck ();
#endif	PARANOID

	state = &vstatebits;
	p = &VDH->csri;

#ifndef VDHE
	while ( (*p ^ V_GO) & (V_EOT|V_GO) ) {
#endif not VDHE
#ifdef VDHE
	while (*p & V_EOT) {
#endif VDHE
		if (!(*state & TRYING)) {
			VDH->csri =& ~ V_INTENB;
			goto finis;
		}
		if (*p<0) {
			vs.harderr++;
			vdherr = *p; 
			goto eot_done;
		}

		p = (*state&VDHIAB) ? &vdhibuf[1][0]:&vdhibuf[0][0];

		if ((t = (VDH->cwai - p) - 1) < 0) {	/* null transfer */
	wcerr:
			/* log some error? */
			goto eot_done;
		}
		if (*p & V_HTOI) {
			vs.looped++;
			goto eot_done;
		}
		if (*p & V_SPBIT) {
			if (t != 0) goto wcerr;
			if (*p>0) {
				*state =| (SAWHELLO|OUT2GO);
			} else {
				hellounacked = 0;
				helloinarow++;
			}
			goto eot_done;
		}
		if (t != (p->hibyte&077)) goto wcerr;
		if (!(*state & UP)) {
			vs.wpaks++;
			goto eot_done;
		}

		xorrot = (prvictl ^ *p) << (14-NCHNS);
		t = NCHNS-1;
		do {
			if (xorrot<0) {
				vdhouse[t] = -1;
				*state =| OUT2GO;
			}
			xorrot =<< 1;
		} while (--t>=0);
		prvictl = *p;
		if ((*p & V_WCBITS)==0) {
			vs.inulls++;
			goto eot_done;
		}
		*state =| (ACK2SND|OUT2GO); 
		if ((t = *p & V_CHBITS) != vdhichn) {
			vs.chwrong++;
			goto eot_done;
		}
		if ((*p ^ ((vdhoack>>t)*(V_EVNODD/V_C0ACK)))
		     & V_EVNODD)  {
			vs.idups++;
			goto eot_done;
		}
		if (*state & VDHILAST) {
			wakeup ( &imp );
			vs.ifull++;
			goto eot_done;
		}
		if (net_b.b_cntfree < (VDHDSIZE + NET_B_SIZE - 1)/NET_B_SIZE) {
			vs.nobuf++;
			*state =| VDHINEED; 
			wakeup (&imp); 
			goto eot_done;
		}
		t = (p++->hibyte & 077); 
		swabuf (p,t); 
		t =<< 1;
		if (impi_msg==0) {
			q = impleader ? &imp.type : &imp.pad1;
			t =- 4;
			*q++ = *p++; 
			*q   = *p++; 
		}
		if (t && vectomsg (p,t,&impi_msg,1))
			v_pan ("vdh: vectomsg failed");
		impleader = 0; 
		vdhoack =^ V_C0ACK<<vdhichn;
		if (prvictl & V_LAST) {
#ifdef	PARANOID
			v_bufck();
#endif	PARANOID
			*state =| VDHILAST;
			wakeup (&imp);
		}
		vdhichn++; 
		vdhichn =& NCHNS-1;
		vs.igood++;
eot_done:

		p = &VDH->csri;

		VDH->cwai = (*state&VDHIAB)?&vdhibuf[1][0]:&vdhibuf[0][0];
#ifndef	VDHE
		VDH->wci = VDHSIZE; 
		do {
			*p =| V_GO;
		} while !(*p&V_GO);
#endif	not VDHE
#ifdef	VDHE
		VDH->wci = -(VDHSIZE*2);
		*p =| V_GO;
#endif	VDHE
		*state =^ VDHIAB;
		*p = (*state&VDHIAB) ? (V_ABSEL | V_INTENB) : V_INTENB; 
	} 
	if (((*state ^ OUT2GO) & (OUT2GO|OUTBUSY)) == 0) {
		vdhostart();
	}
	*state =& ~OUT2GO;
finis:
#ifdef	PARANOID
	v_bufck ();
#endif	PARANOID

	--vdhlock;
}

vdhoint ()
{
	register	*p;
	register	xorrot;
	register	*state;

	if (++vdhlock)
		v_pan ("VDH-o");


	state = &vstatebits;

	if (VDH->csro&V_TIMINT)
	{
		VDH->csro =& ~ V_TIMINT;
#ifdef	NOCLOCK
		*state =| CLOCKINT;
#endif	NOCLOCK
		if (ncpopnstate == 0)
		{
			*state =& ~(UP | TRYING);
			goto clock_done;
		}
		for(xorrot = 0; xorrot < NCHNS; xorrot++)
			if(vdhouse[xorrot] > 0)
				vdhouse[xorrot]--;
		if (--hellonext > 0)
			goto clock_done;
		hellonext =+ VDH_R;

		switch (*state & (UP | TRYING))
		{
		case UP|TRYING:
			if (hellounacked > VDH_T)  
			{
				*state =& ~(TRYING | UP);
				hellonext = 077777;
				needinit++;
				wakeup ( &imp ); 
				goto	clock_done;
			}  
			break;
		case TRYING:
			if (hellounacked)
			{
				helloinarow = 0;
			}
			else
			if (helloinarow >= VDH_K)
			{

				*state =| UP;
				wakeup ( &imp );
			}
			break;
		default:
			*state =| TRYING;
			*state =& ~(SAWHELLO | SENDHELLO | UP);
#ifndef	VDHE
			VDH->csri = 0;
			VDH->cwai = &vdhibuf[0][0];
			VDH->wci = VDHSIZE;
			VDH->csri = V_GO | V_ABSEL;
			VDH->cwai = &vdhibuf[1][0];
			VDH->wci = VDHSIZE;
			VDH->csri = V_INTENB | V_GO;
#endif	not VDHE
#ifdef	VDHE
			VDH->csri = 0;
			VDH->cwai = &vdhibuf[0][0];
			VDH->wci = -(VDHSIZE*2);
			VDH->csri = V_GO;
			VDH->csri =| V_ABSEL;
			VDH->cwai = &vdhibuf[1][0];
			VDH->wci = -(VDHSIZE*2);
			VDH->csri = V_INTENB | V_GO | V_ABSEL;
			VDH->csri =& ~ V_ABSEL;
#endif	VDHE
		}
		*state =| SENDHELLO;
	}

clock_done:

	if (VDH->csro&V_EOTINT)
	{
		*state =& ~OUTBUSY;
	}

	if ((*state & OUTBUSY) == 0) {
		vdhostart();
#ifdef	PARANOID
		v_bufck ();
#endif	PARANOID
	}


	--vdhlock;

#ifdef NOCLOCK
	if (*state & CLOCKINT)
	{
		*state =& ~CLOCKINT;
		vdh_clock();	/* no return */
	}
#endif	NOCLOCK
}

vdhostart ()
{
	register int	*p;
	register int	i;
	register int	*state;

	state = &vstatebits;

	if ((*state & TRYING) == 0)
		return;
#ifdef	PARANOID
	v_bufck();
#endif	PARANOID
	if (*state & SAWHELLO)
	{
		I_HEARD_YOU;
		*state =& ~ SAWHELLO;
		return;
	}
	if (*state & SENDHELLO)
	{
		HELLO;
		hellounacked++;
		*state =& ~SENDHELLO;
		return;
	}
	if ((*state & UP) == 0)
		return;
	/* Proceed below if we are UP&TRYING -- I just didn't want */
	/* to indent that much.... */

	fillochans();
	for (i = 0; i < NCHNS; i++)
	{
		vdhosnd++;
		vdhosnd =& NCHNS-1;
		if (vdhouse[vdhosnd] == 0) {
			p = vdhobuf [vdhosnd];
			p [0] = (p [0] & ~V_ACKBITS) | vdhoack;
			vdhsnd (p);
			vdhouse[vdhosnd] = HZ*100/1000;	/* hold off 100 ms */
			*state =& ~ACK2SND;
			return;
		}
	}
		/* No data to be sent */
	if (*state & ACK2SND) {	/* And an ack to send? */
		OACK;
		*state =& ~ACK2SND;
	}
}

#ifndef UCBUFMOD
struct	buf vdbuf;
#endif UCBUFMOD
#ifdef UCBUFMOD
struct netbuf *vdhmsg;
#endif UCBUFMOD

fillochans ()
{
	register int	*p;
	register char	*bp;
	register int	len;

	while (vdhouse[vdhofill] < 0) {
#ifndef UCBUFMOD
		if (vdbuf.b_dev==0)
		{
			bp = impotab.d_actf;
			if (bp==0)
				return;
			impotab.d_actf = bp->b_forw;
			bytesout (&bp->b_dev, &vdbuf, BUFSIZE, 1);
			vdbuf.b_wcount = msglen (vdbuf.b_dev);
			len = 4;
		}
		else
			len = min (VDHDSIZE, vdbuf.b_wcount);
		bytesout (&vdbuf.b_dev, p = &vdhobuf[vdhofill][1], len, 1);
		vdbuf.b_wcount =- len;
		if (vdbuf.b_wcount<0)
			v_pan ("Bad wcount");
#endif UCBUFMOD
#ifdef UCBUFMOD
		if(vdhmsg==0) {		/* must pick up new message? */
			if((bp = impoq) == 0)	/* if nothing to output */
				return;		/* just go away */
			bp->b_resv =| b_eom;	/* so I'm paranoid */
			do {		/* remove first message from q */
				bp = bp->b_qlink;
			} while( (bp->b_resv&b_eom) == 0 );
			/* now pointing at last buffer of msg */
			if(bp == impoq) {	/* only message in q? */
				impoq = 0;	/* clear q pointer */
			} else {
				len = impoq->b_qlink;	/* use len as temp */
				impoq->b_qlink = bp->b_qlink;
				bp->b_qlink = len;
			}
			vdhmsg = bp; len = 4;	/* only header in first packet */
		} else
			len = VDHDSIZE;
		len =- bytesout(&vdhmsg, p = &vdhobuf[vdhofill][1], len, 1);
#endif UCBUFMOD
		*--p = vdhofill;
		len =>> 1;
		p->hibyte = len;
#ifndef UCBUFMOD
		if (vdbuf.b_wcount==0)
#endif UCBUFMOD
#ifdef UCBUFMOD
		if (vdhmsg==0)
#endif UCBUFMOD
			*p =| V_LAST;
		if ( (prvictl>>vdhofill) & V_C0ACK )
			*p =| V_EVNODD;
		swabuf (&p[1], len);
		vdhouse[vdhofill] = 0;
		vdhofill++;
		vdhofill =& NCHNS-1;
		vs.opaks++;
	}
}

#ifndef UCBUFMOD
msglen(abp)	char *abp;
{
	register char	*bp;
	register int	cnt;

	bp = abp;
	if(bp==0)	return 0;
	for( cnt = bp->b_len; (bp=bp->b_qlink)!=abp; )
		cnt =+ bp->b_len&0377;

	return cnt;
}
#endif UCBUFMOD

impldrd ()
{
	spl_imp ();

	if (!(vstatebits & VDHILAST))
		log_to_ncp ("Bad impldrd");
	else
	{
		impleader = 1;
		impi_msg = 0;		/* can a buffer get lost here ? */
		vstatebits =& ~VDHILAST;
	}
}

vdh1snd (oneliner)  int oneliner;
{
	static vdh1buf;		/* static because we dma out of here */
	vdh1buf = oneliner;

	vdhsnd (&vdh1buf);
}

vdhsnd (buffer)  int *buffer;
{
	vstatebits =| OUTBUSY;
	if ((buffer[0]&V_HTOI) == 0) v_pan ("vdhsnd");
	VDH->cwao = buffer; 
#ifndef	VDHE
	VDH->wco = (buffer->hibyte & 077)+1;
#endif	not VDHE
#ifdef	VDHE
	VDH->wco = -(((buffer->hibyte & 077)+1)<<1);
#endif	VDHE
	VDH->csro =& ~ V_EOTINT; 
	VDH->csro =| V_GO;
}
