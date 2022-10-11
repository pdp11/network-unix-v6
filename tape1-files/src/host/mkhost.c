/*
 * mkhost [-sri] [-i ifile] [-o ofile]
 *
 *	build binary network map
 */
#include "stdio.h"
#include "ctype.h"
#include "netlib.h"
#include "netmap.h"
#include "netfiles.h"
/*
 * table space parameters
 */
#define	NARGS	100		/* max arguments/line */
#define	NACHR	1000		/* max argument chars */
#define	NNETS	200		/* max networks */
#define	NHOSTS	500		/* max hosts */
#define	NCHARS	8000		/* space for names */
#define	NDIALS	400		/* space for dial digits */

#define	EOS	'\0'		/* end-of-string */

char		*progname;	/* program name for cmderr */
char		*ifile;		/* input file */
int		srifmt;		/* input file format flag */
char		*ofile;		/* output file */
/*
 * official map names
 */
char		curmap[] = MAP_BIN;
char		oldmap[] = MAP_OBIN;
char		newmap[] = MAP_NBIN;
/*
 * input variables
 */
unsigned	lineno;		/* for diagnostics */
int		nerror;		/* input error count */
/*
 * argument parsing space
 */
char	**atop;			/* top of arg list */
char	*ctop;			/* top of char space */
char	*abuf[NARGS];		/* argument list */
char	cbuf[NACHR];		/* space for arguments */
typedef	struct	{		/* arglist structure */
	int	narg;		/* count */
	char	**argv;		/* -> arg list */
} arglist;
/*
 * parsing functions
 */
arglist	parse1();	/* white space parser */
arglist	parse2();	/* comma list parser */
/*
 * table space for building map
 */
int		net_cnt;		/* # networks */
net_ent		net_tab[NNETS];		/* networks */
int		host_cnt;		/* # hosts */
host_ent	host_tab[NHOSTS];	/* hosts */
int		name_cnt;		/* # names */
int		char_cnt;		/* # chars */
char		char_buf[NCHARS];	/* name buffer */
int		dial_cnt;		/* # dial digits */
char		dial_buf[NDIALS];	/* dial digit buffer */

#define	nameof(x)	(&char_buf[x])

/*
 * miscellaneous functions
 */
netaddr		_nhosta();	/* numeric network address (from library) */
char		*_atouiv();	/* ascii (dec or oct) to unsigned */
char		*_atoulv();	/* ascii (dec or oct) to unsigned long */

typedef	unsigned int	uint;
typedef	long	ulong;

main(ac, av)
char **av;
{
	char	*ap;
	FILE	*fp;

	/*
	 * get progname for cmderr
	 * process option arguments
	 */
	progname = *av++;
	while (ap = *av++) {
		if (seq(ap, "-i")) {
			/*
			 * input file override
			 */
			if (!(ap = *av++))
				ecmderr(0, "missing argument for -i\n");
			if (ifile)
				ecmderr(0, "duplicate argument for -i\n");
			ifile = ap;
			continue;
		}
		if (seq(ap, "-ai") || seq(ap, "-sri")) {
			/*
			 * sri input file format
			 */
			srifmt = 1;
			continue;
		}
		if (seq(ap, "-o")) {
			/*
			 * output file override
			 */
			if (!(ap = *av++))
				ecmderr(0, "missing argument for -o\n");
			if (ofile)
				ecmderr(0, "duplicate argument for -o\n");
			ofile = ap;
			continue;
		}
		cmderr(0, "argument %s ignored\n", ap);
	}
	/* read ascii input */
	readit();
	/* sort host table and fill in network table */
	sorter();
	/* make network search list */
	chainit();
	/* generate binary map file */
	genmap();
	/* if doing official version, update */
	if (ofile == newmap)
		chgmap();
	exit(0);
}

readit()
{
	FILE	*fp;
	/*
	 * determine input and read it
	 */
	if (!ifile)
		ifile = srifmt? MAP_SRI : MAP_BBN;
	printf("input file = %s\n", ifile);
	if ((fp = fopen(ifile, "r")) == NULL)
		ecmderr(-1, "cannot open %s\n", ifile);
	while (reader(fp))
		nerror += srifmt? parsesri() : parsebbn();
	fclose(fp);
	/*
	 * print statistics
	 */
	printf("%d nets, %d hosts, %d names, %d name chars, %d dial digits\n",
		net_cnt, host_cnt, name_cnt, char_cnt, dial_cnt);
	if (nerror)
		ecmderr(0, "%d errors reading input\n", nerror);
}

