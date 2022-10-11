#
/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */

#include "../h/param.h"
#include "../h/conf.h"
#include "../h/inode.h"
#include "../h/user.h"
#include "../h/tty.h"

#define NPTY 16      /* Number of pseudo-teletypes */
#define NODELAY 0377

/*
 * A pseudo-teletype is a special device which is not unlike a pipe.
 * It is used to communicate between two processes.  However, it allows
 * one to simulate a teletype, including mode setting, interrupt, and
 * multiple end of files (all not possible on a pipe).  There are two
 * really two drivers here.  One is the device which looks like a TTY
 * and can be thought of as the slave device, and hence its routines are
 * are prefixed with 'pts' (PTY slave).  The other driver can be
 * thought of as the controlling device, and its routines are prefixed
 * by 'ptc' (PTY controller).  To type on the simulated keyboard of the
 * PTY, one does a 'write' to the controlling device.  To get the
 * simulated printout from the PTY, one does a 'read' on the controlling
 * device.  Normally, the controlling device is called 'ptyx' and the
 * slave device is called 'ttyx' (to make programs like 'who' happy).
 *
 * Modified by Dan Franklin:
 *    Removed commented-out ptcwrite code; it will block RUBOUT
 *    after TTHIWAT chars typed, which is undesirable.
 * 9/11/78 made ptcopen turn on virtual carrier. Otherwise the sequence
 *    ptsopen - ptsopen - ptcclose - ptcopen leaves pty open with carrier off,
 *    which causes slave reads to return EOF, which causes getty to go away
 *    as soon as init forks it -- repeatedly!
 *    Also removed wakeup on rawq.c_cf, since ptcwrite no longer sleeps on it
 *    Added await and capacity stuff.
 * 9/27/78 ptsopen now sleeps on virtual carrier.  ptsclose, ptsread, ptswrite,
 *    ptcopen simplified accordingly. CHANGE: ptswrite now returns 0 on attempt
 *    to write when controller side closed. This is consistent with handling
 *    of "carrier" in other tty drivers, and means that random garbage from one
 *    (server telnet) session will not get received by the next. Changed await
 *    stuff to give controller side its own slot.
 * 1/4/78 BBN(dan) to fix stty bug: an stty on slave after
 *    controller closed called ttstart when t_addr == 0. Changed to not
 *    clear t_addr in ptcclose. Also changed ptsstart to check if controller
 *    closed and flushtty if so -- this assures that after controller closed,
 *    sleeping ttwrites will still complete.
 * 1/16/78 BBN(dan) to fix stty bug: stty on controller could lead to deadlock
 *    waiting for output to drain if the process doing the stty is the one
 *    that would drain the output. Fixed by imitating stty code in ttystty
 *    without wflushtty call. Also changed ptccap to return half of what it
 *    what it was before -- this allows for every char input being followed
 *    by \377 in ttyinput.
 * 3/22/79 BBN(dan) feature: ctrller can now tell whether slave open
 *    by examining speeds. Nonzero speed means open, zero speed means closed.
 * 4/9/79 BBN(dan): made use new await interface.
 *    Not very reliable, but sufficient for some applications..
 * 3/26/79 BBN(dan): Made empty() work on pty ctrller (ptcsgtty).
 * 4/25/79 BBN(dan): Deleted empty() line from ptcsgtty.
 * 5/3/79  BBN(dan): Made ptcopen set SSTART bit (it already set t_addr).
 *    Otherwise stty on ctrller with slave closed can crash system.
 *    Also made ptsopen set t_addr (it already set SSTART).
 */

struct
   {
   struct tty _XXXX;
   int t_itpc;          /* For await on controller side */
   } pty[NPTY];         /* TTY headers for PTYs */

/* ----------------------------- P T S O P E N ---------------------- */

