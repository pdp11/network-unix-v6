#
/* Revision - New handshake handler */

/*
 * A clist structure is the head
 * of a linked list queue of characters.
 * The characters are stored in 4-word
 * blocks containing a link and 6 characters.
 * The routines getc and putc (m45.s or m40.s)
 * manipulate these structures.
 */
struct clist
{
	int	c_cc;		/* character count */
	int	c_cf;		/* pointer to first block */
	int	c_cl;		/* pointer to last block */
};

/*
 * A tty structure is needed for
 * each UNIX character device that
 * is used for normal terminal IO.
 * The routines in tty.c handle the
 * common code associated with
 * these structures.
 * The definition and device dependent
 * code is in each driver. (kl.c dc.c dh.c)
 */
struct tty
{
	/* These variables are (hopefully) untouchable by the ttymode system
	 * call. The total number of un-hackable bytes should be specified in
	 * the compiler control line for FIXOFS, below.
	 */

	struct	clist t_canq;	/* buffer for ttyread */
	struct	clist t_outq;	/* output list to device */

	int	*t_addr;		/* device address (register or startup fcn) */
	int 	t_dev;			/* device name */
	int	t_llf;			/* ptr to last linefeed in t_canq */
	int	t_state;		/* internal state, not visible externally */
	char	t_char;			/* character temporary */
	char	t_acnt;			/* count of characters sent */
	int	t_pgrp;			/* process group name */
#ifdef	DNTTY
	int	t_dnflgs;	/* DN: TTY flags specific to DN hacks */
	int	t_ioct;		/* DN: I/O cnt for catching idle lines */
#endif

#define TFIXOFS 28		/* Fixed offset from base of tty structure of
				 * first user-hackable parameter, for ttymod */

	/* User hackable info. Any additional parameters to the rear, please. */

	/*	name		       byte #	      Description */

	int	t_speeds;	/*	0,1	input (low) + output (high) speeds */
	char	t_erase;	/*	2	erase char */
	char	t_kill;		/*	3	kill char */
	int	t_flags;	/*	4,5	mode. Bytes 0-5 settable by stty call */
	int 	t_flag2;	/*	6,7	additional flags */
	char	t_flag4;	/*	8	additional bits */
	char	t_cano;		/*	9	canonical terminal type */
	char	t_clin;		/*	10	current line of device */
	char	t_col;		/*	11	printing column of device */
	char 	t_llen;		/*	12	line length */
	char	t_psize;	/*	13	page size */
	char	t_quit;		/*	14	quit char (^B) */
	char	t_intr;		/*	15	interrupt char (^Q) */
	char	t_EOT;		/*	16	end_of_file char (^D) */
	char	t_cntla;	/*	17	special interrupt char (^A) */
	char	t_atime;	/*	18	timeout in 60ths of seconds */
	char	t_asize;	/*	19	number of character per ACK */
	char	t_enq;		/*	20	ENQ character */
	char	t_ack;		/*	21	ACK character */
	char	t_esc;		/*	22	Escape character */
	char	t_alwat;	/*	23	Low water mark for ACK */
	char	t_delpos;	/*	24	Position of first typed char */
	char	t_freeze;	/*	25	freeze character (^F) */
	char	t_thaw;		/*	26	thaw character (^T) */
	char	t_hold;		/*	27	hold character (^S) */
	char	t_release;	/*	28	release character (^Q) */
	char	t_retype;	/*	29	retype line (^R) */
	char	t_silent;	/*	30	silence output (^S) */
	char	t_clear;	/*	31	clear screen (^L) */

#define TMAXBN	31		/* Maximum allowable byte number for ttymod */
};

char partab[];			/* ASCII table: parity, character class */

#define	TTIPRI	10
#define	TTOPRI	20
#define	CERASE	0177		/* default special characters */
#define	CEOT	032		/* ^Z */
#define	CKILL	('U' & 037)
#define	CQUIT	('B' & 037)		/* FS, cntl shift L */
#define	CINTR	('C' & 037)		/* ^C */
#define CSPEC	('A' & 037)		/* Special interrupt, ^A */
#define CACK	('F' & 037)
#define CENQ	('D' & 037)
#define CESC	('_' & 037)
#define CFREEZE	('S' & 037)
#define CTHAW	('Q' & 037)
#define CHOLD	('S' & 037)
#define CREL	('Q' & 037)
#define CRETYPE	('R' & 037)
#define CCLEAR	('L' & 037)
#define CSILENT	('O' & 037)
#define CESC1	'1'
#define CESC2	'2'

