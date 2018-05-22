#/*
Module Name:
	isconf.c -- scan a configuration file for a specific entry

Installation:
	cc -O isconf.c
	if ! -r a.out exit
	chmod 755 a.out
	mv a.out isconf

Synopsis:
	isconf <configuration-file> <search-word>

Function:
	Scan a file to see if the specified word is located at the beginning
	of the line, optionally preceeded by white space or numeric digits.

Diagnostics:

Module History:
	Created 20Jul78 by Greg Noel
*/
	int fi[259];		/* space for input buffer */

main(argc, argv)
int argc;
char **argv;
{
	register int c;		/* character from input */
	register char *p;	/* pointer to argument word */

	if(argc < 3) {
		printf("isconf: usage is 'isconf <file> <word>'\n");
		exit(2);
	}
	if(fopen(argv[1], fi) < 0) {
		printf("isconf: can't open configuration file\n");
		exit(3);
	}
	for(;;) {
		p = argv[2];
		do {
			if( (c = nxtchr()) == '\n')
				if(*p == '\0')
					exit(0);	/* configured */
				else
					break;
		} while(c == *p++);
		while(c != '\n') c = nxtchr();
	}
}
nxtchr()
{
	register int c;

	do {
		if( (c = getc(fi)) < 0) exit(1);	/* not configured */
	} while(c == '\t' || c == ' ' || (c >= '0' && c <= '9') );
	return c;
}
