#/*
Module Name:
	crash.c -- format a post mortem dump of unix

Installation:
	if $1x = finalx goto final
	cc crash.c
	exit
: final
	cc -O -s crash.c
	if ! -r a.out exit
	cp a.out /etc/crash
	rm -f a.out

Synopsis:
	crash [aps] [-s sfile] [-c cfile] [-f number]

Function:
	It examines a dump of unix which it looks for in the file
	sysdump.  It prints out the contents of the general
	registers, the kernel stack and a traceback through the
	kernel stack.  If an aps is specified, the ps and pc at
	time of interrupt are also printed out.  The dump of the
	stack commences from a "reasonable" address and all addresses
	are relocated to virtual addresses by using the value of
	kdsa6 found in the dump.
	  If the -s argument is found the following argument is taken
	to be the name of a file, containing a symbol table which
	should be used in interpreting text addresses.  The default
	is "/unix".  If the -c argument is found, the following argument
	is taken to be the name of a file which should be used instead
	of "sysdump".  If the -f argument is found, the following argument
	is taken to be the address of the beginning of a stack frame
	from which to begin the dump.  The default is to use the presumed
	contents of r5 at the time of the crash.

Restrictions:
	It will believe symbol tables that don't match the core image,
	producing pages of junk.

Diagnostics:
	Several, hopefully self-expainatory.

Files:
	sysdump
	/unix

See Also:

Bugs:
	A core image that's too short will cause it to loop.  This can be
	fixed when I get around to it....

Compile time parameters and effects:

Module History:
	Stolen originally from the University of Illinois.  Much-modified
	    by Greg Noel at NOSC.
	Slightly fixed for split I/D kernel by KLH at SRI.
*/
#include "param.h"	/* parameters used in later files */
#include "user.h"
#include "text.h"
#include "proc.h"	/* has big proc table; only need small one */
/*	struct proc proc[1];	/* force proc table to only have one entry */
#include "inode.h"	/* has big inode table; only need small one */
/*	struct inode inode[1];	/* force inode table to only have one entry */

	int buf[8];
	int buf1[8];	char*maxloc;
	struct syment {
		char name[8];
		int flags;
		char *value;
		} ;
	struct syment *symtab;
	struct syment *maxsym;
	char *corref {"sysdump"};
	char *symref {"/unix"};
	int texta;	/* address of text table */
	char*proca;	/* address of proc table */
	int stroct();
	int core;
	char *curloc;
	char *datsiz;	/* size of initialized data */
	char *bsssiz;	/* size of bss data area */
	char *dataval;	/* total of datsiz + bsssiz */

