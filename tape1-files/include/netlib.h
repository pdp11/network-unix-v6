/*
 * network number type
 */
typedef	long		netnumb;
/*
 * all purpose net address
 */
typedef	union	{
	long		_na_l;		/* the whole thing */
	char		_na_b[sizeof(long)];	/* the pieces */
} netaddr;

/*
 * these values refer to network data, not machine representation
 * they should stay the same regardless of the machine type
 */
#define	_NABM	0377		/* network byte mask */
#define	_NABW	8		/* network byte width */

/*
 * macros for host address access
 *	values are constructed from individual address bytes
 *	for machine independence
 */
#define	net_part(a)	((a)._na_b[0]&_NABM)
#define imp_part(a)	(((a)._na_b[3]&_NABM)|(((a)._na_b[2]&_NABM)<<_NABW))
#define	logh_part(a)	((a)._na_b[2]&_NABM)
#define	hoi_part(a)	((a)._na_b[1]&_NABM)

#define	net_set(a,n)	((a)._na_b[0]=(n)&_NABM)
#define	imp_set(a,i)	(((a)._na_b[3]=(i)&_NABM),((a)._na_b[2]=((i)>>_NABW)&_NABM))
#define	hoi_set(a,h)	((a)._na_b[1]=(h)&_NABM)
#define	logh_set(a,h)	((a)._na_b[2]=(h)&_NABM)
#define	host_set(a,n)	(imp_set(a,n),hoi_set(a,(n)>>(2*_NABW)))

/*
 * special net number macros
 */
#define	ANYNET	0L
#define	BADNET	(-1L)
#define	isbadnet(n)	((n)==BADNET)
#define	isanynet(n)	((n)==ANYNET)
/*
 * special network address macros
 */
#define	mknetaddr(a,n,i,h)	(net_set(a,n),imp_set(a,i),hoi_set(a,h))
#define	mkbadhost(a)	((a)._na_l=(-1L))
#define	mkanyhost(a)	((a)._na_l=0)
#define	mkoldhost(a,n)	((a)._na_l=(n))
#define	isbadhost(a)	((a)._na_l==(-1L))
#define	isanyhost(a)	((a)._na_l==0)
#define	iseqhost(a,b)	((a)._na_l==(b)._na_l)

#define	NETNAMSIZ	24
#define	NONETNAME	"nonet"
#define	NOHOSTNAME	"nohost"
/*
 * network capability parts
 */
#define	NETA_TYPE(c)	((c)&07)
#define		NETA_REG	0	/* regular format */
#define		NETA_LONG	1	/* long octal format */
#define		NETA_DIAL	2	/* dial digits */

/*
 * function definitions
 */
extern	netnumb		getnet();	/* net name to number */
extern	netnumb		thisnet();	/* local network number */
extern	netaddr		gethost();	/* host name to address */
extern	netaddr		getnhost();	/* host name, net to address */
extern	netaddr		thishost();	/* local host address */
extern	char		*netname();	/* network number to name */
extern	char		*netsyn();	/* network number to alias */
extern	char		*netfmt();	/* network number to ascii */
extern	unsigned	netcap();	/* network number to capability */
extern	char		*hostname();	/* network address to host name */
extern	char		*hostsyn();	/* network address to alias */
extern	unsigned	hostcap();	/* network address to capability */
extern	char		*hostfmt();	/* host address to ascii format */
extern	char		*dialnum();	/* network address to dial digits */
extern	char		*thisname();	/* local host name */
extern	int		hashost();	/* network address hash */
extern	long		lhostnum();	/* network address to NCP host */

#ifdef	LIBN
/*
 * internal library functions
 *	not for public consumption
 */
extern	netnumb		_nnetn();	/* numeric net to number */
extern	netnumb		_snetn();	/* symbolic net to number */
extern	netaddr		_nhosta();	/* numeric host to address */
extern	netaddr		_shosta();	/* symbolic host to address */
extern	char		*_1parse();	/* parse host or net name */
extern	char		*_2parse();	/* parse host,net name */
extern	char		*_atouiv();	/* ascii to unsigned int */
extern	char		*_atoulv();	/* ascii to unsigned long */
extern	unsigned	_dialstr();	/* dial digits to index */
#endif	LIBN
