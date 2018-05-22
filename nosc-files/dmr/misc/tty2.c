#

/*
 * general TTY subroutines
 */
#include "/usr/sys/h/param.h"
#include "/usr/sys/h/systm.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/tty1.h"
#include "/usr/sys/h/proc.h"
#include "/usr/sys/h/inode.h"
#include "/usr/sys/h/file.h"
#include "/usr/sys/h/reg.h"
#include "/usr/sys/h/conf.h"

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 */
char	maptab[];

/*
 * character mapping tables for ibm 2741
 */
char	uc[],lc[];
/*
 * The actual structure of a clist block manipulated by
 * getc and putc (mch.s)
 */
struct cblock {
	struct cblock *c_next;
	char info[6];
};

/* The character lists-- space for 6*NCLIST characters */
struct cblock cfree[NCLIST];
/* List head for unused character blocks. */
struct cblock *cfreelist;

/*
 * structure of device registers for KL, DL, and DC
 * interfaces-- more particularly, those for which the
 * SSTART bit is off and can be treated by general routines
 * (that is, not DH).
 */
struct {
	int ttrcsr;
	int ttrbuf;
	int tttcsr;
	int tttbuf;
};
/*
 * transfer raw input list to canonical list,
 * doing erase-kill processing and handling escapes.
 * It waits until a full line has been typed in cooked mode,
 * or until any character has been typed in raw mode.
 */
canon(atp)
struct tty *atp;
{
	register char *bp;
	char *bp1;
	register struct tty *tp;
	register int c;
	int cnt;

	tp = atp;
	spl5();
	while (tp->t_delct==0) {
		if ((tp->t_state&CARR_ON)==0)
			return(0);
		sleep(&tp->t_rawq, TTIPRI);
	}
	spl0();
loop:
	bp = &canonb[2];
	while ((c=getc(&tp->t_rawq)) >= 0) {
		if (c==0377) {
			tp->t_delct--;
			/* so may receive more than one char in raw mode */
			if (tp->t_flags&RAW)
				continue;
			break;
		}
		if (c==0376) {
			if (tp->t_speeds&LOOP) {
				tp->t_delct--;
				continue;
			}
		}
		/*
		 * handle escape and single char erase processing.
		 * 2741 has the erase character hard-coded to the
		 * default CERASE.  The backspace key on the 2741
		 * is always translated to CERASE which is defined
		 * in tty.h.  Normal devices have a programmable
		 * erase char in this version of unix.
		 */
		if (!(tp->t_flags&RAW)) {
			if (bp[-1]!='\\') {
#ifdef	IBM
				if ((tp->t_speeds&017)==IBM && c==CERASE) {
					if (bp> &canonb[2])
						bp--;
					continue;
				}
#endif	IBM
				if (c==tp->t_erase
#ifdef	IBM
				&& (tp->t_speeds&017) != IBM
#endif	IBM
				) {
					if (bp > &canonb[2])
						bp--;
					continue;
				}
				if (c==tp->t_kill)
					goto loop;
				if (c==CEOT)
					continue;
			} else 
			if (maptab[c] && (maptab[c]==c || (tp->t_flags&LCASE))) {
				if (bp[-2] != '\\') {
					c = maptab[c];
				}
				bp--;
			}
		}
		*bp++ = c;
		if (bp>=canonb+CANBSIZ)
			break;
	}
	bp1 = bp;
	bp = &canonb[2];
	cnt = bp1 - bp;			/* calc num chars processed */
	c = &tp->t_canq;
	while (bp<bp1) {
		putc(*bp++, c);
	}
	return(cnt);
}

/*
 * Place a character on raw TTY input queue, putting in delimiters
 * and waking up top half as needed.
 * Also echo if required.
 * The arguments are the character and the appropriate
 * tty structure.
 */
ttyinput(ac, atp)
struct tty *atp;
{
	register int t_flags, c;
	register struct tty *tp;
	int conv;

