#
/*
 *      Genisco display driver
 *
 *      Bruce Borden				 4-Sep-77
 *		VTUNIX Modifications
 */

#include "../h/param.h"
#include "../h/conf.h"
#include "../h/buf.h"
#include "../h/user.h"
#include "../h/old_tty.h"
#include "../h/proc.h"

/* #define GNTTTY  1               /* define for v-terminal driver */
#define GNRAND  1               /* define for Rand Genisco */
/* #define GNHIRES   1             /* define for high-resolution config */
/* #define GNNOSC  1               /* define for NOSC Genisco */
/* #define RANDEBUG 1              /* horrible kludge for debugging */

struct {
	int gnopen;             /* open count */
	char *gnbp;             /* buffer pointer */
	char *gnbuf;		/* buffer data area */
	char *gnenbuf;          /* room for one more defn */
	char gnpc;              /* program counter of pgp (saved during dump) */
	char gnstate;           /* see states below */
} gn;


/* gnstate flags */

#define BUSY    01              /* Buffer busy flag */
#define WAITING 02              /* Someone waiting for the buffer */
#define GNTIO   04              /* io in progress from term device */

/* external page device registers */
struct {
	int noop;
	int strt;
	int clr;
	int iopls;
	int doc;
	int dob;
	int doa;
	int nio;
};

#define GEN     0167730         /* 0167750 for HI-RES B/W */

#ifdef GNNOSC
#ifdef GNHIRES
#define GEN     0167750
#endif
#endif

#define GNPRI   1               /* sleep priority - pretty high */


gnopen(dev)
{
	gn.gnopen++;
	if(gn.gnbp == 0) {
		gn.gnbp = getblk(NODEV);
		gn.gnbuf = gn.gnbp->b_addr;
		gn.gnenbuf = &gn.gnbuf[512-26]; /* 24 chars needed for defn */
	}                                       /* so have 2 for slop */
}

gnclose(dev)
{

	if(--gn.gnopen == 0) {
		gn.gnstate = 0;
		if(gn.gnbp != 0) {
#ifdef BUFMOD
			brelse(gn.gnbp,&bfreelist);
#endif
#ifndef BUFMOD
			brelse(gn.gnbp);
#endif BUFMOD
			gn.gnbp = 0;
		}
	}
}

/* Returns at Priority Level 5 with Genisco BUFFER not busy     */
/*      but with state flagged buffer BUSY                      */
gnwait()
{
	spl5();
	while (gn.gnstate&BUSY) {
		gn.gnstate =| WAITING;
		sleep(gn.gnbp,GNPRI);   /* wait on the buffer */
	}
	gn.gnstate =& ~WAITING;
	gn.gnstate =| BUSY;
	/* spl0(); */
}

gnint()
{
	GEN->nio = 0;	/* ack the interrupt, i hope */
	GEN->clr++;
#ifdef GNTTTY
	if (gn.gnstate&GNTIO) {
		gn.gnstate =& ~(GNTIO|BUSY);
		gntstart();
	} else
#endif
	wakeup(&gn);
}

gnread(dev)
{
	register c, tc, n;

	switch(dev.d_minor) {
case 0:		/* dump pgp memory */
		c = u.u_count;
		if(u.u_offset[1] + u.u_count >= 512) {
			if(u.u_offset[1] >= 512)
				return;		/* eof */
			c = 512-u.u_offset[1];
		}
		gnwait();       /* Returns at level 5 */
		gn.gnpc = GEN->dob.hibyte & 0377;     /* save the pgp pc */
		GEN->noop++;
		GEN->doa = (gn.gnbuf>>1)&077777; /* address */
		GEN->noop++;
		GEN->dob = -256;        /* word count */
		GEN->clr++;             /* set direction */
		GEN->doc = 0;		/* address */
		GEN->clr++;             /* start it */
		sleep(&gn, GNPRI);      /* wait for completion */
		if (GEN->dob != 0)
			u.u_error = EIO;
		GEN->doc = gn.gnpc;     /* restore the pgp pc */
		GEN->noop++;
		spl0();
		iomove(gn.gnbp, u.u_offset[1], c, B_READ);
		break;

case 1:		/* read from pgp */
		gnwait();
		while(u.u_count) {
			tc = c = min(u.u_count,512);
			if (c&01) tc++;
			GEN->doa = (gn.gnbuf>>1)&077777;
			GEN->clr++;
			GEN->dob = -(tc>>1);
			GEN->strt++;    /* start the pgp */
			sleep(&gn, GNPRI);      /* wait for completion */
			/* resid count */
			if (n = GEN->dob.lobyte)
				c = tc + (n|0177400) << 1;

			iomove(gn.gnbp, 0, c, B_READ);
			if (n) break;
		}
	}
	spl0();
	gn.gnstate =& ~BUSY;            /* Buffer no longer busy */
#ifdef GNTTTY
	gntstart();                     /* start term i/o if needed */
#endif
	if((gn.gnstate&(WAITING|BUSY)) == WAITING)
		wakeup(gn.gnbp);        /* wake them up */
}