/*
 * read a line of input
 *	increment lineno
 *	reset arg list pointers
 *	truncate after semicolon
 *	convert to lower case
 */
reader(fp)
register FILE *fp;
{
	register int c, eol;

	atop = abuf;
	ctop = cbuf;
	++lineno;
	eol = 0;
	while ((c = getc(fp)) != EOF) {
		if (c == '\n') {
			addchr(EOS);
			return 1;
		}
		if (eol)
			continue;
		if (c == ';') {
			eol = 1;
			continue;
		}
		addchr(isupper(c)? tolower(c) : c);
	}
	return 0;
}

/*
 * parse BBN style host map
 *
 * input format:
 *
 *	NET	name number [synonym ...]
 *
 *	[HOST]	name address,network[,capability] [synonym ...]
 */
parsebbn()
{
	arglist	a1, a2;
	netnumb	net;		/* network number */
	netaddr	hosta;		/* host address */
	uint	cap = 0;	/* capability word */

	a1 = parse1(cbuf);
	if (a1.narg == 0)
		return 0;
	if (seq(a1.argv[0], "net")) {
		if (a1.narg < 3) {
			cmderr(0, "line %u: bad number of arguments\n", lineno);
			return 1;
		}
		a2 = parse2(a1.argv[2]);
		if (a2.narg < 1 || a2.narg > 2) {
			cmderr(0, "line %u: bad network number %s\n", lineno,
				a1.argv[2]);
			return 1;
		}
		if (*_atoulv(a2.argv[0], &net) != EOS) {
			cmderr(0, "line %u: bad net number %s\n",
				lineno, a1.argv[2]);
			return 1;
		}
		if (a2.narg == 2 && *_atouiv(a2.argv[1], &cap) != EOS) {
			cmderr(0, "line %u: bad net capability %s\n",
				lineno, a2.argv[1]);
			return 1;
		}
		return addnet(a1.argv[1], &a1.argv[3], net, cap);
	}
	if (seq(a1.argv[0], "host"))
		--a1.narg, ++a1.argv;
	if (a1.narg < 2) {
		cmderr(0, "line %u: bad host arguments\n", lineno);
		return 1;
	}
	a2 = parse2(a1.argv[1]);
	if (a2.narg < 2 || a2.narg > 3) {
		cmderr(0, "line %u: bad host address\n", lineno, a1.argv[1]);
		return 1;
	}
	if (!makeaddr(a2.argv[0], a2.argv[1], &hosta))
		return 1;
	if (a2.narg == 3 && *_atouiv(a2.argv[2], &cap) != EOS) {
		cmderr(0, "line %u: bad host capability %s\n",
			lineno, a2.argv[2]);
		return 1;
	}
	return addhost(a1.argv[0], &a1.argv[2], hosta, cap);
}

/*
 * parse SRI style network map
 *
 *	NET	name,number
 *
 *	HOST	name,address(es),status,system,machine,synonym(s)
 *
 */
parsesri()
{
	ecmderr(0, "SRI not implemented yet\n");
}

/*
 * parser for BBN style input lines
 *	words separated by white space
 */
arglist
parse1(sp)
register char *sp;
{
	register int c, inword;
	arglist args;

	args.argv = atop;
	inword = 0;
	while (c = *sp++) {
		if (isspace(c)) {
			if (inword) {
				inword = 0;
				addchr(EOS);
			}
			continue;
		}
		if (!inword) {
			inword = 1;
			addarg(1);
		}
		addchr(c);
	}
	if (inword)
		addchr(EOS);
	args.narg = atop - args.argv;
	addarg(0);
	return args;
}

/*
 * parser for comma separated list
 */
arglist
parse2(sp)
register char *sp;
{
	register int c, inword;
	arglist	args;

	args.argv = atop;
	inword = 0;
	while (c = *sp++) {
		if (c == ',') {
			if (!inword)
				addarg(1);
			inword = 0;
			addchr(EOS);
			continue;
		}
		if (!inword) {
			if (isspace(c))
				continue;
			inword = 1;
			addarg(1);
		}
		addchr(c);
	}
	if (inword)
		addchr(EOS);
	args.narg = atop - args.argv;
	addarg(0);
	return args;
}