	tp = atp;
	c = ac & 0177;
	/*
	 * Input processing for 2741's is done here
	 * separately from normal devices.  Thus only one compare
	 * is done to get here, and normal processing is not affected.
	 * The following implements the 2741 synchronization strategy
	 * with the output side and the necessary
	 * character translations.
	 */
#ifdef	IBM
	if ((tp->t_speeds&017) == IBM) {
		/*
		 * the only way (c==0177) is if a break
		 * is detected by the driver.  To be consistent with
		 * standard unix this is translated to a zero for getty
		 * so that speed-shifting on login can be done.
		 * it must be done this way because zero is a
		 * legal 2741 character.
		 */
		if (c==0177) {
		   if (tp->t_flags&RAW) {
			if (!putc(0,&tp->t_rawq)) {
				tp->t_delct++;
			}
			wakeup(&tp->t_rawq);
			return;
		   }
		}
		c =& 077;		/* mask to 6-bits in case of 0177 */
		/*
		 * record upper/lower case changes
		 */
		if (c==LC) {
			tp->ibm =& ~0200;
			tp->ibm =| 0100;
			return;
		}
		if (c==UC) {
			tp->ibm =& ~0100;
			tp->ibm =| 0200;
			return;
		}
		/*
		 * CRD char is not unique.  It is treated as such only if
		 * expected; i.e. when CHANGE bit has been set by the
		 * output side.
		 */
		if (c==CRD) {
			if (tp->ibm & CHANGE) {
				tp->ibm =& ~  (CHANGE+LOCK+EOL);
				tp->ibm =| ULOCK;
				wakeup(&tp->ibm);
				return;
			}
		}
		/*
		 * CRC is unique indicating end of tty message and
		 * transition to locked keyboard.
		 */
		if (c==CRC) {
			if (tp->ibm&WANT) {
				tp->ibm =| LOCK;
				wakeup(&tp->ibm);
				return;
			}
			if (tp->ibm&CHANGE) {
				printf(" urk ");
			}
			tp->ibm=| CHANGE;
			putc(CRC,&tp->t_outq);
			putc(0214,&tp->t_outq);
			ttstart(tp);
			/*
			 * if EOL not set, we have a null
			 * input line.  this can be got only
			 * by pressing the ATTN key.  so c gets
			 * converted to 077 which translates
			 * to 0177 (rubout) below
			 */
			if (!(tp->ibm&EOL)) {
				c = 077;
			}
			else return;
		}
		/*
		 * translate 6-bit chars to ascii
		 * first pick a char out of translate table.
		 * then do escape processing.  CMAP bit is high
		 * if previous char was the escape char.
		 */
		if ((tp->ibm&CASE)&0100)
			c = lc[c];
		else	c = uc[c];
		if (tp->ibm&CMAP) {
			tp->ibm =& ~  CMAP;
			switch(c) {
			   case 'D': c=CEOT;	break;
			   case 'E': c= 033;	break;	/* ESC for tenex */
			   case 'Q': c=CQUIT;	break;
			   case 'L': c= '<';	break;
			   case 'G': c= '>';	break;
			   case '!': c= '|';	break;
			   case '(': c= '{';	break;
			   case ')': c= '}';	break;
			   case 047: c= '`';	break;
			   case 'T': c= '~';	break;
			   case 'U': c= '^';	break;
			   case 'r':
			   case 't':
			   case 'n':
			   case '\\': putc('\\',&tp->t_rawq);
						break;
			   default:
				/* 
				 * convert alphabetics not trapped
				 * by the case statement to 
				 * ASCII control characters.
				 */
				conv = 0;
				if (c>='A' && c<='Z') {
					conv = 0100;
				}
				if (c>='a' && c<='z') {
					conv = 0140;
				}
				if (conv) c=- conv;
				else putc('\\',&tp->t_rawq);
			}
		} else if (c == '\\') {
			tp->ibm =| CMAP;
			return;
		}
		/*
		 * notice end-of-line in order to detect
		 * null lines from ibm terminal
		 */
		if (c=='\n') tp->ibm =| EOL;
		else tp->ibm =& ~ EOL;
	}
#endif	IBM
	t_flags = tp->t_flags;
	/*
	 * block output side if user hasn't finished typing
	 * an input line as long as we're not in raw mode.
	 */
	if (!(t_flags&RAW) && c!= '\n') {
		tp->t_speeds =| BLOCK;
		/*
		 * CSTOP character toggles output blocking.
		 * CDUMP is used to throw characters away.
		 * this is used to (1) remove CSTOP chars from
		 * the input stream, (2) to throw away newline-
		 * after-line delete for 2741's.
		 */
		if (c==CSTOP) {
			if (tp->t_speeds&CDUMP) c = '\n';
			else {
				tp->t_speeds =| CDUMP;
				return;
			}
		} else tp->t_speeds =& ~  CDUMP;
	}
	if (( c =& 0177 ) == '\r' && t_flags&CRMOD )
		c = '\n';
	/*
	 * Handle process-interrupting characters,
	 * re-enable output,
	 * turn line around for 2741's.
	 */
	if (!(t_flags&RAW) && (c==CQUIT || c==CINTR)) {
		signal(tp->t_pgrp, c==CINTR? SIGINT:SIGQIT);
		flushtty(tp);
		tp->t_speeds=& ~ (BLOCK+CDUMP);
		wakeup(&tp->t_speeds);
#ifdef	IBM
		if ((tp->t_speeds&017)==IBM) {
			tp->ibm=LOCK;
			ttstop(tp);
		}
#endif	IBM
		return;
	}
	if (tp->t_rawq.c_cc >= TTYHOG) {
		flushtty(tp);
		return;
	}
	if (t_flags&LCASE && c>='A' && c<='Z')
		c =+ 'a'-'A';
	if (c!='\n' || !(tp->t_speeds&CDUMP))
		putc(c, &tp->t_rawq);
	if (t_flags&RAW || c=='\n' || c==004) {
		wakeup(&tp->t_speeds);
		tp->t_speeds=& ~BLOCK;
		if (c=='\n' && tp->t_speeds&CDUMP) {
			tp->t_speeds=& ~ CDUMP;
			return;
		}
		wakeup(&tp->t_rawq);
		if (putc(0377, &tp->t_rawq)==0)
			tp->t_delct++;
	}
	/*
	 * Some unix software twiddles the ECHO bit.
	 * This must be ignored on 2741's.
	 */
	if (t_flags & ECHO
#ifdef	IBM
	&& !((tp->t_speeds&017)==IBM)
#endif	IBM
	) {
		ttyoutput(c,tp);
		ttstart(tp);
	}
	/*
	 * Line kill processing done here.
	 * echo newline and enable output
	 * (except for 2741's)
	 */
	if (c==tp->t_kill && !(t_flags&RAW)) {
#ifdef	IBM
		if ((tp->t_speeds&017)==IBM) {
			tp->t_speeds=| CDUMP;
			return;
		}
#endif	IBM
		ttyoutput('\n',tp);
		tp->t_speeds =& ~  (BLOCK+CDUMP);
		ttstart(tp);
		wakeup(&tp->t_speeds);
	}
}