gnwrite(dev)
{
	register tc, c, n;

	switch(dev.d_minor) {
case 0:		/* load */
		c = u.u_count;
		if(c <= 0 || u.u_offset[1] >= 512)
			return;
		if(u.u_offset[1] + c > 512)
			c = 512 - u.u_offset[1];
		n = u.u_offset[1];
		gnwait();
		iomove(gn.gnbp,u.u_offset[1],c,B_WRITE);

		/* Handle odd boundaries and counts */
		if (n&01)
		{       c++;
			gn.gnbuf[--n] = '\0';
		}
		if ((n+c)&01)
			gn.gnbuf[n + c++] = '\0';

		GEN->doa = ((gn.gnbuf+n)>>1)&077777;
		GEN->iopls++;
		GEN->dob = -(c>>1);
		GEN->noop++;
		GEN->doc = n>>1;
		GEN->clr++;              /* start it */
		sleep(&gn, GNPRI);      /* wait for completion */
		if (GEN->dob != 0)
			u.u_error = EIO;
		break;

case 1:		/* write */
		gnwait();
		while(u.u_count) {
			tc = c = min(u.u_count,512);
			iomove(gn.gnbp,0,c,B_WRITE);
			if (c&01) /* If count is odd, pad with null */
				gn.gnbuf[tc++] = '\0';
			GEN->doa = (gn.gnbuf>>1)&077777;
			GEN->iopls++;   /* set write */
			GEN->dob = -(tc>>1);
			GEN->strt = 0;
			sleep(&gn, GNPRI);      /* wait for completion */

			n = GEN->dob.lobyte;
		/*      printf ("n = %d ucount = %d\n",n,u.u_count); */

			if (n)
			{       u.u_count =- (((n|0177400)<<1) + (c&01));
				break;
			}
		}
	}
	spl0();
	gn.gnstate =& ~BUSY;            /* Buffer no longer busy */
#ifdef GNTTTY
	gntstart();                     /* start term i/o if needed */
#endif
					/* if anyone wants the buf */
	if((gn.gnstate&(WAITING|BUSY)) == WAITING)
		wakeup(gn.gnbp);        /* wake them up */
}

gnsgty(dev,v)
int *v;
{

	if(v) {		/* gtty */
		v[0] = 0;
		v[1] = GEN->doc;
		GEN->noop++;
		v[2] = GEN->dob.hibyte & 0377;
		GEN->noop++;
		return;
	}
	switch(u.u_arg[0]) {

case 0100001:	/* start */
		if(u.u_arg[2].hibyte)
			GEN->nio++;     /* bad addr - no set */
		else
			GEN->doc = u.u_arg[2];
		GEN->strt++;
		return;

case 0100002:	/* stop or single step */
		GEN->nio = 0;
		GEN->iopls++;
		return;

	}
	u.u_error = EINVAL;	/* bad argument */
}

#ifdef  GNTTTY                  /* Genisco Virtual Terminal Interface */


/************ Declarations for terminal devices *************/

#define	NGNWINS 16	/* # of windows pgp can handle      */

#ifdef GNHIRES
#define GNCWIDTH 12     /* Width of Genisco characters      */
#define GNCHEIGHT 24    /* Height of Genisco characters     */
#endif
#ifndef GNHIRES
#define GNCWIDTH 6      /* Width of Genisco characters      */
#define GNCHEIGHT 12    /* Height of Genisco characters     */
#endif

#define GNFLAG    0200  /* Flag char for Genisco escapes    */
#define GNSETWIN  0176  /* Genisco set window command is    */
                        /*  GNFLAG followed by GNSETWIN     */
#define HISPEED   14    /* use OPTIONA as speed             */
#define GNTLINES  42
#define GNTCOLS   85
#define GNQUANTA  50	/* Num chars output per window	    */

