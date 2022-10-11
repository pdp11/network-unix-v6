/*
 * basic types
 */
typedef	unsigned short	map_index;	/* indices */
typedef	unsigned short	map_count;	/* counts */
typedef	unsigned short	map_cap;	/* capability */
/*
 * the map header simply contains counts of the number
 * of entries in each array that follows
 */
typedef	struct	{
	map_count	map_nnet;	/* # network entries */
	map_count	map_nhost;	/* # host entries */
	map_count	map_nchar;	/* # name characters */
	map_count	map_ndial;	/* # dial digits */
} map_head;
/*
 * one entry for each network
 */
typedef	struct	{
	map_index	net_name;	/* index to name(s) */
	map_cap		net_cap;	/* network capabilities */
	map_index	net_next;	/* search chain */
	map_index	net_host;	/* index to host(s) */
	map_count	net_nhost;	/* # hosts */
} net_ent;

/*
 * one entry for each host
 *	sorted by network address (net,imp,hoi)
 */
typedef	struct	{
	map_index	host_name;	/* index to name(s) */
	netaddr		host_addr;	/* network address */
	map_cap		host_cap;	/* host capabilities */
} host_ent;
/*
 * name and dial digit buffers follow
 * these are character arrays
 * multiple names are kept in lists terminated by an empty string
 */
#ifdef	LIBN
/*
 * base pointers for in-core copy of map
 */
extern	map_head	*_netmap;	/* -> network map header */
extern	net_ent		*_nettab;	/* -> network table */
extern	host_ent	*_hosttab;	/* -> host table */
extern	char		*_namebuf;	/* -> name buffer */
extern	char		*_dialbuf;	/* -> digit buffer */
/*
 * local host table
 */
extern	netaddr		_lhosttab[];	/* local host addresses */
#define	_LHOSTSIZ	16		/* max number */
extern	unsigned	_lhostcnt;	/* actual number */
/*
 * dial digit extension buffer
 */
extern	char		_dialext[];	/* dial digit buffer extension */
#define	_DIALSIZ	(4*(NETNAMSIZ+1))	/* size of buffer */
extern	unsigned	_dialcnt;	/* # digits in buffer */
/*
 * internal access functions
 */
extern	net_ent		*_nnetp();	/* network number to net pointer */
extern	host_ent	*_shostp();	/* name to host ent pointer */
extern	host_ent	*_ahostp();	/* network address to host pointer */
#endif	LIBN
