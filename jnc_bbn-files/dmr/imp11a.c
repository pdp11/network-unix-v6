#
/* IMP-11a driver -- multi-device imp interface handler
 *
 *    Bolt Beranek and Newman: Jack Haverty 7/8/77
 * Modified 13 Apr 79 to separately enable read and write for await 
 *
 */

/* define the symbol DEBUG to get printouts on system console
 */


#include "../h/param.h"
#include "../h/inode.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/user.h"

/* bits in IMP-11A CSRs */
#define error 0100000   /* error flag */
#define ready 0200      /* device ready */
#define ienable 0100    /* interrupt enable */
#define xmem 060        /* extended memory mask */
#define eom 04          /* end-of-message in tx CSR */
#define nex 040000      /* non-ex-mem error flag */
#define clear 02        /* interface clear status */
#define go 01           /* start DMA */
#define reom 04000      /* end-of-message in rx CSR */
#define impnrdy 02000   /* IMP down in rx CSR */
#define rdyerr 01000    /* ready-line error flag in rx CSR */
#define hmr 04          /* host ready in RX csr */
#define rxgo 011        /* rx -- start DMA and write-enable */

#define doit ienable|eom|go|reom|hmr|rxgo /* note that this code works
		for both a normal read and a normal write */

#define spl spl5        /* hardware priority level is 5 */
#define BNCPRI -10      /* priority for bounce timeout (no abort) */
#define WAITPRI 10      /* priority for waiting, allows aborts */
#define BNCDELAY 180    /* # 60ths to allow for relay bounce */
#define NIMPS 2         /* number of IMP-11As out there */
#define IMPRSIZ 19      /* number of words of read-buffer in kernel */
#define IMPSECS 60      /* default number of seconds for timeouts */

/* structure for device registers */
struct devloc
  { int wcr;    /* word count register  */
    int addr;   /* buffer address */
    int csr;    /* control and status register */
  };

/* each IMP-11a uses a structure called an imptab for state
   information */
struct imptab
  {  int impflag ;      /* bits for state info */
     char impdev ;      /* minor device code, right-shifted 1 bit */
     char imptout ;     /* timeout in seconds (max 128 seconds) */
     char impchidx ;    /* index into impdma of next char to take out */
     char impchcnt ;    /* count of # chars valid in impdma */
     char impdma[IMPRSIZ*2] ;   /* kernel DMA buffer for input */
     struct buf imprbuf ;       /* read buffer for user-core DMA */
     struct buf impwbuf ;       /* write buffer for user-core DMA */
     struct devloc *imprx, *imptx ;     /* device registers */
     char *impitr ;     /* 0, or pointer to await table (itab) read inode */
     char *impitw ;     /*, same for write side */
  } imp[NIMPS] ;

/* flags in impflag word */
#define open 01         /* if set, both sides are open */
#define rxopen 02       /* rx has been opened */
#define txopen 04       /* tx has been opened */
#define anyopen (rxopen|txopen) /* either side open mask */
#define abort 010       /* ready-line-error came up, or timeout 
			   if on, must close both sides and reopen */
#define rxhere 020      /* DMA on kernel buffer has completed --
			   there is a packet ready to receive */
#define init 040        /* on when initializing the interface */
#define iokill 0100     /* timeout flag */
#define rxact 0200      /* read DMA is in progress to user core */
#define rxactk 0400     /* read DMA is in progress to impdma */
#define txact 01000     /* write DMA is in progress from user core */
#define rxeom 02000     /* read DMA halted with eom set */
#define rxeomu 04000    /* eom flag for user, cleared on entering read */

/* flags to decode reason for EIO error */
#define readyline 0100000      /* ready line down */
#define watchdog 040000       /* watchdog got hungry */
#define driver 020000  /* error in kernel DMA or somewhere (system bug ) */

/* table of device addresses, entries are rx1, tx1, rx2, tx2, etc. */
/* copied into imprx and imptx elements of imptab */
struct devloc *impadr[] {
	0172430, /* RX side of IMP-11A #1 */
	0172410, /* corresponding TX side */
	0172470, /* RX side of IMP-11A #2 */
	0172450  /* corresponding TX side */
	} ;