#ifdef GNNOSC           /* NOSC defns   *********************/
#define NGNCRT  3       /* Number of Genisco CRTs on sys    */
int gnplanes[NGNCRT] {   07,  070,  0700 } ;
#endif                                          /************/

#ifdef RANDEBUG         /* debug defns  *********************/
#define NGNCRT  3       /* Number of Genisco CRTs on sys    */
int gnplanes[NGNCRT] {   01,  02,  04 } ;
#endif                                          /************/

#ifdef GNRAND           /* RAND defns   *********************/
#define NGNCRT  1       /* Number of Genisco CRTs on sys    */
#ifdef GNHIRES
int gnplanes[NGNCRT] {  017 } ;
#endif
#ifndef GNHIRES
int gnplanes[NGNCRT] {  007 } ;
#endif
#endif                                          /************/

struct	tty gnt11[NGNCRT];
char	gntmap[NGNWINS];

extern struct vterm vt_structures[];

/************ End declarations for terminal devices *********/

gntopen(dev, flag)
{
	register struct tty *tp;
	register struct vterm *vt;
	extern gntstart(), gntwmap();

	if(dev.d_minor >= NGNCRT) {
		u.u_error = ENXIO;
		return;
	}
	tp = &gnt11[dev.d_minor];
	if((tp->t_state&ISOPEN) == 0) {
		tp->t_state = ISOPEN|CARR_ON|SSTART;
		tp->t_flags = CRMOD;
		tp->t_addr  = gntstart;
		tp->t_dev   = dev;
		tp->t_speed = HISPEED | (HISPEED<<8);
		gnopen(-1);             /* Get a sys buffer if needed */
		if(vt_open(tp))         /* turn into a vt */
			return;
		vt = tp->t_vterm;
		if(gntwmap(vt)) {
			u.u_error = ENOSPC;
			return;
		}
		vt->v_curw.w_lrhc.integ = vt->v_maxw.w_lrhc.integ =
			((GNTLINES-1)<<8)|(GNTCOLS-1);
		vt->v_flags =| LEVEL8; /* 8-Bit Data Path flag */
		vt->v_flags =& ~VTLOCK; /* Not KBD locked! */
		vt->v_caddr = gntwmap;
	}
}


gntclose(dev)
{
	register struct tty *tp;

	tp = &gnt11[dev.d_minor];
	tp->t_state = 0;
	gnwait();               /* wait for buf not busy */
	  gn.gnstate =& ~BUSY;  /* don't really want the buf at all */
	  spl0();
	vt_ttclose(tp);
	gnclose(-1);            /* just don't want this to give up */
				/*   active buffer */
}


gntwrite(dev)
{
	ttwrite(gnt11[dev.d_minor].t_vterm);
}

gntread(dev)
{
	ttread(gnt11[dev.d_minor].t_vterm);
}

gntsgtty(dev, v)
{
	register struct tty *tp;

	tp = &gnt11[dev.d_minor];
	if(vt_ttsgtty(tp->t_vterm, v) == 0)
		tp->t_flags =& ~(NLDELAY|TBDELAY|CRDELAY|VTDELAY);
}


