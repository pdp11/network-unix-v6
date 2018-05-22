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
	struct	clist t_rawq;	/*+00 input chars right off device */
	struct	clist t_canq;	/*+06 input chars after erase and kill */
	struct	clist t_outq;	/*+12 output list to device */
	int	t_flags;	/*+18 mode, settable by stty call */
	int	*t_addr;	/*+20 dev address (register or startup fcn) */
	char	t_delct;	/*+22 number of delimiters in raw q */
	char	t_col;		/*+23 printing column of device */
	char	t_erase;	/*+24 erase character */
	char	t_kill;		/*+25 kill character */
	char	t_state;	/*+26 internal state, not visible externally */
	char	t_char;		/*+27 character temporary */
	int	t_speeds;	/*+28 output+input line speed */
	int	t_pgrp;		/*+30 process group name */
};

char partab[];			/* ASCII table: parity, character class */

#define	TTIPRI	10
#define	TTOPRI	20

#define	CERASE	'\b'		/* default special characters */
#define	CEOT	004
#define CKILL	'@'
#define	CQUIT	034		/* FS, cntl shift L */
#define	CINTR	0177		/* DEL */

/* limits */
#define	TTHIWAT	150	/* max chars process can type before sleep */
#define	TTLOWAT	50	/* wakeup point */
#define	TTYHOG	100	/* max user can type ahead */

/* modes */
#define	HUPCL	0000001
#define	XTABS	0000002
#define	LCASE	0000004
#define	ECHO	0000010
#define	CRMOD	0000020
#define	RAW	0000040
#define	ODDP	0000100
#define	EVENP	0000200
#define	NLDELAY	0001400
#define	TBDELAY	0006000
#define	CRDELAY	0030000
#define	VTDELAY	0040000

/* Hardware bits */
#define	DONE	0200
#define	IENABLE	0100

/* Internal state bits */
#define	TIMEOUT	0001		/* Delay timeout in progress */
#define	WOPEN	0002		/* Waiting for open to complete */
#define	ISOPEN	0004		/* Device is open */
#define	SSTART	0010		/* Has special start routine at addr */
#define	CARR_ON	0020		/* Software copy of carrier-present */
#define	BUSY	0040		/* Output in progress */
#define	ASLEEP	0100		/* Wakeup when output done */
#define NOTIFY	0200		/* Notify user process when input complete */