ptsopen(dev,flag)
   {
   register struct tty *tp;
   register struct proc *pp;
   extern int pgrpct;
   extern ptsstart();

   if (dev.d_minor >= NPTY)      /* Verify minor device number */
      {
      u.u_error = ENXIO;         /* No such device */
      return;
      }
   tp = &pty[dev.d_minor];       /* Setup pointer to PTY header */
   if ((tp->t_state & ISOPEN) == 0)  /* Not already open? */
      {
      /* Mark as open and with special start routine */
      tp->t_state =| ISOPEN | SSTART;
      tp->t_addr = &ptsstart;
      tp->t_flags = 0;           /* No special features (nor raw mode) */
      tp->t_erase = CERASE;
      tp->t_kill = CKILL;
      }
   ttyopen(dev, tp);

   while((tp->t_state & CARR_ON) == 0)
      sleep(&tp->t_addr, TTIPRI);
   tp->t_speeds = (07<<8) | 07;     /* Set to 300 baud */
   }

/* ----------------------------- P T S C L O S E -------------------- */

ptsclose(dev)
   {
   register struct tty *tp;

   tp = &pty[dev.d_minor];       /* Setup pointer to PTY header */
   if (tp->t_state & CARR_ON)    /* Other side open? */
      wflushtty(tp);             /* Yes, wait for it */
   else
      flushtty(tp);              /* Otherwise, just blast it */
   tp->t_state =& ~ISOPEN;       /* Mark as closed */
   tp->t_speeds = 0;             /* So ctrller can find out */
   }

/* ----------------------------- P T S R E A D ---------------------- */

ptsread(dev) /* Read from PTY, i.e. from data written by controlling device */
   {
   register struct tty *tp;

   tp = &pty[dev.d_minor];     /* Setup pointer to PTY header */
   ttread(tp);
   }

/* ----------------------------- P T S W R I T E -------------------- */
/*
 * Write on PTY, chars will be read from controlling device
 */
ptswrite(dev)
   {
   register struct tty *tp;

   tp = &pty[dev.d_minor];      /* Setup pointer to PTY header */
   ttwrite(tp);
   }

/* -------------------------- P T C O P E N -------------------- */

ptcopen(dev,flag) /* Open for PTY Controller */
   {
   register struct tty *tp;
   extern int ptsstart();             /* Routine to start write from slave */

   if (dev.d_minor >= NPTY)    /* Verify minor device number */
      {
      u.u_error = ENXIO;       /* No such device */
      return;
      }
   tp = &pty[dev.d_minor];     /* Setup pointer to PTY header */
   if (tp->t_state & CARR_ON)  /* Already open? */
      {
      u.u_error = EIO;         /* I/O error (Is there something better?) */
      return;
      }
   tp->t_state =| SSTART;   /* Set "special start" flag */
   tp->t_addr = &ptsstart;     /* Set address of start routine */
   tp->t_state =| CARR_ON;     /* Turn on virtual carrier */
   tp->t_itpc = 0;             /* No controller side awaiters yet */
   if (tp->t_itp)
      awake(tp->t_itp, -1);    /* Wake up slave's awaiters */
   wakeup(&tp->t_addr);        /* Wake up slave's openers */
   }

/* -------------------------- P T C C L O S E ------------------- */

ptcclose(dev) /* Close controlling part of PTY */
   {
   register struct tty *tp;

   tp = &pty[dev.d_minor];       /* Setup pointer to PTY header */
   if (tp->t_state & ISOPEN)     /* Is it slave open? */
      signal(tp->t_pgrp,SIGHUP); /* Yes, send a HANGUP */
   tp->t_state =& ~CARR_ON;      /* Virtual carrier is gone */
   flushtty(tp);                 /* Clean things up */
   }

/* -------------------------- P T C R E A D ---------------------- */

