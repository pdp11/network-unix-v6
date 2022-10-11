#include "stdio.h"
#include "netlib.h"
#include "netmap.h"
#include "netfiles.h"

FILE		*mapf;
map_head	maph;
net_ent		netb;
host_ent	hostb;

main(ac, av)
char **av;
{
	mapf = fopen(ac > 1? av[1] : MAP_BIN, "r");
	if (mapf == NULL) {
		printf("can't open map\n");
		exit(1);
	}
	prhead();
	prnets();
	prhosts();
	prchars("name chars", maph.map_nchar);
	prchars("dial digits", maph.map_ndial);
	if (getc(mapf) != EOF) {
		printf("didn't read entire map\n");
		exit(1);
	}
	exit(0);
}

prhead()
{
	if (fread(&maph, sizeof maph, 1, mapf) != 1) {
		printf("can't read header\n");
		exit(1);
	}
	printf("%u nets, %u hosts, %u name chars, %u dial digits\n",
		maph.map_nnet, maph.map_nhost, maph.map_nchar, maph.map_ndial);
}

prnets()
{
	register int i;

	printf("\n    net#    name     cap    next   host#   nhost\n");
	for (i = 0; i < maph.map_nnet; ++i) {
		if (fread(&netb, sizeof netb, 1, mapf) != 1) {
			printf("can't read net ent %d\n", i);
			exit(1);
		}
		printf("%8d%8u%8o%8u%8u%8u\n", i,
			netb.net_name, netb.net_cap,
			netb.net_next,
			netb.net_host, netb.net_nhost);
	}
	printf("\n");
}

prhosts()
{
	register int i;

	printf("\n   host#    name     address     cap\n");
	for (i = 0; i < maph.map_nhost; ++i) {
		if (fread(&hostb, sizeof hostb, 1, mapf) != 1) {
			printf("can't read host ent %d\n", i);
			exit(1);
		}
		printf("%8d%8u%4o%4o%4o%4o%8o\n", i,
			hostb.host_name,
			hostb.host_addr._na_b[0], hostb.host_addr._na_b[1],
			hostb.host_addr._na_b[2], hostb.host_addr._na_b[3],
			hostb.host_cap);
	}
	printf("\n");
}
prchars(name, count)
char *name;
{
	register int c, i, j;

	printf("\n   char# %s ...\n", name);
	j = 0;
	for (i = 0; i < count; ++i) {
		if ((c = getc(mapf)) == EOF) {
			printf("EOF at char %d\n", i);
			exit(1);
		}
		if (j == 0)
			printf("%8u", i);
		if (c < ' ' || c > 0177)
			printf("%4o", c); else
			printf(" '%c'", c);
		if (!c || ++j >= 16) {
			j = 0;
			printf("\n");
		}
	}
	printf("\n");
}