gntstart()
{
	register struct tty *tp;
	register struct vterm *vt;
	register char *cp;
    /***int sps; ***/
	int windef[5], cnt, quant;
	static struct tty *curtty;
	char *cw;

	if(gn.gnstate&BUSY) return;
    /***sps = PS->integ;
	spl5();         ***/
	cp = gn.gnbuf;
	if((tp = curtty) == 0)
		if((tp = gntgwork()) == 0)
			goto out;       /* No Work for the nonce */
		else
			goto setwin;    /* This must be new window */

	do {

		if(tp->t_state&KBDSYNC) { /* Currently, this has no meaning */
			tp->t_state =& ~KBDSYNC;
			if(tp->t_kbd) {
				tp->t_screen = tp->t_kbd;
				if(tp->t_screen->v_outq.c_cc)
					goto setwin;
			}
		}

		if((vt = tp->t_screen) == 0 || vt->v_state&VT_CLOSED)
			goto next;              /* This screen unusable */

		if(vt->v_flags & VIRGIN)        /* window redefined! */
			goto setwin;

		quant = GNQUANTA;	/* smoothing factor */
		while(quant-- && (cnt = getc(&vt->v_outq)) >= 0) {
			if(cnt == 0 || cnt == GNFLAG) {
				*cp++ = GNFLAG;
				*cp++ = ~cnt;
			} else
				*cp++ = cnt;
			if(cp >= &gn.gnbuf[510]) /* leave room for 2 chars */
				goto send;
		}
		if(vt->v_state&VT_OSLEEP && vt->v_outq.c_cc <= TTLOWAT) {
			vt->v_state =& ~VT_OSLEEP;
			wakeup(&vt->v_outq);    /* wake oput blocked procs */
		}
   next:
		if(cp >= gn.gnenbuf || !(gnt_nxo(tp) || (tp = gntgwork())))
			break;                  /* No more to do */
   setwin:
		vt = tp->t_screen;
		*cp++ = GNFLAG;                 /* Select this window */
		if((*cp++ = vt->v_mapw) == 0)
			panic("Missing Mapped Genisco Window");
		if(vt->v_flags&VIRGIN) {        /* First time ?? */
			cw = &vt->v_curw;       /* Current Window def */
			windef[0] = cw->w_ulhc.cr_row * GNCHEIGHT;
			windef[1] = (cw->w_lrhc.cr_row + 1)
				    * GNCHEIGHT + windef[0];
			windef[2] = cw->w_ulhc.cr_col * GNCWIDTH;
			windef[3] = (cw->w_lrhc.cr_col + 1)
				    * GNCWIDTH + windef[2];
			windef[4] = gnplanes[tp->t_dev.d_minor];

			*cp++ = GNFLAG; *cp++ = GNSETWIN;
			for(cw = windef; cw < &windef[5];)
/* This line:
 *				if(*cw == 0 || *cw == GNFLAG) {
 * had sign extension problems... the following line only works
 * if GNFLAG == 0200!!
 */
				if((*cw & 0177) == 0) {
					*cp++ = GNFLAG;
					*cp++ = ~*cw++;
				} else
					*cp++ = *cw++;

			vt->v_flags =& ~VIRGIN; /* No Longer Virgin */
		}

		curtty = tp;

	} while(cp < gn.gnenbuf);               /* Glutton... eat till full */

 send:  if(cnt = cp - gn.gnbuf) {

		if(cnt&01) {
			*cp++ = 0;
			cnt++;
		}

#ifdef DEBUG
		VT_DB3 {
			printf("GNN:Writing %d:", cnt);
			for(cp=gn.gnbuf; cp<&gn.gnbuf[cnt];cp++) {
				if(((cp-gn.gnbuf)%10) == 0) printf("\n");
				printf(" %o", *cp&0377);
			}
			printf("\n");
		}
#endif

		GEN->doa = (gn.gnbuf>>1)&077777;
		GEN->iopls++;                   /* set write */
		GEN->dob = -(cnt>>1);
		GEN->strt = 0;
		gn.gnstate =| BUSY|GNTIO;  /* flag buf busy & term i/o  */
					   /*  in progress              */
	} else if(gn.gnstate & WAITING)    /* If no term i/o, start any */
		wakeup(gn.gnbp);           /* direct i/o if desired     */

  out:  ;
   /*   PS->integ = sps;  */
}


gntgwork()
{
	register struct tty *tp;
	register struct vterm *vt;

/*      Redundant code below...  mob:12/19/77 */
/*      for(tp = gnt11; tp < &gnt11[NGNCRT]; tp++)
		if((tp->t_state&ISOPEN) && vt_nxo(tp))
			return(tp);
*/
	for(tp = gnt11; tp < &gnt11[NGNCRT]; tp++)
		if((tp->t_state&ISOPEN) && gnt_nxo(tp))
			return(tp);
	return(0);
}


gntgvt(dev)
{
	int minor;

	if((minor = dev.d_minor) >= NGNCRT)
		return(0);
	return(gnt11[minor].t_vterm);
}

gnt_nxo(atp)
struct tty *atp;
{
	register struct tty *tp;
	register struct vterm *vt;

	tp = atp;
	if(vt = vt_nxo(tp))
		return(vt);
	if((vt = tp->t_screen)->v_outq.c_cc)
		return(vt);
	return(0);
}

gntwmap(avt)
struct vterm *avt;
{
	register struct vterm *vt;
	register int i;

	if((vt = avt) == 0)
		panic("Zero VT");
	if(i = vt->v_mapw) {
		gntmap[i - 1] = 0;
		vt->v_mapw = 0;
		return(0);
	} else {
		for(i = 0; i < NGNWINS; i++)
			if(gntmap[i] == 0) {
				gntmap[i] = (vt - vt_structures) + 1;
				vt->v_mapw = i + 1;
				return(0);
			}
		return(1);
	}
}
