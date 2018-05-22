#/*
Module Name:
	mk.c

Installation:
	cc -O -s mk.c
	if ! -r a.out exit
	chmod 755 a.out
	mv a.out mkconf

Module History:
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
#	include "btab.h"
	0
};
char	*ctab[]
{
	"console",
#	include "ctab.h"
	0
};
/**/
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
	".globl\tstart, dump",
	"1:\tjmp\tstart\t\t/ 40",
	"\tjmp\tdump\t\t/ 44",
	"",
	0,
};

char	*strb1[]
{
	"",
	"\ttrap; br7+7.\t\t/ 114  11/70 parity",
	"",
	0
};

char	*strb2[]
{
	"",
	"\ttrap; br7+7.\t\t/ 240 programmed interrupt",
	"\ttrap; br7+8.\t\t/ 244 floating point",
	"\ttrap; br7+9.\t\t/ 250 segmentation violation",
	"",
	0
};

char	*strc[]
{
	"/ floating vectors",
	"",
	0,
};

char	*strd[]
{
	"",
	"//////////////////////////////////////////////////////",
	"/\t\tinterface code to C",
	"//////////////////////////////////////////////////////",
	"",
	"\t.globl\tbcall, call, trap",
	"badint0: jsr\tr0,bcall;  000",
	"badint1: jsr\tr0,bcall;  100",
	"badint2: jsr\tr0,bcall;  200",
	"badint3: jsr\tr0,bcall;  300",
	"badint4: jsr\tr0,bcall;  400",
	"badint5: jsr\tr0,bcall;  500",
	"badint6: jsr\tr0,bcall;  600",
	"badint7: jsr\tr0,bcall;  700",
	"",
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
	"int\tnblkdev\t{ sizeof bdevsw/8 /* four words per entry */ };",
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
	"int\tnchrdev\t{ sizeof cdevsw/10 /* five words per entry */ };",
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

int	addr	50,	intnr	012;

main()
{
	register struct tab *p;
	register *q;
	int i, n, nkl;
	int flagf;

	while(input());

/*
 * pass 0 -- check if any selected devices have same vector address
 */
	q = &""; i = 0;
	for(p=table; p->address < 300; p++) if(p->count) {
		if(i==p->address) {
			printf("can't have both %s and %s -- %s selected\n",
				q, p->name, q);
			p->count = 0;  /* zap it */
		} else {
			q = p->name; i = p->address;
		}
	}

/*
 * pass 1 -- create interrupt vectors
 */
	nkl = 0;
	flagf = 1;
	fout = creat("l.s", 0664);
	puke(stra);
	for(p=table; p->name; p++)
	if(p->count != 0 && p->key & INTR) {
		if(p->address >= 300) {
			if(flagf) {
				flagf = 0;
				adjaddr(300);
				puke(strc);
			}
			if(p->key & EVEN && intnr & 1) {
				adjaddr(cvtintnr(intnr+1));
			}
		} else
			adjaddr(p->address);
		n = p->count<0 ? -p->count : p->count;
		for(i=0; i<n; i++)
			if(p->key & KL) {
				outintv(p->codea, nkl);
				nkl++;
			} else
				outintv(p->codea, i);
		printf("\n");
	}
/*TOOBIG  not enuf room on a /40 for this....
	adjaddr(800);
TOOBIG*/if (addr < 250) adjaddr(250);	/* picks up all trap vectors */
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

/*
 * error vector insertion routines
 */

adjaddr(new)
{
	if (new < addr) {
		/*  O O P S ! !  */
		write(2, "addressing error in l.s -- see listing\n", 39);
		printf("* * * error * * *  address should be %d here and isn't!!\n", new);
		addr = new;
	}

	if(new==addr) return;

	for (; new > addr; addr = cvtintnr(++intnr)) {
		if(addr==114) {		/* parity vector */
			puke(strb1);
		} else
		if(addr==240) {       /* various vectors */
			puke(strb2);
			intnr =+ 2;  /* skip extra vectors */
		} else     /* generate an error interrupt vector */
			printf("\tbadint%d;  br7+%d.\t/ %d\n",
				intnr>>4, intnr&017, addr);
	}
	printf("\n");
}

outintv(c, v)
char *c;
{
	char buf[100]; register char *p;

	while(*c) {   /* while there's more in the string */
		for(p=buf; (*p = *c++) != '\n'; p++);  /* copy line */
		*p = '\0';   /* put in null terminator */
		printf(buf,v);   /* output interrupt vector code */
		printf("\t\t/ %d\n", addr);   /* output position comment */
		addr = cvtintnr(++intnr);
	}  /* end of while there's more */
}

cvtintnr(i)
{
	return (  ( (i>>4)*10 + ((i>>1)&7) )*10 + ((i<<2)&7)  );
}
