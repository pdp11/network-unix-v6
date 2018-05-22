#/*
Module Name:
	banner.c

Installation:
	if $1x = finalx goto final
	cc banner.c
	exit
: final
	cc -O -s banner.c -lj
	if ! -r a.out exit
	su cp a.out /usr/bin/banner
	rm a.out

Synopsis:
	banner word ...

Function:

Restrictions:

Diagnostics:

Files:

See Also:

Bugs:

Compile time parameters and effects:

Module History:
	Original from Bell Labs distribution tape.
	Modified 12Jul78 by Greg Noel for extra characters (numbers) and
		multiple words on the banner line.
*/
#define nlines  6	/*number of lines in a banner character*/
#define pposs  85	/*number of print positions on a line (must be multiple of 4)*/
			/*followed by end of string character*/
#define pospch 8	/*number of char positions per banner char*/
#define chpln  10	/*number of banner characters per line*/

struct bann{
	  char alpha[nlines][pposs];
} buffer;

char ctbl[]{
	'a', 014,022,041,077,041,041,
	'b', 076,041,076,041,041,076,
	'c', 036,041,040,040,041,036,
	'd', 076,041,041,041,041,076,
	'e', 077,040,076,040,040,077,
	'f', 077,040,076,040,040,040,
	'g', 036,041,040,047,041,036,
	'h', 041,041,077,041,041,041,
	'i', 004,004,004,004,004,004,
	'j', 001,001,001,001,041,036,
	'k', 041,042,074,044,042,041,
	'l', 040,040,040,040,040,077,
	'm', 041,063,055,041,041,041,
	'n', 041,061,051,045,043,041,
	'o', 036,041,041,041,041,036,
	'p', 076,041,041,076,040,040,
	'q', 036,041,041,045,042,035,
	'r', 076,041,041,076,042,041,
	's', 036,040,036,001,041,036,
	't', 037,004,004,004,004,004,
	'u', 041,041,041,041,041,036,
	'v', 041,041,041,041,022,014,
	'w', 041,041,041,055,063,041,
	'x', 041,022,014,014,022,041,
	'y', 021,012,004,004,004,004,
	'z', 077,002,004,010,020,077,
	'0', 014,022,041,041,022,014,
	'1', 004,014,004,004,004,016,
	'2', 014,022,002,004,010,036,
	'3', 014,022,004,004,022,014,
	'4', 004,014,024,076,004,004,
	'5', 036,020,034,002,022,014,
	'6', 016,020,034,022,022,014,
	'7', 076,004,004,010,010,020,
	'8', 014,022,014,022,022,014,
	'9', 014,022,022,016,002,002,
	'A', 014,022,041,077,041,041,
	'B', 076,041,076,041,041,076,
	'C', 036,041,040,040,041,036,
	'D', 076,041,041,041,041,076,
	'E', 077,040,076,040,040,077,
	'F', 077,040,076,040,040,040,
	'G', 036,041,040,047,041,036,
	'H', 041,041,077,041,041,041,
	'I', 004,004,004,004,004,004,
	'J', 001,001,001,001,041,036,
	'K', 041,042,074,044,042,041,
	'L', 040,040,040,040,040,077,
	'M', 041,063,055,041,041,041,
	'N', 041,061,051,045,043,041,
	'O', 036,041,041,041,041,036,
	'P', 076,041,041,076,040,040,
	'Q', 036,041,041,045,042,035,
	'R', 076,041,041,076,042,041,
	'S', 036,040,036,001,041,036,
	'T', 037,004,004,004,004,004,
	'U', 041,041,041,041,041,036,
	'V', 041,041,041,041,022,014,
	'W', 041,041,041,055,063,041,
	'X', 041,022,014,014,022,041,
	'Y', 021,012,004,004,004,004,
	'Z', 077,002,004,010,020,077,
	' ', 000,000,000,000,000,000,
	'.', 000,000,000,000,014,014,
	',', 000,000,014,014,004,010,
	':', 014,014,000,014,014,000,
	'-', 000,000,077,077,000,000,
	'+', 014,014,077,077,014,014,
	'@', 036,041,046,051,046,035,
	'[', 016,010,010,010,010,016,
	'\\', 040,020,010,004,002,001,
	'/', 001,002,004,010,020,040,
	']', 034,004,004,004,004,034,
	'^', 004,012,000,000,000,000,
	'_', 000,000,000,000,000,077,
	'!', 010,010,010,010,000,010,
	'"', 022,022,000,000,000,000,
	'#', 022,077,022,022,077,022,
	'$', 036,054,036,015,055,036,
	'%', 001,062,064,013,023,040,
	'&', 014,022,014,024,042,035,
	'\'', 010,010,000,000,000,000,
	'(', 004,010,010,010,010,004,
	')', 010,004,004,004,004,010,
	'*', 000,022,014,014,022,000,
	'|', 014,014,014,014,014,014,
	000, 000,000,000,000,000,000	/* default -- currently blank */
};
char blank ' ';
char plot 'X';
int msk 040;	/*mask at sixth bit*/

main(argc,argp)
char **argp;int argc;
{
	int i;

	/*if invoked with no arguments, prints error comment;
	  if invoked with an argument, prints it in banner form.
	*/

	if(argc<2){
	  printf("missing argument\n");
	  exit();
	}
	while(--argc) {
		banner(*++argp,&buffer);
		banprt(&buffer);
	}
}

banner(s,bufp)
char *s;char *bufp;
{
	register char c,*p,*q;

	p=s;
	banset(blank,bufp);

	while((c= *s++)!=0){
	  if((s-p)>chpln)return;
	  ctbl[sizeof ctbl-nlines-1] = c;	/* setup default */
	  for(q=ctbl;*q++!=c;q=+nlines);
	  banfil(q,bufp);
	  bufp=+pospch;
	}
}

banfil(c,p)
char *c;
struct bann *p;
{
	int i,j;
	for(i=0;i<nlines;i++){
	  for(j=0;j<pospch;j++){
	    if(((c[i]<<j)&msk)!=0)p->alpha[i][j] = plot;
	  }
	}
	return(0);
}

banset(c,p)
char c;
struct bann *p;
{
	int i,j;
	for(i=0;i<nlines;i++)
	  for(j=0;j<pposs-1;j++)
	    p->alpha[i][j] = c;
}

banprt(ptr)
struct bann *ptr;
{
	int i,j;
	for(i=0;i<nlines;i++){
	  ptr->alpha[i][pposs-1]='\0';
	  for(j=pposs-2;j>=0;j--){
	    if(ptr->alpha[i][j]!=blank)break;
	    ptr->alpha[i][j]='\0';
	  }
	printf("%s\n",ptr->alpha[i]);
	}
}
