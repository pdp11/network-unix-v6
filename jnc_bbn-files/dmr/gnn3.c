#
/*
 *
 *
 *      Genisco display driver
 *
 *              BBN-version (supports 3 graphic processors)
 *
 *      Kevin Hunter                        October 1, 1978
 *
 *
 */

#include "../h/param.h"                                 /* global defs      */
#include "../h/conf.h"
#include "../h/buf.h"
#include "../h/user.h"
#include "../h/old_tty.h"
#include "../h/proc.h"


#define NGENISCO        3                               /* number of pgp's  */

#define BUSY           01                               /* buffer state flgs*/
#define WAITING        02

#define GNPRI           1                               /* sleep priority   */



struct gntype {                                         /* per pgp structure*/
	int     gnopen;                                 /*  open count      */
	char    *gnbp;                                  /*  buffer ptr.     */
	char    *gnbuf;                                 /*  buffer data ptr.*/
	char    *gnenbuf;                               /*  extra defn ptr. */
	char    gnpc;                                   /*  pgp PC save area*/
	char    gnstate;                                /*  buffer state    */
} gn[NGENISCO];


struct {                                                /* interface regs.  */
	int     noop;                                   /*  function reg    */
	int     strt;                                   /*  function reg    */
	int     clr;                                    /*  function reg    */
	int     iopls;                                  /*  function reg    */
	int     doc;                                    /*  data reg        */
	int     dob;                                    /*  data reg        */
	int     doa;                                    /*  data reg        */
	int     nio;                                    /*  data reg        */
};


int     GENADDR[NGENISCO]                               /* interface addrs. */
       {
	0167750,                                         /*  genisco #0      */
	0167710,                                         /*  genisco #1      */
	0167730                                          /*  genisco #2      */
};


gnopen(dev)
{
	register struct gntype *gnptr;

	gnptr = &gn[dev.d_minor >> 1];                  /* find struct ptr. */

	gnptr->gnopen =| (dev.d_minor & 1) + 1;         /* set open flag    */

	if(gnptr->gnbp == 0) {                          /* if no data block */
		gnptr->gnbp = getblk(NODEV);            /*  get fresh block */
		gnptr->gnbuf = gnptr->gnbp->b_addr;     /*  extract data ptr*/
		gnptr->gnenbuf = &gnptr->gnbuf[512-26]; /*  room for defn   */
	}
}


gnclose(dev)
{
	register struct gntype *gnptr;

	gnptr = &gn[dev.d_minor >> 1];                  /* find struct ptr. */

	if(!(gnptr->gnopen =& ~((dev.d_minor & 1) + 1))) { /* clr open flag */
		gnptr->gnstate = 0;                     /*  clr buffer state*/
		if(gnptr->gnbp != 0) {                  /*  if data remains */
#ifdef BUFMOD
			brelse(gnptr->gnbp,&bfreelist);
#endif
#ifndef BUFMOD
			brelse(gnptr->gnbp);            /*   output block   */
#endif BUFMOD
			gnptr->gnbp = 0;                /*   flag no_block  */
		}
	}
}

gnwait(gnptr_val)
{
	register struct gntype *gnptr;

	gnptr = gnptr_val;                              /* get struct ptr.  */

	spl5();                                         /* priority 5       */
	while ((gnptr->gnstate) & BUSY) {               /* while buffer busy*/
		gnptr->gnstate =| WAITING;              /*  we are waiting  */
		sleep(gnptr->gnbp, GNPRI);              /*  sleep til wakeup*/
	}
	gnptr->gnstate =& ~WAITING;                     /* we are'nt waiting*/
	gnptr->gnstate =| BUSY;                         /* but we are busy  */
				/* we return at pri. 5  */
}


gnint(dev)
{
	register struct gntype *gnptr;
	register int gen;
	register int index;

	index = dev.d_minor >> 1;                       /* get pgp index    */

	gen = GENADDR[index];                           /* find device ptr. */

	gen->nio = 0;                                   /* clear interrupt  */
	gen->clr++;

	gnptr = &gn[index];                             /* find struct ptr. */

	wakeup(gnptr);                                  /* wakeup sleeper   */
}


gnread(dev)
{
	register struct gntype *gnptr;
	register int gen;
	register int index;

	int c, tc, n;                                   /* used to be regs. */

	index = dev.d_minor >> 1;                       /* get pgp index    */

	gen = GENADDR[index];                           /* find device ptr. */
	gnptr = &gn[index];                             /* find struct ptr. */

	switch(dev.d_minor & 1) {                       /* pgp or dsply mem?*/
case 0:         /* dump pgp memory */
		c = u.u_count;                          /* get byte count   */
		if(u.u_offset[1] + u.u_count >= 512) {

			if(u.u_offset[1] >= 512) return;/* eof              */

			c = 512 - u.u_offset[1];
		}
		gnwait(gnptr);                          /* returns at pri. 5*/

		gnptr->gnpc = gen->dob.hibyte & 0377;   /* save the pgp PC  */
		gen->noop++;

		gen->doa = (gnptr->gnbuf >> 1) & 077777;/* PDP-11 address   */
		gen->noop++;

		gen->dob = -256;                        /* word count       */
		gen->clr++;                             /* pgp -> PDP-11    */

		gen->doc = 0;                           /* pgp address      */
		gen->clr++;                             /* start autodump   */

		sleep(gnptr, GNPRI);                    /* wait til complete*/

		if(gen->dob != 0) u.u_error = EIO;      /* pgp cnt/PC != 0 ?*/
		gen->noop++;    /* formerly there was   */
				/* no function pairing  */
				/* to previous data reg.*/

		gen->doc = gnptr->gnpc;                 /* restore pgp pc   */
		gen->noop++;

		spl0();                                 /* priority 0       */
		iomove(gnptr->gnbp, u.u_offset[1], c, B_READ);  /* ship data*/
		break;                                          /* exit case*/

case 1:         /* read from pgp */
		gnwait(gnptr);                          /* returns at pri. 5*/
		while(u.u_count) {                      /* while data exists*/
			tc = c = min(u.u_count, 512);   /* constrain count  */

			if(c & 01) tc++;                /* even up count    */

			gen->doa = (gnptr->gnbuf >> 1) & 077777;/*PDP-11 adr*/
			gen->clr++;                     /* pgp -> PDP-11    */

			gen->dob = -(tc >> 1);          /* work count       */
			gen->strt++;                    /* start the pgp    */

			sleep(gnptr, GNPRI);            /* wait til complete*/

			if(n = gen->dob.lobyte)         /* residual count   */
				c = tc + (n | 0177400) << 1;

			iomove(gnptr->gnbp, 0, c, B_READ);      /* ship data*/
			if(n) break;                    /* quit if part dma */
		}
	}                                                       /* exit case*/
	spl0();                                         /* priority 0       */
	gnptr->gnstate =& ~BUSY;                        /* buffer not busy  */

	if(((gnptr->gnstate) & (WAITING | BUSY)) == WAITING)
		wakeup(gnptr->gnbp);                    /* wake up sleeper  */

}