/**/
main(argc,argv)
 int argc;
 char **argv;
{
	register int i;
	register int j;
	register struct syment *p;
	struct syment *q;

	int sym;
	int trapstack;
	int sp;
	int r5;
	int int_kdsa;
	extern int fout;

	trapstack = 0;	r5 = 0;
	/* parse off the arguments and do what I can with them */
	for(i=1; i<argc; i++)
	{	if ((*argv[i] == '-') && ((i+1) < argc))
		{	if (argv[i][1] == 'c')
			{	corref = argv[++i];
				continue;
			} else if (argv[i][1] == 's')
			{	symref = argv[++i];
				continue;
			} else if (argv[i][1] == 'f')
			{	r5 = stroct(argv[++i]);
				continue;
			}
			else printf("Bad argument: %s\n",
					argv[i]);
		}
		else
		{	trapstack = stroct(argv[i]);
			if (trapstack == 0)
				printf("Bad argument: %s\n",argv[i]);
		}
	}

	core = open(corref,0);
	if (core < 0)
		done("Unable to open %s\n",corref);

	sym = open(symref,0);
	if (sym < 0)
		done("Unable to open %s\n",symref);
	read(sym,buf,020);
	if (buf[0]!=0407 && buf[0]!=0410 && buf[0]!=0411)
		done("Format error in %s\n",symref);
	datsiz = buf[2];
	bsssiz = buf[3];
	dataval = datsiz + bsssiz;
	seekit(sym,buf[1]);
	seekit(sym,buf[2]);
	if (buf[7] != 1)
	{	seekit(sym,buf[1]);
		seekit(sym,buf[2]);
	}
	j = (i = buf[4]/12)*12;
	if (i == 0)
		done("No namelist in %s\n",symref);
	symtab = sbrk(j);
	read(sym,symtab,j);
	maxsym = symtab + i;

	fout = dup(1);
/**/
	if((p = findsym("_text")) == 0)
		done("\n\nError, location of text table not in symbol table.\n");
	texta = p->value;
	if((proca = findsym("_proc")) == 0)
		done("\n\nError, location of proc table not in symbol table.\n");
	printf(
"\14 F S UID   PID  PPID  PGRP CPU NICE PRI   ADDR  SZ TXT TIME SIG WCHAN");
	for(i = 0; i < NPROC; i++) {
		seek(core, proca->value + i*sizeof proc[0], 0);
		if(read(core, &proc[0], sizeof proc[0]) != sizeof proc[0])
			done("Read error in proc table.\n");
		if(proc[0].p_stat == 0) continue;
		printf("\n%2o %c ", proc[0].p_flag, "?SWRIZT"[proc[0].p_stat]);
		printf("%3d", proc[0].p_uid & 0377);
		printf("%6d%6d%6d ", proc[0].p_pid, proc[0].p_ppid, proc[0].p_pgrp);
		printf("%3d %4d%4d ", proc[0].p_cpu & 0377, proc[0].p_nice, proc[0].p_pri);
		printf("%6o %3d ", proc[0].p_addr, proc[0].p_size);
		if(proc[0].p_textp)
			/* W A R N I N G -- the definition of p_textp in proc.h
			must be changed to a char * from an int * to permit the
			arithmetic in the next line to work properly.  (I don't
			know why it wasn't that way in the first place; nobody
			depends upon it being defined that way!)  */
			printf("%3d ", ((proc[0].p_textp - texta)/sizeof text[0]) + 1);
		else	printf("    ");
		printf("%4d ", proc[0].p_time);
		printf("%3d", proc[0].p_sig);
		if(proc[0].p_clktim) printf(" (CLKTIM=%o)", proc[0].p_clktim);
		symbol(proc[0].p_wchan, 1);
	}

		/* Now display info from the shared-text table */

	printf("\n\n\tShared texts\n\nTXT  DADDR  CADDR  SZ I-NODE CNT RES\n");
	seek(core, texta, 0);
	if(read(core, text, sizeof text) != sizeof text)
		done("\nError, couldn't read text table.\n");
	for(i = 0; i < NTEXT; i++) {
		if(text[i].x_count == 0) continue;
		printf("%3d %6o %6o %3d %6o %3d %3d\n", i+1, text[i].x_daddr,
			text[i].x_caddr, text[i].x_size, text[i].x_iptr,
			text[i].x_count, text[i].x_ccount);
	}
/**/
	if((p = findsym("start")) == 0)
		done("Can't tell if system is split\n");

	/* Seek to beginning of core image in both files */
	if((p->flags&077) == 043) {	/* system is split */
		printf("\nSystem is split\n");
		seek(sym, 16 + datsiz, 0);	/* Assumes bin was sysfixed */
			/* Text gets put into 1st page after data seg */
		seek(core, ((dataval+077)>>6)<<6, 0);
		curloc=0;
	}
	else {
		printf("\nSystem not split\n");
		seek(sym,16+040,0);	/* beginning of core image */
		seek(core,040,0);
		curloc=040;
	}
	maxloc = buf[1];
	for(; curloc<maxloc; curloc=+16){
		read(sym,buf1,16);
		read(core,buf,16);
		for(j=0;j<8;j++) if(buf1[j] != buf[j]) {
			printf("\nMemory text doesn't match load module:\n");
			octout(curloc); printf(":");
			for(i=0; i<8; i++) {
				if(i == 4) printf(" ");
				printf("  "); octout(buf1[i]);
			}
			printf("\n       ");
			for(i=0; i<8; i++) {
				if(i == 4) printf(" ");
				printf("  "); octout(buf[i]);
			}
			putchar('\n');
			break;
		}
	}
	close(sym);

	for(p = symtab; p < maxsym; p++) {
		if(p->flags != 043 && p->flags != 044)	/* data symbol? */
			continue;
		if( (q = nextsym(p->value)) == 0)	/* _end symbol? */
			continue;
		for(i = 0; i < 8; i++)
			putchar(p->name[i] ? p->name[i] : ' ');
		seek(core, curloc = p->value, 0);
		if(comp_sym(p, "_proc") == 0) /* do nothing! */; else
		if(comp_sym(p, "_text") == 0) /* do nothing! */; else
		if(comp_sym(p, "_buffers") == 0) /* do nothing! */; else
		if(comp_sym(p, "_cfree") == 0) /* do nothing! */; else
		if(comp_sym(p, "_inode") == 0) inode_dump(); else
		for(i = 0; curloc < q->value; i++)
		switch(i) {
		case 8:
			putchar('\n');
			octout(curloc);
			printf(": ");
			i = 0;
		case 0:
			if(read(core, buf, 16) != 16) {
				printf("Oops, read failed!");
				break;
			}
		case 4:
			putchar(' ');
		default:
			printf("  "); octout(buf[i]);
			curloc =+ 2;
		}
		putchar('\n');
	}
/**/
		/* now give a dump of the active user's area */

	printf("\014The users's area that was active at the time of the crash\n");

	/*	dump the registers	*/
	seek(core,4,0);
	read(core,buf,16);
	printf("\nr0: "); octout(buf[0]);
	printf("\tr1: "); octout(buf[1]);
	printf("\tr2: "); octout(buf[2]);
	printf("\tr3: "); octout(buf[3]);
	printf("\nr4: "); octout(buf[4]);
	printf("\tr5: "); octout(buf[5]);
	printf("\tsp: "); octout(buf[6]);
	printf("\tkdsa: "); octout(int_kdsa = buf[7]);
	sp = buf[6];
	if (r5 == 0) r5 = buf[5];

	/*	If an aps was specified, give ps, pc, sp at trap time */
	if (trapstack)
	{
		seek_kdsa(int_kdsa);
		seek(core,trapstack-0140000,1);
		read(core,buf,4);
		printf("\n\n\timmediately prior to the trap:\n");
		printf("\nsp: "); octout(trapstack+4);
		printf("\tps: "); octout(buf[1]);
		printf("\tpc: "); octout(buf[0]);
		symbol(buf[0], 0);
	}

	print_kdsa(int_kdsa, sp, r5);	/* print interrupted user's area */

		/* now give a dump of all the other user areas */

	for(p                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    