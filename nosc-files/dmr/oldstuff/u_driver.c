#
#include "../../h/param.h"
#include "../../user.h"
#include "../../buf.h"
#include "../../conf.h"
#include "./dc11h.h"

	/* defines for bits in state and flag words */
#define REPORT_CARRIER	001000
#define HAD_TMO		002000
#define PHO_DEAD	004000
#define READ		004000
#define CARR_ON		040000
#define WOPEN		000001
#define ENDWRITE	000002
#define RAW_MODE	000040

	/* defines for bits in transmsitter and reciever status words */
#define READY		000200
#define CARRIER		000004
#define CARRIER_TRANS	040000
#define RING_IND	020000
#define INT_ENABLE	000100
#define SPEEDH		000010
#define SPEEDL		000030
#define STOP_CODE	000400
#define DSREADY		000001
#define CLEAR_SEND	000002
#define REQ_SEND	000001

#define DCPRI		8
#define dc11h_bufl	512
#define tmo_time	10
#define true		1
#define false		0
#define EOT		004
#define SOT		001
#define ESC		033


dchopen(dev,flag) {
	extern dchtimer();
	dc11h.t_addr = DC11ADDR + dev.d_minor * 8;
	dchclose();				/* make sure flags are reset */
	if (dc11h.outbuf != 0) return;		/* check for multiple opens */
	dc11h.t_state = WOPEN;
	dc11h.t_addr->dcrcsr = SPEEDH|INT_ENABLE;
	spl6();
	while((dc11h.t_state & READ) == 0) sleep(&dc11h,DCPRI);
						/* wait for phone to ring */
	dc11h.t_addr->dctscr = SPEEDH|STOP_CODE;
	dc11h.t_addr->dcrcsr =| DSREADY; /* turn on the phone */
	dc11h.nxtc = dc11h.lastc = 0;
	spl0();
	dc11h.outbuf = getblk(NODEV);
	dc11h.time_left = 120;
	timeout(dchtimer,0,30);
}



dchclose() {
	dc11h.t_addr->dcrcsr = 0;
	dc11h.t_addr->dctscr = 0;
	dc11h.t_state = dc11h.t_flags = 0;	/* clear flags */
	dc11h.time_left = -1;			/* turn off timer */
	if (dc11h.outbuf != 0) {
		brelse(dc11h.outbuf);
		dc11h.outbuf = 0;
	}
}



dchwrite() { 					/* controls write to phone */
	if (dchwait()) return;			/* wait until we can do the write */
	dc11h.t_addr->dcrcsr =& ~INT_ENABLE;	/*turn off read interrupts */
	dc11h.t_state =& ~(READ|ENDWRITE);	/* show we are in write state */
	dc11h.nxtc = dc11h.outbuf->b_addr;	/* set up internal buffer */
	if (u.u_count > dc11h_bufl) u.u_count = dc11h_bufl;
	dc11h.lastc = dc11h.nxtc + u.u_count;
	iomove(dc11h.outbuf,0,u.u_count,B_WRITE); /* move data */
	dchstart();				/* start the write */
}



dchwait() {					/* wait for previous write to complete */
	spl6();					/* turn up priority level */
	for(;;) {
		if (dc11h.t_flags & HAD_TMO) {
			dc11h.t_flags =| PHO_DEAD;
			spl0();
			return(1);
		}
		if (((dc11h.t_state & READ) != 0) && ((dc11h.t_state & CARR_ON) == 0)) {
			spl0();
			return(0);
		}
		sleep(&dc11h,DCPRI);		/* wait for something to happen */
	}
}



dchstart() {					/* starts up or continues a write operation */
	register int stat;
	stat = dc11h.t_addr->dctscr;		/* get status word */
	if ((stat & READY) && (stat & CLEAR_SEND)) /* check if ready to go already */
		dc11h.t_addr->dctbuf = *dc11h.nxtc++;
	else
		dc11h.t_addr->dctscr =| REQ_SEND|INT_ENABLE;
	dc11h.time_left = tmo_time+8;
}



dchxint() {					/* transmitter interrupt handeler */
	if (dc11h.nxtc < dc11h.lastc) dchstart(); /* if more data, keep going */
	else {
		dc11h.t_state =| READ|ENDWRITE;	/* say we are in read state */
		dc11h.time_left = 0;
		wakeup(&dc11h);			/* allow read or anotherwrite to start */
	}
}



dchread() {					/* process read from user */
	register char ch,
		      did_one;			/* set when character has been passed */
	did_one = 0;
	for(;;){
		spl6();				/* bump our priority */
		while((ch = getc(&dc11h.t_inq)) < 0) { /* look for character */
			if (did_one || (dc11h.t_flags & (REPORT_CARRIER|HAD_TMO|PHO_DEAD)) != 0) {
				spl0();
				return;
			}
			if (dc11h.t_state & ENDWRITE) { /* check if we must turn line around */
				dc11h.t_addr->dctscr =& ~(REQ_SEND|INT_ENABLE);
				dc11h.t_addr->dcrcsr =| INT_ENABLE;
				dc11h.time_left = tmo_time;
				dc11h.t_state =& ~ENDWRITE;
			}
			sleep(&dc11h,DCPRI);
		}
		spl0();
		if (passc(ch) < 0) return;
		did_one = 1;
	}
}



dchrint() {					/* read interrupt */
	register int csr;
	csr = dc11h.t_addr->dcrcsr;
	if ((dc11h.t_state & WOPEN) == 0) return; /* make sure we should pay attention to phone */
	if (csr & RING_IND) {			/* phone is ringing, allow open to answer */
		dc11h.t_state =| READ;
		wakeup(&dc11h);
	}
	if ((csr & READY) != 0) {		/* we have a character */
		putc(dc11h.t_addr->dcrbuf & 0177, &dc11h.t_inq);
	}
	if ((csr & CARRIER) == 0) {
		dc11h.time_left = tmo_time;
		dc11h.t_state =& ~CARR_ON;
		if ((csr & CARRIER_TRANS) != 0) {
			dc11h.t_flags =| REPORT_CARRIER;
			wakeup(&dc11h);		/* wake up write, if pending */
		}
	}
	else {
		dc11h.t_state =| CARR_ON;
		dc11h.t_flags =& ~(REPORT_CARRIER|HAD_TMO|PHO_DEAD);
		dc11h.time_left = 0;		/* halt timer */
	}
	wakeup(&dc11h);
}



dchtimer() {					/* handles timeouts */
	if (dc11h.time_left > 0) {
		if (--dc11h.time_left == 0) {
			dc11h.t_flags =| HAD_TMO;
			wakeup(&dc11h);
			dc11h.time_left = tmo_time;
		}
	}
	else if (dc11h.time_left < 0) return;	/* turn off timer */
	timeout(dchtimer,0,30);
}



dchsgtty(dev,av) int *av; {
	register int *v;
	v = av;
	if (v) {
		*v++ = dc11h.t_speeds;
		*v++ = 0;
		*v++ = dc11h.t_flags;
		dc11h.t_flags =& ~(REPORT_CARRIER|HAD_TMO|PHO_DEAD);
	}
	else {
		dc11h.t_speeds = u.u_arg[0];
		dc11h.t_addr->dcrcsr =& ~030;
		dc11h.t_addr->dctscr =& ~030;
		if (dc11h.t_speeds == 9) {
			dc11h.t_addr->dcrcsr =| SPEEDH;
			dc11h.t_addr->dctscr =| SPEEDH;
		}
		else {
			dc11h.t_addr->dcrcsr =| SPEEDL;
			dc11h.t_addr->dctscr =| SPEEDL;
		}
	}
	return;
}