/* limits */
#define	TTHIWAT	50
#define	TTLOWAT	30
#define	TTYHOG	256
#define TTACK	50

/* modes */
#define	HUPCL	01
#define	XTABS	02
#define	LCASE	04
#define	ECHO	010
#define	CRMOD	020
#define	RAW	040
#define	ODDP	0100
#define	EVENP	0200
#define	NLDELAY	001400
#define	TBDELAY	006000
#define	CRDELAY	030000
#define	VTDELAY	040000
#define DUMC	0100000	/* KLH: Don't Use Modem Control. */

/* Speeds (mostly for DH11) */
#define B0	00	/* 0 baud, for turning lines off */
#define B50	01
#define B75	02
#define B110	03	/* 110 baud */
#define B134	04	/* 134.5 baud */
#define B150	05
#define B200	06
#define B300	07
#define B600	010
#define B1200	011
#define B1800	012
#define B2400	013
#define B4800	014
#define B9600	015
#define BEXTA	016
#define BEXTB	017

/* Hardware bits */
#define	DONE	0200
#define	IENABLE	0100

/* Internal state bits */
#define	TIMEOUT	01		/* Delay timeout in progress */
#define	WOPEN	02		/* Waiting for open to complete */
#define	ISOPEN	04		/* Device is open */
#define	SSTART	010		/* Has special start routine at addr */
#define	CARR_ON	020		/* Software copy of carrier-present */
#define	BUSY	040		/* Output in progress */
#define	ASLEEP	0100		/* Wakeup when output done */
#define ERMODE	0200		/* Erase mode, deleting chars */
#define ACKWAIT	0400		/* timeout is pending */
#define ESCRCV	01000		/* set when escape has been recieved */
#define ENQRCV	02000		/* ENQ recieved by ttyinput */
#define	FROZEN	04000		/* tty output is frozen (by ^F)	*/
#define HELD	010000		/* tty input is held */

#ifdef DNTTY
/* DN-specific TTY flags, in t_dnflgs */
#define	BAUDOT	01		/* When set, TTY is baudot tty. */
#define	FIGS	02		/* Set during FIGS mode, off during LETS */
#define	CTS	04		/* Copy of DM hardware CTS */
#define TMORTS	010		/* Set during RTS-drop timeout */
#define TMOCTS	020		/* Set during CTS-on timeout wait */
#define LICCR	040		/* Last input char was CR */
#define LICLF	0100		/* Last input char was LF */
#endif

/* ttymode parameters */

#define TFLUSH	0020000		/* Request to flush buffer first */
#define TCNTL	0140000		/* Control bits in a ttycommand  */
#define TBYTNUM 0017400		/* Byte number           "       */
#define TBITS	0000377		/* Argument bits         "       */
#define	TCLEAR	0040000		/* Clear control command in a ttycommand */
#define	TSET	0100000		/* Set		   "			 */
#define	TREPLAC	0140000		/* Replace	   "			 */
#define	TGET	0		/* Get		   "			 */

/* t_tflag2 flags */

#define	SILENT	01		/* Silences tty */
#define	LITIN	02		/* 8 bit input */
#define	LITOUT	04		/* 8 bit output */
#define	MORE	010		/* Enables "MORE" processing */
#define EEI	020		/* Explicitly Enable Interrupts
				 * (allows ^Q,^B,^A in raw mode */
#define RACK	040
#define WACK	0100
#define SECRX	0200		/* don't transmit if SECRX is 0 */
#define WHOLD	0400		/* transmit t_hold and t_release */
				/* when the input queue grows/shrinks */

/* Canonical terminal types */

#  define TC_PRINTING   0
#  define TC_VT52       1
#  define TC_ARDS	2
#  define TC_SARDS	3
#  define TC_GSI        4       /* courrier wheel */
#  define TC_GSIE       5       /* elite wheel */
#  define TC_GSIP       6       /* pica wheel */
#  define TC_GSIQ	7
#  define TC_GSIQE	8
#  define TC_GSIQP	9
#  define TC_GT40       10      /* assumed to be running the VT52 scroller */
#  define TC_TERM       11      /* the chris termanal */
#  define TC_CRT        12      /* cathode-ray decwriter */
#  define TC_AJ		13	/* Anderson-Jacobson 832 (Qume)		*/
#  define TC_AJE	14