gnwrite(dev)
{
	register struct gntype *gnptr;
	register int gen;
	register int index;

	int c, tc, n;                                   /* used to be regs. */

	index = dev.d_minor >> 1;                       /* get pgp index    */

	gen = GENADDR[index];                           /* find device ptr. */
	gnptr = &gn[index];                             /* find struct ptr. */

	switch(dev.d_minor & 1) {                       /* pgp or dsply mem?*/
case 0:         /* load pgp memory */
		c = u.u_count;

		if(c <= 0 || u.u_offset[1] >= 512) return; /* eof?          */

		if(u.u_offset[1] + c > 512)
			c = 512 - u.u_offset[1];

		n = u.u_offset[1];
		gnwait(gnptr);                          /* grab buffer      */
		iomove(gnptr->gnbp, u.u_offset[1], c, B_WRITE); /* move data*/

		if(n & 01) { c++; gnptr->gnbuf[--n] = '\0'; } /* odd boundry*/

		if((n + c) & 01) gnptr ->gnbuf[n + c++] = '\0'; /* odd count*/

		gen->doa = ((gnptr->gnbuf + n) >> 1) & 077777; /* PDP-11 adr*/
		gen->iopls++;                           /* PDP-11 -> pgp    */

		gen->dob = -(c >> 1);                   /* word count       */
		gen->noop++;

		gen->doc = n >> 1;                      /* pgp address      */
		gen->clr++;                             /* start autoload   */

		sleep(gnptr, GNPRI);                    /* wait til complete*/
		if(gen->dob != 0) u.u_error = EIO;      /* pgp cnt/PC != 0 ?*/
		gen->noop++;    /* formerly there was   */
				/* no function pairing  */
				/* to previous data reg.*/
		break;                                  /* exit case        */

case 1:         /* write to pgp */
		gnwait(gnptr);                          /* grab buffer      */
		while(u.u_count) {                      /* while data exists*/
			tc = c = min(u.u_count, 512);
			iomove(gnptr->gnbp, 0, c, B_WRITE); /* move data    */
			if(c & 01) gnptr->gnbuf[tc++] = '\0';/* cnt odd->pad*/

			gen->doa = (gnptr->gnbuf >> 1) & 077777;/*PDP-11 adr*/
			gen->iopls++;                   /* PDP-11 -> pgp    */

			gen->dob = -(tc >> 1);          /* word count       */
			gen->strt = 0;                  /* start pgp        */

			sleep(gnptr, GNPRI);            /* wait til complete*/

			n = gen->dob.lobyte;            /* residual count   */
			gen->noop++;
				/* formerly there was   */
				/* no function pairing  */
				/* to previous data reg.*/

		/*      printf("n = %d ucount = %d\n", n, u.u_ucount); */

			if(n) {                         /*get residual count*/
				u.u_count =- (((n|0177400) << 1) + (c & 01));
				break;
			}
		}
	}                                               /* exit case        */
	spl0();                                         /* priority 0       */
	gnptr->gnstate =& ~BUSY;                        /* buffer not busy  */

	if(((gnptr->gnstate) & (WAITING | BUSY)) == WAITING)/*buf not taken?*/
		wakeup(gnptr->gnbp);                    /* wakeup sleeper   */
}


gnsgty(dev,v)
int *v;
{
	register int gen;

	gen = GENADDR[dev.d_minor >> 1];

	if(v) {                                         /* gtty call        */
		v[0] = 0;

		v[1] = gen->doc;                        /* pgp instrctn reg */
		gen->noop++;

		v[2] = (gen->dob.hibyte) & 0377;        /* pgp PC           */
		gen->noop++;

		return;
	}

	switch(u.u_arg[0]) {                            /* stty call        */
case 0100001:   /* start pgp at address in u.u_arg[2] */

				/* address should be    */
				/* 0 <= ADDR <= 255     */
		if(u.u_arg[2].hibyte)                   /* bad address?     */
				gen->nio++;             /* null operation   */
			else
				gen->doc = u.u_arg[2];  /* set address      */
		gen->strt++;                            /* start pgp        */

		return;

case 0100002:   /* stop or single step pgp */

		gen->nio = 0;
		gen->iopls++;                           /* single step/stop */
				/* pgp executes one     */
				/* instruction and stops*/
		return;
	}
	u.u_error = EINVAL;                             /* bad arguement    */
}