ptcread(dev) /* Read from PTY's output buffer */
   {
   register struct tty *tp;
   register int temp;

   tp = &pty[dev.d_minor];              /* Setup pointer to PTY header */
   while (tp->t_outq.c_cc==0)           /* Wait for something to arrive */
      sleep(&tp->t_outq.c_cf,TTIPRI);   /* (Woken by ptsstart) */
   temp = tp->t_outq.c_cc;              /* Remember it */

   /* Copy in what's there, or all that will fit */
   while (tp->t_outq.c_cc && passc(getc(&tp->t_outq))>=0);

   /* Now, decide whether to wake up process waiting on input queue */
   if (tp->t_outq.c_cc==0 || (temp>TTLOWAT && tp->t_outq.c_cc<=TTLOWAT))
      {
      if (tp->t_itp)
         awake(tp->t_itp, -1);          /* Wake up slave side */
      wakeup(&tp->t_outq);
      }
   }

/* -------------------------- P T S S T A R T ------------------- */

ptsstart(tp) /* Called by 'ttstart' to output a character. */
/* Merely wakes up controlling half, which does actual work */
   {
   if ((tp->t_state & CARR_ON) == 0)   /* Nobody there! */
      flushtty(tp);                 /* To get rid of chars */
   if (tp->t_itpc)
      awake(tp->t_itpc, -1);        /* Wake up controller awaiters */
   wakeup(&tp->t_outq.c_cf);
   }

/* -------------------------- P T C W R I T E ------------------- */

ptcwrite(dev) /* Stuff characters into PTY's input buffer */
   {
   register struct tty *tp;
   register char c;

   tp = &pty[dev.d_minor];     /* Setup pointer to PTY header */
   /* For each character... */
   while ((c = cpass()) >= 0)
      ttyinput(c,tp);
   }

/* -------------------------- P T S S G T T Y ------------------- */
/*
 * Slave sgtty routine. Just call ttystty.
 */
ptssgtty(dev,v)
   int *v;
   {
   register struct tty *tp;

   tp = &pty[dev.d_minor];
   ttystty(tp, v);
   }

/* -------------------------- P T C S G T T Y ------------------- */
/*
 * Controller sgtty. Differs from slave only in that stty calls
 * do not do a wflushtty.
 */
ptcsgtty(dev, v)
   int *v;
   {
   register struct tty *tp;

   tp = &pty[dev.d_minor];     /* Setup pointer to PTY header */
   if (v)  /* gtty? */
      {
      ttystty(tp, v); /* Normal */
      }
   else {
      v = u.u_arg;
      tp->t_speeds = *v++;
      tp->t_erase = v->lobyte;
      tp->t_kill = v->hibyte;
      tp->t_flags = v[1];
      }
   }

/* =================== A W A I T   &   C A P A C I T Y ========== */


/* -------------------------- P T C C A P ----------------------- */

ptccap(ip, v)
   struct inode *ip;
   int *v;
   {
   register struct tty *tp;
   tp = &pty[ip->i_addr[0].d_minor];
   *v++ = tp->t_outq.c_cc;          /* Can read whatever's in output queue */
   *v = TTYHOG - tp->t_rawq.c_cc;   /* After this many, chars thrown away */
   *v =/ 2;   /* Allow for every user char being followed by \377 */
   }

/* -------------------------- P T S C A P ---------------------- */

ptscap(ip, v)
   struct inode *ip;
   int *v;
   {
   register struct tty *tp;

   tp = &pty[ip->i_addr[0].d_minor];
   ttycap(tp, v);
   }

/* -------------------------- P T S A W T --------------------- */

ptsawt(type, ip, fd)
   char type;
   struct inode *ip;
   int fd;
   {
   extern char *ablei();
   register struct tty *tp;

   tp = &pty[ip->i_addr[0].d_minor];
   tp->t_itp = ablei(type, 0, ip, fd);
   }

/* -------------------------- P T C A W T --------------------- */

ptcawt(type, ip, fd)
   char type;
   struct inode *ip;
   int fd;
   {
   extern char *ablei();
   register struct tty *tp;

   tp = &pty[ip->i_addr[0].d_minor];
   tp->t_itpc = ablei(type, 0, ip, fd);
   }
