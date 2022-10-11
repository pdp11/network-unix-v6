#include "stdio.h"
#include "netlib.h"

main(ac, av)
char **av;
{
	int	netf;
	extern char *progname;

	progname = *av++;
	netf = seq(progname, "net");
	if (!loadnetmap())
		ecmderr(0, "can't load the network map\n");
	if (*av == NULL) {
		if (netf)
			donet(netname(thisnet())); else
			dohost(hostname(thishost(ANYNET)));
	}
	else while (*av) {
		if (netf)
			donet(*av++); else
			dohost(*av++);
	}
	exit(0);
}

/*
 * print host information
 *
 * format:
 *	hostname,netname	address,netnumb[,capability]	[alias ...]
 */
dohost(name)
char *name;
{
	int	i;
	char	*p;
	netaddr	addr;
	netnumb	net;
	unsigned cap;

	addr = gethost(name);
	if (isbadhost(addr)) {
		cmderr(0, "bad host: %s\n", name);
		return;
	}
	net = net_part(addr);
	printf("%s,%s	%s", hostname(addr), netname(net), hostfmt(addr, 1));
	if ((cap = hostcap(addr)) > 0)
		printf(",%u", cap);
	for (i = 1; p = hostsyn(addr, i); ++i)
		printf(" %s", p);
	printf("\n");
}

/*
 * print network info
 */
donet(name)
char *name;
{
	int	i;
	char	*p;
	netnumb	net;
	unsigned cap;

	net = getnet(name);
	if (isbadnet(net)) {
		cmderr(0, "bad network: %s\n", name);
		return;
	}
	printf("%s	%d", netname(net), (int)net);
	if ((cap = netcap(net)) > 0)
		printf(",%u", cap);
	for (i = 1; p = netsyn(net, i); ++i)
		printf(" %s", p);
	printf("\n");
}
