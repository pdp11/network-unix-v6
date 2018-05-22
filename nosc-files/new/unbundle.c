/* Program to unbundle stuff shipped from KL.
Format is:
	line 0: "<File n>" where n is format version number (this is 1)
	line 1: <unix filename to store in>
	line 2: <# chars in following text>
	line 3-n: <text of file>
*/

char filename[200];
char sfillen[50];

long fillen;
long atol();

int nfiles;

struct buf {
	int fildes;
	int nleft;
	char *nextp;
	char buff[512];
};
struct buf inb,outb;


main(argc,argv)
char **argv;
{
	char *ifil;
	register int c;
	long i;
	int ok,linect;
	if(argc < 2)
	{	printf("Usage: <filename>\n");
		exit(0);
	}
	ifil = argv[1];
	if(fopen(ifil, &inb) < 0)
	{	printf("Cannot open %s\n",ifil);
		exit(0);
	}
	while ((c = getc(&inb)) == '<')
	{	nxtlin();
		rdlin(filename);
		rdlin(sfillen);
		fillen = atol(sfillen);
		printf("%s  %ld  ",filename,fillen);
		ok = 1;
		if(fcreat(filename,&outb) < 0)
		{	printf("Cannot write %s  ",filename);
			ok = 0;
		}
		linect = 0;
		for(i=fillen; i > 0; --i)
		{	if ((c = getc(&inb)) >= 0)
			{	if(ok) putc(c,&outb);
				if(c == '\n')
				{	--i;
					linect++;
				}
			}
			else snipped();
		}
		if (ok)
		{
			fflush(&outb);
			close(outb.fildes);
		}
		nfiles++;
		if (linect)
		{	printf("(%ld)",(fillen-linect));
		}
		printf("\n");
	}
	if (c >= 0)
	{	printf("Next file not found, halting.\n");
	}
	printf("%d files unbundled.\n",nfiles);
	exit(0);
}

snipped()
{	printf("Unexpected EOF\n");
	cleanup(0);
}
cleanup(arg)
{
	if(outb.fildes >= 0)
	{	fflush(&outb);
		close(outb.fildes);
	}
	exit(arg);
}
nxtlin(str)
char *str;
{
	register int c;
	while((c = getc(&inb)) >= 0)
		if (c == '\n') return;
	snipped();
}
rdlin(str)
char *str;
{
	register int c;
	int first;
	first = 0;
	for(;;)
	{	if((c = getc(&inb)) < 0) snipped();
		if(first==0)
			if(c == ' ') continue;
			else first++;
		if(c != '\n') *str++ = c;
		else
		{	*str = 0;
			return;
		}
	}
}