/*
 * impopen -- open routine.  Only one process can open for reading
   and one for writing.
 */
impopen(dev, flag)
   {register int *ip ;
    register struct imptab *i ;
    register struct devloc *rxadr ;
    int opmask, f, dir, devr, wakeup() ;
    dir=dev.d_minor & 01 ;       /* even for reads, odd for writes */
    devr=(dev.d_minor & 0376)>>1 ;    /* index into tables */

    /* check for user opening wrong device, or device being aborted */
    if ((devr>(NIMPS-1))||(dir^(flag?01:00))) /* invalid device code */
       {u.u_error=ENXIO ; return ;}
    i = &imp[devr] ;       /* the table for this IMP-11A */
    opmask = dir?txopen:rxopen;
    if (i->impflag & opmask) {u.u_error = EBUSY ; return ; }
    while (i->impflag & init) sleep(&impopen, WAITPRI);
    f = i->impflag ;
    if (f&abort) {u.u_error = EIO ; return ;}
    if (!(f&anyopen))	/* first open initializes the interface */
       {ip = &impadr[devr*2];
	i->imprx = rxadr = *ip++ ;
	i->imptx = *ip ;
	(*ip)->csr = clear ; (*ip)->csr = 0 ;
	rxadr->csr = clear|hmr ;
	i->impitr = 0 ;         /* no awaiters at first */
        i->impitw = 0 ; 
	i->impflag = init ; /* also will clear any misc. bits */
	i->impdev = devr ;
	i->imptout = IMPSECS ;
	timeout (&wakeup, &impopen, BNCDELAY); /* for bounce */
	sleep (&impopen,BNCPRI) ;
	rxadr->csr = hmr ; /*ready line settled by now, hopefully */
#ifdef DEBUG
	printf("IMP-11A %d init.\n",devr+1) ;
#endif
	i->impflag = 0 ;}
    else rxadr=i->imprx ;

    /* check to see if we can open */
    if (rxadr->csr & (rdyerr|impnrdy))
       {i->impflag=|readyline ; u.u_error = EIO; return ; }
    i->impflag =| opmask ;
    if ((i->impflag&anyopen) == anyopen)
	{impwatch(i) ; /* start fido */
	 imprdgo(i) ; /* start dma */
	 i->impflag =| open ;	/* indicates fully open */
#ifdef DEBUG
	 printf("IMP-11A #%d opened.\n",devr+1) ;
#endif
        }
   } ;

/*
 * Close */
impclose(dev)
   {register int devr ;
    register struct imptab *i ;
    register int dir ;
    dir=dev.d_minor & 01 ;       /* even for reads, odd for writes */
    devr=(dev.d_minor & 0376)>>1 ; /* index into tables */
    i = &imp[devr] ;
    if (i->impflag & open)
       {i->impflag =& ~open;
#ifdef DEBUG
        printf("Beginning close of IMP-11A #%d\n",devr+1);
#endif
       }
    i->impflag =& ~(dir?txopen:rxopen) ;
    if (i->impflag & anyopen) return ;
#ifdef DEBUG
    printf("Finishing close of IMP-11A #%d.\n",devr+1) ;
#endif
    impabort(i) ;          /* clears the interface */
    i->impflag = 0;        /* also clears abort if any */
  };

/*
 * General strategy routine */
impstrat(abp)
    struct buf *abp ;
   {register struct imptab *i ;
    register struct devloc *devadr ;
    register struct buf *bp ;
    int dir, xmask ;
    bp=abp ;
    dir=bp->b_dev&01 ;
    xmask = dir?txact:rxact ;
    i = &imp[(bp->b_dev&0376)>>1] ;
    if (i->impflag & xmask) 
       {u.u_error = EBUSY ;
        return ;
       }
    if ((i->imprx)->csr & rdyerr)
       {u.u_error=EIO ;
        i->impflag =| readyline ;
	impabort(i) ;
        return ;
       }
    devadr=dir?(i->imptx):(i->imprx) ;
    devadr->wcr=bp->b_wcount ;
    devadr->addr=bp->b_addr ;
    i->impflag =& ~iokill ; /* hold the watch dog at bay */
    i->impflag =| xmask ; /* but tell him we are vulnerable */
    devadr->csr = doit|((bp->b_xmem & 03) << 4) ;
   };