/*
 * add an argument to the list
 */
addarg(f)
{
	if (atop >= &abuf[NARGS])
		ecmderr(0, "line %u: too many arguments\n", lineno);
	*atop++ = f? ctop : NULL;
}

/*
 * add a character to the argument buffer
 */
addchr(c)
{
	if (ctop >= &cbuf[NACHR])
		ecmderr(0, "line %u: too many argument characters\n", lineno);
	*ctop++ = c;
}

/*
 * add a network
 */
addnet(name, syns, net, cap)
char	*name;		/* primary name */
char	**syns;		/* -> aliases */
netnumb	net;		/* network number */
uint	cap;		/* capability */
{
	register net_ent *np;

	if (net_cnt <= net)
		net_cnt = net + 1;
	if (net_cnt >= NNETS)
		ecmderr(0, "line %u: out of network table space\n", lineno);
	np = &net_tab[net];
	np->net_name = addname(name);
	np->net_cap = cap;
	if (syns)
		while (*syns)
			addname(*syns++);
	addname(NULL);
	return 0;
}

/*
 * add host to host table
 */
addhost(name, syns, hosta, hostc)
char	*name;		/* primary name */
char	**syns;		/* -> aliases */
netaddr	hosta;		/* host address */
uint	hostc;		/* host capability */
{
	register host_ent *hp;

	if (host_cnt >= NHOSTS)
		ecmderr(0, "line %u: out of host table space\n", lineno);
	hp = &host_tab[host_cnt++];
	hp->host_addr = hosta;
	hp->host_cap = hostc;
	hp->host_name = addname(name);
	if (syns)
		while (*syns)
			addname(*syns++);
	addname(NULL);
	return 0;
}

/*
 * find network name
 */
findnet(name, netp)
char	*name;
netnumb	*netp;
{
	register char *lp, *tp;
	register net_ent *np;
	register int nn;

	for (np = &net_tab[0], nn = net_cnt; nn-- > 0; ++np)
		for (lp = &char_buf[np->net_name]; *lp; ++lp) {
			tp = name;
			while (*tp++ == *lp)
				if (*lp++ == EOS) {
					*netp = np - net_tab;
					return 1;
				}
			while (*lp != EOS)
				++lp;
		}
	cmderr(0, "line %u: unknown network %s\n", lineno, name);
	return 0;
}

/*
 * build host address
 */
makeaddr(haddr, nname, hostp)
char	*haddr;		/* host address */
char	*nname;		/* network name */
netaddr	*hostp;		/* -> network address */
{
	netnumb	net;		/* network number */
	netaddr	hosta;		/* actual address */

	if (!findnet(nname, &net))
		return 0;
	/*
	 * parse the host address
	 */
	hosta = _nhosta(haddr, net);
	if (isbadhost(hosta)) {
		cmderr(0, "line %u: bad host address %s\n", haddr);
		return 0;
	}
	*hostp = hosta;
	return 1;
}

/*
 * return pointer to network entry
 *	actually a substitute for the library routine of the same name
 *	used by _nhosta()
 */
net_ent *
_nnetp(net)
netnumb net;
{
	if (net >= net_cnt)
		ecmderr(0, "bad net number %D supplied by _nhosta\n", net);
	return &net_tab[net];
}

/*
 * add name to list
 *	return index of entry
 *	an empty string is considered an error here
 *	since it is used to terminate lists
 */
addname(name)
char	*name;
{
	int	x;
	register char *p;

	p = name;
	if (p == NULL)
		p = "";	/* NULL implies empty */
	else if (*p == EOS)
		ecmderr(0, "internal error: empty string in addname\n");
	x = char_cnt;
	do
		if (char_cnt >= NCHARS)
			ecmderr(0, "line %u: out of character space\n", lineno);
	while (char_buf[char_cnt++] = *p++);
	++name_cnt;
	return x;
}

/*
 * add dial digits to dial buffer
 * same algorithm as addname()
 *	this routine supplants the one supplied by the library
 *	used by _nhosta()
 */
unsigned
_dialstr(digits)
char	*digits;
{
	unsigned x;
	register char *p;

	p = digits;
	if (p == NULL || *p == EOS)
		ecmderr(0, "internal error: null or empty dial digits\n");
	x = dial_cnt;
	do
		if (dial_cnt >= NDIALS)
			ecmderr(0, "line %u: out of dial digit space\n", lineno);
	while (dial_buf[dial_cnt++] = *p++);
	return x;
}

