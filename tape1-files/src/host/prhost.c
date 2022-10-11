#include "stdio.h"
#include "sys/types.h"
#include "sys/stat.h"
#define	LIBN
#include "netlib.h"
#include "netmap.h"

char		*ifile;		/* input filename */
int		hostcnt;	/* # hosts */
host_ent	**hosttab;	/* array used for host sorting */
int		hostcmp();	/* host entry comparison function */
int		acomp();	/* host address comparison */
				/* sort by (net,name,addr) is default */
int		aflag;		/* sort by (net,addr,name) */
int		nflag;		/* sort by (name,net,addr) */

extern	char	*progname;	/* program name for cmderr */
extern	char	*_mapname;	/* network map filename override */

main(ac, av)
char **av;
{
	register char *ap;

	progname = *av++;
	while (ap = *av++) {
		if (seq(ap, "-i")) {
			if (!(ap = *av++))
				ecmderr(0, "missing argument for -i\n");
			if (ifile)
				ecmderr(0, "duplicate argument for -i\n");
			ifile = ap;
			continue;
		}
		if (seq(ap, "-n")) {
			nflag = 1;
			continue;
		}
		if (seq(ap, "-a")) {
			aflag = 1;
			continue;
		}
		cmderr(0, "argument %s ignored\n", ap);
	}
	if (ifile)
		_mapname = ifile;
	if (!loadnetmap())
		ecmderr(-1, "Can't load %s\n", _mapname);
	sorter();
	printer();
	exit(0);
}

/*
 * allocate an array of host entry pointers
 *	fill it with pointers to actual host entries
 *	then sort by net,hostname
 */
sorter()
{
	register int i;
	extern char *malloc();
	/*
	 * grab space for pointer array and initialize it
	 */
	hostcnt = _netmap->map_nhost;
	hosttab = (host_ent **)malloc(hostcnt * sizeof *hosttab);
	if (hosttab == NULL)
		ecmderr(-1, "can't allocate space for host pointers\n");
	for (i = 0; i < hostcnt; ++i)
		hosttab[i] = &_hosttab[i];
	/*
	 * now sort the pointers
	 */
	qsort(&hosttab[0], hostcnt, sizeof(host_ent *), hostcmp);
}

/*
 * host entry comparison
 */
hostcmp(a, b)
host_ent **a, **b;
{
	register int cc;
	register host_ent *ap, *bp;

	ap = *a;
	bp = *b;
	if (nflag) {
		cc = namecmp(ap, bp);
		if (cc == 0)
			cc = netcmp(ap, bp);
		if (cc == 0)
			cc = addrcmp(ap, bp);
	}
	else if (aflag) {
		cc = netcmp(ap, bp);
		if (cc == 0)
			cc = addrcmp(ap, bp);
		if (cc == 0)
			cc = namecmp(ap, bp);
	}
	else {
		cc = netcmp(ap, bp);
		if (cc == 0)
			cc = namecmp(ap, bp);
		if (cc == 0)
			cc = addrcmp(ap, bp);
	}
	return cc;
}

/*
 * network number comparison
 */
netcmp(ap, bp)
register host_ent *ap, *bp;
{
	return net_part(ap->host_addr) - net_part(bp->host_addr);
}

/*
 * host address comparison
 */
addrcmp(ap, bp)
register host_ent *ap, *bp;
{
	register int cc;

	cc = imp_part(ap->host_addr) - imp_part(bp->host_addr);
	if (cc == 0)
		cc = hoi_part(ap->host_addr) - hoi_part(bp->host_addr);
	return cc;
}

/*
 * host name comparison
 */
namecmp(ap, bp)
register host_ent *ap, *bp;
{
	return strcmp(&_namebuf[ap->host_name], &_namebuf[bp->host_name]);
}

printer()
{
	register int i;

	/*
	 * print general information
	 */
	printf("\n; regenerated from %s\n\n", _mapname);
	/*
	 * print network information
	 */
	printf("\n; %u networks ordered by number\n\n", _netmap->map_nnet);
	for (i = 0; i < _netmap->map_nnet; ++i)
		prnet((netnumb)i, &_nettab[i]);
	/*
	 * print host information
	 */
	printf("\n; %u hosts ordered by %s\n\n", hostcnt,
		aflag?	"net,address,name" :
		nflag?	"name,net,address" :
			"net,name,address");
	for (i = 0; i < hostcnt; ++i)
		prhost(hosttab[i]->host_addr);
}

/*
 * print entry for a single network
 */
prnet(net, np)
netnumb	net;
net_ent *np;
{
	int	i;
	char	*p;
	unsigned cap;

	printf("net %-*s %d", NETNAMSIZ, netname(net), (int)net);
	if (cap = netcap(net))
		printf(",0%o", cap);
	for (i = 1; p = netsyn(net, i); ++i)
		printf(" %s", p);
	/*
	 * print known hosts (if non-zero)
	 */
	if (np->net_nhost > 0)
		printf(" ; %d known hosts", np->net_nhost);
	printf("\n");
}

/*
 * print host information
 */
prhost(addr)
netaddr	addr;
{
	int	i;
	char	*p;
	unsigned cap;
	/*
	 * print name and address
	 */
	printf("%-*s %s", NETNAMSIZ, hostname(addr), hostfmt(addr, 2));
	if (cap = hostcap(addr))
		printf(",0%o", cap);
	/*
	 * print aliases
	 */
	for (i = 1; p = hostsyn(addr, i); ++i)
		printf(" %s", p);
	printf("\n");
}
