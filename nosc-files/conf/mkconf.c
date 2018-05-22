#/*
Module Name:
	mkconf.c

Installation:
	cc -O -s mkconf.c
	if ! -r a.out exit
	chmod 755 a.out
	mv a.out mkconf

Module history:
*/
#define CHAR	01
#define BLOCK	02
#define INTR	04
#define EVEN	010
#define KL	020
#define ROOT	040
#define NET	0100


char	*btab[]
{
#include "btab.h"
	0
};


char	*ctab[]
{
#include "ctab.h"
	0
};


struct tab
{
	char	*name;
	int	count;
	int	address;
	int	key;
	char	*codea;
	char	*codeb;
	char	*codec;
	char	*coded;
	char	*codee;
} table[]
{
#include "table.h"

	0
};
/**/
char	*stra[]
{
	"/ low core",
	"",
	"br4 = 200",
	"br5 = 240",
	"br6 = 300",
	"br7 = 340",
	"",
	". = 0^.",
	"\tbr\t1f\t\t/ 0 entry point",
	"\t4",
	"",
	"/ trap vectors",
	"\ttrap; br7+0.\t\t/  4 bus error",
	"\ttrap; br7+1.\t\t/ 10 illegal instruction",
	"\ttrap; br7+2.\t\t/ 14 bpt-trace trap",
	"\ttrap; br7+3.\t\t/ 20 iot trap",
	"\ttrap; br7+4.\t\t/ 24 power fail",
	"\ttrap; br7+5.\t\t/ 30 emulator trap",
	"\ttrap; br7+6.\t\t/ 34 system entry",
	"",
	". = 40^.",
	".globl\tstart, dump",
	"1:\tjmp\tstart\t\t/ 40",
	"\tjmp\tdump\t\t/ 44",
	"",
	0,
};

char	*strb[]
{
	"",
	". = 240^.",
	"\ttrap; br7+7.\t\t/ 240 programmed interrupt",
	"\ttrap; br7+8.\t\t/ 244 floating point",
	"\ttrap; br7+9.\t\t/ 250 segmentation violation",
	0
};

char	*strc[]
{
	"",
	"/ floating vectors",
	". = 300^.",
	0,
};

char	*strd[]
{
	"",
	"//////////////////////////////////////////////////////",
	"/\t\tinterface code to C",
	"//////////////////////////////////////////////////////",
	"",
	".globl\tcall, trap",
	0
};

char	*stre[]
{
	"/*",
	" */",
	"",
	"int\t(*bdevsw[])()",
	"{",
	0,
};

char	*strf[]
{
	"\t0",
	"};",
	"",
	"int\t(*cdevsw[])()",
	"{",
	0,
};

char	*strg[]
{
	"\t0",
	"};",
	"",
	"int\trootdev\t{(%d<<8)|0};",
	"int\tswapdev\t{(%d<<8)|0};",
	"int\tswplo\t4000;\t/* cannot be zero */",
	"int\tnswap\t872;",
	0,
};
/**/
int	fout;
int	root	-1;

main()
{
	register struct tab *p;
	register *q;
	int i, n, ev, nkl;
	int flagf, flagb;

	while(input());

/*
 * pass1 -- create interrupt vectors
 */
	nkl = 0;
	flagf = flagb = 1;
	fout = creat("l.s", 0664);
	puke(stra);
	for(p=table; p->name; p++)
	if(p->count != 0 && p->key & INTR) {
		if(p->address>240 && flagb) {
			flagb = 0;
			puke(strb);
		}
		if(p->address >= 300) {
			if(flagf) {
				ev = 0;
				flagf = 0;
				puke(strc);
			}
			if(p->key & EVEN && ev & 07) {
				printf("\t.=.+4\n");
				ev =+ 4;
			}
			ev =+ p->address - 300;
		} else
			printf("\n. = %d^.\n", p->address);
		n = p->count;
		if(n < 0)
			n = -n;
		for(i=0; i<n; i++)
			if(p->key & KL) {
				printf(p->codea, nkl, nkl);
				nkl++;
			} else
			printf(p->codea, i, i);
	}
	if(flagb)
		puke(strb);
	puke(strd);
	for(p=table; p->name; p++)
	if(p->count != 0 && p->key & INTR)
		printf("%s%s", p->codeb, p->codec);
	flush();
	close(fout);

/*
 * pass 2 -- create configuration table
 */

	fout = creat("c.c", 0664);
	puke(stre);
	for(i=0; q=btab[i]; i++) {
		for(p=table; p->name; p++)
		if(equal(q, p->name) &&
		   (p->key&BLOCK) && p->count) {
			printf("%s\t/* %d %s */\n", p->coded, i, q);
			if(p->key & ROOT)
				root = i;
			goto newb;
		}
		printf("\t&nodev,\t\t&nodev,\t\t&nodev,\t\t0,\t/* %d %s */\n", i, q);
	newb:;
	}
	puke(strf);

	for(p=table; p->name; p++)   /* if net device selected, pick up NCP */
		if((p->key & NET) && p->count) p->name="net";

	for(i=0; q=ctab[i]; i++) {
		for(p=table; p->name; p++)
		if(equal(q, p->name) &&
		   (p->key&CHAR) && p->count) {
			printf("%s\t/* %d %s */\n", p->codee, i, q);
			goto newc;
		}
		printf("\t&nodev,    &nodev,    &nodev,    &nodev,    &nodev,\t/* %d %s */\n", i, q);
	newc:;
	}
	puke(strg, root);
	flush();
	close(fout);
	if(root < 0)
		write(2, "no block device given\n", 22);
}

puke(s, a)
char **s;
{
	char *c;

	while(c = *s++) {
		printf(c, a);
		printf("\n");
	}
}
/**/
input()
{
	char line[100];
	register char *p;
	register struct tab *q;
	register n;

	p = line;
	while((n=getchar()) != '\n') {
		if(n == 0)
			return(0);
		if(n == ' ' || n == '\t')
			continue;
		*p++ = n;
	}
	*p++ = 0;
	n = 0;
	p = line;
	while(*p>='0' && *p<='9') {
		n =* 10;
		n =+ *p++ - '0';
	}
	if(n == 0)
		n = 1;
	if(*p == 0)
		return(1);
	for(q=table; q->name; q++)
	if(equal(q->name, p)) {
		if(root < 0 && (q->key&BLOCK)) {
			root = 0;
			q->key =| ROOT;
		}
		if(q->count < 0) {
			printf("%s: no more, no less\n", p);
			return(1);
		}
		q->count =+ n;
		if(q->address <= 300 && q->count > 1) {
			q->count = 1;
			printf("%s: only one\n", p);
		}
		return(1);
	}
	if(equal(p, "done"))
		return(0);
	printf("%s: cannot find\n", p);
	return(1);
}

equal(a, b)
char *a, *b;
{

	while(*a++ == *b)
		if(*b++ == 0)
			return(1);
	return(0);
}

getchar()
{
	int c;

	c = 0;
	read(0, &c, 1);
	return(c);
}