/*
 * sort host array
 */
sorter()
{
	register int hi;
	register host_ent *hp;
	register net_ent *np;
	netnumb	nnet, hnet;
	int	hostcmp();
	/*
	 * sort host table by net,imp,hoi
	 */
	qsort(host_tab, host_cnt, sizeof(host_ent), hostcmp);
	/*
	 * find 1st host for each net and # hosts/net
	 */
	nnet = -1;
	for (hi = 0; hi < host_cnt; ++hi) {
		hp = &host_tab[hi];
		hnet = net_part(hp->host_addr);
		if (hnet < nnet)
			ecmderr(0, "internal error: bad sort\n");
		if (hnet > nnet) {
			nnet = hnet;
			np = &net_tab[nnet];
			np->net_host = hi;
			np->net_nhost = 0;
		}
		++np->net_nhost;
	}
}

/*
 * compare two network addresses, piece by piece
 */
hostcmp(h1, h2)
host_ent *h1, *h2;
{
	register int i;

	if (i = net_part(h1->host_addr) - net_part(h2->host_addr))
		return i;
	if (i = imp_part(h1->host_addr) - imp_part(h2->host_addr))
		return i;
	if (i = hoi_part(h1->host_addr) - hoi_part(h2->host_addr))
		return i;
	return strcmp(nameof(h1->host_name), nameof(h2->host_name));
}

/*
 * build network search chain
 */
chainit()
{
	register map_index *pp;
	register net_ent *np;
	register int nn;

	pp = &net_tab[0].net_next;
	for (np = &net_tab[1], nn = net_cnt - 1; nn-- > 0; ++np) {
		if (np->net_nhost <= 0)
			continue;
		*pp = np - net_tab;
		pp = &np->net_next;
	}
	*pp = 0;
}

/*
 * generate binary host map
 */
genmap()
{
	int	ofd;
	map_head	head;
	extern long	lseek();

	/*
	 * determine appropriate output file and build binary map
	 *	if the default is used, maintain backup version
	 */
	if (!ofile) {
		ofile = newmap;
		if (access(newmap, 0) == 0 && unlink(newmap) != 0)
			ecmderr(-1, "%s cannot be removed\n", newmap);
	}
	printf("output file = %s\n", ofile);
	/*
	 * create output file
	 */
	if ((ofd = creat(ofile, fmodes(ofile))) < 0)
		ecmderr(-1, "can't create %s\n", ofile);
	/*
	 * Initialize the fields of the header.
	 */
	head.map_nnet = net_cnt;
	head.map_nhost = host_cnt;
	head.map_nchar = char_cnt;
	head.map_ndial = dial_cnt;
	/*
	 * Write header onto output file.
	 */
	ckwrite(ofd, (char *)&head, sizeof head);
	ckwrite(ofd, (char *)net_tab, net_cnt * sizeof(net_ent));
	ckwrite(ofd, (char *)host_tab, host_cnt * sizeof(host_ent));
	ckwrite(ofd, (char *)char_buf, char_cnt);
	ckwrite(ofd, (char *)dial_buf, dial_cnt);
	printf("map size = %D bytes\n", lseek(ofd, 0L, 1));
	close(ofd);
}

/*
 * write something, die if it doesn't work
 */
ckwrite(fd, buf, cnt)
char *buf;
{
	if (write(fd, buf, cnt) != cnt)
		ecmderr(-1, "write error fdes %d\n", fd);
}

/*
 * change the official binary network map
 */
chgmap()
{
	if (access(curmap, 0) == 0) {
		unlink(oldmap);
		if (link(curmap, oldmap) == -1)
			ecmderr(-1, "can't link %s to %s\n", curmap, oldmap);
		if (unlink(curmap) == 1)
			ecmderr(-1, "can't unlink %s\n", curmap);
		printf("%s renamed %s\n", curmap, oldmap);
	}
	if (link(newmap, curmap) == -1) {
		cmderr(-1, "can't link %s to %s\n", newmap, curmap);
		if (link(oldmap, curmap) == 0) {
			unlink(oldmap);
			ecmderr(0, "%s restored\n", curmap);
		}
		ecmderr(-1, "can't relink %s to %s\n", oldmap, curmap);
	}
	printf("%s renamed %s\n", newmap, curmap);
	unlink(newmap);
}
