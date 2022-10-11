struct sgttybuf
{
	char s_ispeed, s_ospeed;
	char s_erase, s_kill;
	int s_flags;
};

/* Input and output speeds - DH11. Other interfaces may not support them all. */

#define BHANGUP	0
#define B50	1
#define B75	2
#define	B110	3
#define B134	4
#define	B150	5
#define B200	6
#define	B300	7
#define B600	8
#define B1200	9
#define B1800	10
#define B2400   11
#define B4800	12
#define	B9600	13
#define BEXTA	14
#define BEXTB	15

/* Special characters */

#define	CERASE	''
#define	CEOT	004
#define	CKILL	'@'
#define	CQUIT	034		/* FS, cntl \ */
#define	CINTR	0177		/* DEL */

/* Flag definitions */

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