/*
 * Write routine (the simpler side) */
impwrite(dev)
{register struct imptab *i ;
 register int f ;
 i = &imp[(dev&0376)>>1] ;
 f=i->impflag ;
#ifdef DEBUG
 printf("Imp write, f:%o\n",f);
#endif
 if ((f&abort) || (!(f&open))) {u.u_error = EIO ; return ;}
 physio(impstrat,&i->impwbuf,dev,B_WRITE) ;
};

/*
 * Read routine */
impread(dev)
   {register struct imptab *i ;
    register int f ;
    int devr;
    devr = (dev&0376)>>1 ;
    i = &imp[devr] ;
    f=i->impflag ;
    if ((f&abort) || (!(f&open))) {u.u_error = EIO ; return ;}
#ifdef DEBUG
    printf("Imp read, #%d, f:%o, cnt:%d\n",devr+1,f,i->impchcnt);
#endif
    if (!(f&rxhere)) return;      /* no data here yet */
    i->impflag =& ~rxeomu ; /* turn off eom flag */
    while (i->impchcnt-- > 0)
	  {if (passc(i->impdma[i->impchidx++])) goto rdout ;
	  }
    if (!(f&rxeom)) /* kernel DMA got entire message? */
       {
#ifdef DEBUG
	printf("Beginning user read.\n");
#endif
	physio(impstrat,&i->imprbuf,dev,B_READ) ;
	f=i->impflag ; /* may have changed */
#ifdef DEBUG
	printf("User read complete.\n");
#endif
       }
rdout:
    if (f&rxeom) i->impflag =| rxeomu ; /* for user to look at */
    if (!(f&abort)) imprdgo(i) ;        /* DMA for next packet */
   } ;

/* Read interrupt handler */
impint(dev) /*dev -- minor device code (even read, odd write) */
   {register struct imptab *i ;
    register struct devloc *devadr ;
    register struct buf *bp ;
    extern int awake() ;
    int dir, f ;
    i = &imp[dev>>1] ;
    devadr=((dir=dev&01)?(i->imptx):(i->imprx)) ;
    f = (i->impflag =& ~iokill) ;
    if (f&abort) return ;
    bp = dir?(&i->impwbuf):(&i->imprbuf) ;
    if ((devadr->csr & error & nex) && (devadr->addr == 0))
       {devadr->addr = 0 ; /* page boundary, restart */
	devadr->csr = doit|(((bp->b_xmem + 1) & 03) << 4) ;
	return ; }
    if (devadr->csr & error)
       {if ((!dir) && rxactk)
	   {i->impflag=|driver ; impabort(i) ; return ;}
	else {bp->b_flags = B_ERROR ; iodone(bp) ; return ; }}
    /* transfer completed, set up for return */
    if ((!dir) && f&(rxact|rxactk) && devadr->csr&reom)
	i->impflag =| rxeom ; /* indicates eom to user */
    if ((dir && (f&txact)) || ((!dir) && (f&rxact)))
       {i->impflag =& ~(dir?txact:rxact) ;
	bp->b_resid = devadr->wcr ;
	iodone(bp) ; return ; }
    if ((!dir) && (f&rxactk))
       {i->impchcnt=(IMPRSIZ+devadr->wcr)*2 ;
	i->impchidx=0 ; /* index into impdma where read went */
	i->impflag =& ~(rxactk|iokill) ;
#ifdef DEBUG
	printf("\nImpint, dev:%o, cnt:%d",i->impdev,i->impchcnt);
#endif
	/* awake waiting process(es) */
	if (i->impitr != 0) awake(i->impitr, 0);
	i->impflag =| rxhere ; }
    if (dir && i->impitw != 0) awake(i->impitw, 0);
    } ;

/* imprdgo -- starts DMA to kernel buffer for receiving header */
imprdgo (ii)
    struct imptab *ii ;
    {register struct imptab *i ;
     register struct devloc *raddr ;
     i=ii ;
     raddr=i->imprx ;
     raddr->wcr = -IMPRSIZ ;
     raddr->addr = &i->impdma[0] ;
     i->impflag=|rxactk ;
     i->impflag=& ~(rxeom|rxhere|iokill) ;
#ifdef DEBUG
     printf("Beginning kernel DMA.\n");
#endif
     raddr->csr=doit ; /* start DMA */
    };

