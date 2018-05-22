#/*
Module Name:
	install.c

Installation:
	if $1e = finale goto finale
	cc install.c
	exit
: finale
	cc -O -s install.c
	if ! -r a.out exit
	su cp a.out /usr/bin/install
	rm -f a.out

Synopsis:
	install [-e] program [ parameters ]

Function:
	Install a program as directed by the "Installation" section of the
	module header.

Compile time parameters and effects:
	none.

Module History:
	Written by Greg Noel on 29 Dec 77.
	Modified to handle assembly files 27 Feb 79 by Greg Noel.
*/
struct {	/* input file buffering */
	int fildes;	/* file descriptor */
	int nleft;	/* chars left in buffer */
	char *nextp;	/* ptr to next character */
	char buff[512];	/* the buffer */
} in;

char line[300];

int out, len, fi;

main(argc, argv)
int argc;
char **argv;
{
	char *getline();
	register char *ln;
	register char **av;

	for(av = argv; **++av == '-';);
	if(av - argv >= argc) {
		printf("Usage: 'install [-e] program [parameters]'\n");
		exit();
	}
	if(fopen(*av, &in) < 0) {
		printf("Can't open program file(%s)\n", *av);
		exit();
	}
	*av = "-";		/* use standard input */

	/* scan for the "Installation" marker */

	while(strcomp(getline(), "installation:\n") != 0);

	/* make a phantom temporary file for the installlation parameters */

	if( (out = creat("/tmp/install", 0400)) < 0) {
		printf("Can't create temporary file -- try again\n");
		exit();
	}
	fi = open("/tmp/install", 0);	/* open temp for input as well */
	unlink("/tmp/install");	/* unlink temp file -- doesn't go away */
				/*    'cause file is open */
	if(fi < 0) {
		printf("Can't read temporary file\n");
		exit();
	}


	/* copy text until a null line is encountered */

	while(*(ln = getline()) != '\n') {
		write(out, ln, len);
	}

	/* transfer to shell with our temp file as standard input */

	close(0);		/* make standard input */
	dup(fi);		/*    the temporary file */

	close(in.fildes);	/* close our work files */
	close(out);
	close(fi);

	argv[argc] = 0;		/* end-of-list marker */
	execv("/bin/sh", argv);
	printf("Can't get shell!!\n");
	exit();
}
char *getline()
{
	register char *p;

	p = line;
	do {
		if( (*p = getc(&in)) < 0) {
			printf("EOF encountered before installation parameters\n");
			exit();
		}
	} while(*p++ != '\n');
	*p = 0;	/* null terminate */
	len = p-line;
	if(*line != '/')
		return(line);		/* high-level language line */
	len--;
	return(line+1);			/* assembly language line */
}
strcomp(a, b)
char *a, *b;
{
	register char *p, *q, c;

	p = a;  q = b;
	do {
		if( (c = *p++) >= 'A'  &&  c <= 'Z')
			c =- 'A' - 'a';
	} while(c == *q++  &&  c);
	return(*--p - *--q);
}
