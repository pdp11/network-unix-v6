#define BBNNET
#include <stdio.h>
#include "netlib.h"
#include <sys/types.h>
#include "net.h"
#include "ifcb.h"
/*
 * mkgate [-i infile] [-o outfile]
 *
 * Generates a binary gateway file from ascii.  Records are of the form:
 * 
 * [ <loc net> -> <for net> via <gateway> [ flags <flag> ] ] [ ; [ <comment> ] ]
 */
int eof = 0;
int nrecs = 0;
int lines = 0;
char defin[] = "/etc/net/gateway";
char defout[] = "/etc/net/gateway.bin";

#define NCHAR 80
char line[NCHAR] = "";
char *lp = line;
FILE *fd;
extern errno;
extern char *progname;

main(argc, argv)
int argc;
char *argv[];
{
	register char *s;
	register out;
	register char *infile, *outfile;
	char *backfile;
	int error = 0;
	int recs = 0;
	int flags;
	struct gway gate;
	netaddr addr;
	char *getword();
	char *scat();
	progname = *argv;

	infile = defin;
	outfile = defout;

	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {

		case 'i':			/* input file */
			if (argc < 3)
				goto bad;
			infile = argv[2];
			break;

		case 'o':			/* output file */
			if (argc < 3)
				goto bad;
			outfile = argv[2];
			break;

		default: bad:
			ecmderr(0, "usage: %s [-i <infile>] [-o <outfile>]\n",
				argv[0]);
		}
		argv += 2; argc -= 2;
	}
	/*
	 * open input file
	 */
	if ((fd = fopen(infile, "r")) == NULL)
		ecmderr(errno, "Can't open input file \"%s\".\n", infile);
	/*
	 * if output file exists, try to back it up in outfile~
	 */
	backfile = scat(outfile, "~");
	if (access(outfile, 0) == 0) {
		unlink(backfile);
		if (link(outfile, backfile) < 0)
			ecmderr(errno, "Cannot backup \"%s\".\n", outfile);
		unlink(outfile);
	}

	if ((out = creat(outfile, 0644)) < 0)
		ecmderr(errno, "Can't create output file \"%s\".\n", outfile);
	/* 
	 * read the input file and generate output
	 */
	getword();
	for (;;) {
		/*
		 * ignore blank lines, exit on eof
		 */
		if ((s = getword()) == NULL && !eof)
			continue;
		else if (eof)
			break;
		/*
		 * check local network name
		 */
		if (isbadnet(gate.g_lnet = (net_t)getnet(s))) {
			cmderr(0, "\"%s\" line %d: bad local net \"%s\".\n",
				infile, lines, s);
			error++;
			break;
		}
		if ((s = getword()) == NULL || !seq(s, "->") || 
		    (s = getword()) == NULL) {
			badform(infile, lines);
			error++;
			break;
		}
		/*
		 * check foreign network name
		 */
		if (isbadnet(gate.g_fnet = (net_t)getnet(s))) {
			cmderr(0, "\"%s\" line %d: bad foreign net \"%s\".\n",
				infile, lines, s);
			error++;
			break;
		}
		if ((s = getword()) == NULL || !seq(s, "via") || 
		    (s = getword()) == NULL) {
			badform(infile, lines);
			error++;
			break;
		}
		/*
		 * check gateway name
		 */
		addr = getnhost(s, gate.g_lnet);
		if (isbadhost(addr)) {
			cmderr(0, "\"%s\" line %d: bad gateway \"%s\".\n",
				infile, lines, s);
			error++;
			break;
		}
		gate.g_local.s_addr = addr._na_l;
		/*
		 * check for flags
		 */
		if ((s = getword()) != NULL && seq(s, "flags")) {
			if ((s = getword()) == NULL) {
				badform(infile, lines);
				error++;
				break;
			}
			flags = atoi(s);
		}
		gate.g_flags = GWFORMAT | flags;
		gate.g_ifcb = NULL;
		flags = 0;
		/*
		 * write an output file record
		 */
		if (write(out, &gate, sizeof gate) != sizeof gate) {
			cmderr(errno, "Error writing output file \"%s\".\n",
				outfile);
			error++;
			break;
		} else
			recs++;
	}
	/*
	 * go to backup copy if any errors
	 */
	if (error) {
		unlink(outfile);
		link(backfile, outfile);
		unlink(outfile);
		exit(1);
	} else
		printf("Gateway file \"%s\" written from \"%s\", %d entries.\n",
			outfile, infile, recs);
}	
			
/*
 * get a token from the input file
 */
char *getword()
{
	char *word;  
	register c;

	/*
	 * skip separators
	 */
	for (;;)
		switch (*lp) {

		case ' ': case '\t':	/* token separators */
			lp++;
			break;
		
		case ';':		/* line separators */
		case '\n':
		case 0:
			if (fgets(line, NCHAR, fd) == NULL) 
				eof++;
			else {
				lines++;
				lp = line;
			}
			return(NULL);
		
		default:
			goto span;
		}

	/*
	 * span token until separator
	 */
span:
	word = lp;

	while ((c= *(++lp)) && c != ' ' && c != '\t' && c != ';' && c != '\n');
	*lp = 0;
	if (c != 0 && c != ';' && c != '\n')
		lp++;
	return(word);
}

badform(file, line)
char *file;
int line;
{
	cmderr(0, "\"%s\" line %d: syntax error.\n", file, line);
}