/* 
 * impabort -- clear the interface, wakeup anyone waiting
 */
impabort (ii)
   struct imptab *ii ;
   {register struct imptab *i ;
    register struct buf *bp ;
    i=ii ;
    i->impflag =| abort ;
    (i->imprx)->csr = clear ; (i->imprx)->csr = 0 ;
    (i->imptx)->csr = clear ; (i->imptx)->csr = 0 ;
    bp = &i->imprbuf ; iodone(bp) ; bp->b_flags=|B_ERROR ;
    bp = &i->impwbuf ; iodone(bp) ; bp->b_flags=|B_ERROR ;
    /* awake waiting process(es) */
    if (i->impitr != 0) awake(i->impitr, 0);
    if (i->impitw != 0) awake(i->impitw, 0);
   } ;


/* 
 * impwatch -- the watchdog, sometimes known as fido.
   uses flag bit iokill in impflag.  Whenever he runs, if iokill
   is on, and rxact or txact is on indicating the interface is active,
   he will clear the interface and set the abort bit in impflag.
   Also triggered if ready-error is on, regardless of state of other bits.
*/
impwatch (ii)   /* runs at priority 5 (callout from clock) */
   struct imptab *ii ;
   {register struct imptab *i ;
    register int f ;
    i=ii;
    if (((f=i->impflag)&abort) || (!(f&(rxopen|txopen)))) return ;
    if ((i->imprx)->csr & rdyerr)
       {i->impflag=|readyline ;
	impabort(i) ;
#ifdef DEBUG
	printf("IMP-11A #%d ready-line error.\n",ii->impdev + 1);
#endif
	return ;}
    if (f&(rxact|txact)) /* user I/O is active */
       {if (f&iokill) /* timeout */
	   {i->impflag=|watchdog ;
#ifdef DEBUG
	    printf("Watchdog timeout, IMP-11A #%d\n",i->impdev + 1);
#endif
	    impabort(i) ;
	    return ;}
	i->impflag =| iokill ; /* better be cleared before we look again */ }
    timeout(&impwatch,i,60*i->imptout) ; /* reschedule */
   };


/* 
 * status entry for sgtty.
   -- returns 1/ impflag word 2/ imptout value
   -- sets variables.  Arg 1 is ORed into impflag (mostly for
      setting dirchk.  Arg 2 is used to set imptout value.
 */

impsgtty(dev,v)
  int *v ;
  {register struct imptab *i ;
   i = &imp[(dev&0376)>>1] ;
   if (v==0)
      {i->impflag =| u.u_arg[0] ;
       i->imptout = u.u_arg[1] ;
      }
   else
     {*v++ = i->impflag ;
      *v++ = i->imptout ;
     }
  } ;


/* 
 * Await and capacity functions
 */

impcap(iip,vv)
struct inode *iip;
int *vv;
{       struct imptab *i;
	register int f, *v;

	v=vv;
	i = &imp[((iip->i_dev.d_minor)&0376)>>1] ;
	f = i->impflag;
	if ((f&abort) || ((f&(txopen|rxopen)) != (txopen|rxopen)))
	    {*v = 0; v++; *v = 0;}
	else
	    {*v = (f&rxhere) ? -1 : 0 ;         /* read is 0 or -1 */
	     v++;
	     *v = -1 ;                          /* writes always allowed */
	    }
}


impawt(type,iip,fd)
char type;
struct inode *iip;
int fd;

{
	register struct inode *ip;
	register struct imptab *i;
        int dir, itp;

	ip = iip;
        dir = ip->i_addr[0].d_minor;
	i = &imp[(dir&0376)>>1];
        dir = dir&01 ;

	switch(type)  {

		case  'e':

			     if (!(i->impflag&open)) u.u_error = EIO;
				 else { itp = ablei(type,0,ip,fd) ;
					if (dir) i->impitw = itp ;
					  else i->impitr = itp;
				      }
			     break;

		case  'd':
			     ablei(type,0,ip,fd) ;
			     if (dir) i->impitw = 0 ;
				 else i->impitr = 0 ;
			     break;
		}
}


