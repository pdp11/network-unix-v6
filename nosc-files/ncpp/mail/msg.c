#/*
Module Name:
	msg.c

Installation:
	if $1e = finale goto finale
	cc msg.c
	exit
: finale
	cc -O -s -n msg.c
	su cp a.out /bin/msg
	rm a.out

*/
#define MSGSEP "\001\001\001\001\n"
#define MSGSEPL 5
#define NMSGS 120
#define SIZEDATE 12    /* also have to change the printf */
#define SIZEFROM 21
#define SIZESUBJ 41
#define DELETED 0000001
#define LINESIZE 180
#define PLEN 23	/* page length for display commands */

char *newmsgfn "/.mail";
char *savmsgfn "/mbox";
char *helpfile "/etc/help/msg.help";
struct {long int start;
	int len;
	char flags;
	char date[SIZEDATE];
	char from[SIZEFROM];
	char subject[SIZESUBJ];
	} msg[NMSGS], *p;

struct
{	int hiword;
	int loword;
};

struct
{	char minor;
	char major;
	int inumber;
	int sflags;
	char nlinks;
	char uid;
	char gid;
	char size0;
	char size1;
	int addr[8];
	long int actime;
	long int modtime;
}	statb1, statb2; /* used for comparing times */

int pwr10[] {1,10,100,1000};
int ttyraw[3],ttynorm[3];
extern int errno;
int nottty;
long int curpos;
long int newpos;
int nmsgs;
int ndeleted;
int msgno;
char line[LINESIZE], templine[LINESIZE], *tlp;
char filename[60], tempname[60], outfile[60], mbox[60];
char maininbox[60];
char defoutfile[60];
char userid[20];
char flag1,flag2;
int doflg;
int msgbeg, msgend;
char nxtchar;
int needhead;
int gotnl;
int verbose;
int orig;
int curmsg;
int inmsg;
int wfile;
int lcount;
struct getcbuf
	{int fildes;
	int nleft;
	char *nextp;
	char buff[512];
	} filebuf;
/**/
onint() {
	signal(2, &onint);
	error("\n...interrupted\n");
}
/**/
error(s) char *s; {
	if (wfile > 0) close(wfile);
	if (verbose) stty(0,ttyraw);
	printf("%s", s);
	if (!(verbose || gotnl || nxtchar == '\n')) suckup();
	reset();
}
/**/
gline(file)
	struct getcbuf *file;
{	register char *cp;
	char *linend;
	linend = line + LINESIZE - 1;
	for(cp = line; cp < linend; cp++)
	{	if((*cp = getc(file)) == -1) return(-1);
		if(*cp == '\n') break;
	}
	if (*cp != '\n') *++cp = '\n';
	return(++cp-line);
}
/**/
prefix(s1, s2)
	char *s1, *s2;
{	while(*s2) if(*s1++ != *s2++) return(0);
	return(1);
}
/**/
copy(s1, s2) char *s1, *s2; {
	while(*s2++ = *s1++);
}
/**/
setup()
{	int count;
	int total;

	nmsgs = ndeleted = needhead = curmsg = inmsg = 0;
	curpos = 0;
	newpos = 0;
	if (filebuf.fildes > 0) close(filebuf.fildes);
	if(fopen(filename, &filebuf) < 0)
	{	printf("cannot open %s\n", filename);
		error("");
	}
	fstat(filebuf.fildes,&statb1);
	total = 0;
	while ((count=gline(&filebuf)) != -1)
	{	curpos = newpos;
		newpos =+ count;
		if (line[0] == MSGSEP[0])
			inmsg = 0;
		else   {if (!inmsg) begmsg();
			p->len =+ count;
			total =+ count;
			if (needhead) gethead();
			}
	}

	printf("%d message%c\n",nmsgs, nmsgs == 1 ? ' ' : 's');
}
/**/

nxtfld(s)
	char *s;
{
	while (*s && (*s++ != ' '));
	while(*s == ' ') s++;
	return(s);
}
/**/
nxtfld1()
{	char c;
	if (!verbose)
		while((c = getcc()) != '\n' && c != ' ');
	else    c=getcc();
	if (c == ' ')
		while((c = getcc()) == ' ');
	return(c);
}
/**/

prhdr()
{	if (verbose) stty(0,ttynorm);
	printf("%3d%c %-11s %-20s %-40s\n", msgno,
		p->flags&DELETED ? '*' : ' ',
		p-> date, p->from, p->subject);
	if (verbose) stty(0,ttyraw);
}
/**/
main(argc, argv)
	int argc;
	char *argv[];
{
	int restrt;
	extern prmsg(), delmsg(), undelmsg(), prhdr(), movmsg(),
	  putmsg(), lstmsg();
	orig = signal(2, &onint);
	if (gtty(0,ttynorm) < 0)
		nottty++;
	ttyraw[0] = ttynorm[0];
	ttyraw[2] = ttynorm[2] | 040;
	printf("MSG Version 3.3.NOSC\n");
	printf("	    Use DEL to abort output.\n");
	printf("	    For help type ?\n");
	printf("reading mailbox ...\n");
	getuser();
	restrt = 0;
	if (argc > 1)
	  copy(argv[1],filename);
	setexit();
	if (!(restrt++))
	   setup();
	if (argc != 0 && argv[0][0] != 'r' && !nottty)
	{	verbose++;
		stty(0,ttyraw);
	}
	setexit();
/**/
	for(;;) {
		printf("<- ");
		flag1 = flag2 = gotnl = 0;
		nxtchar = getcc();
	/*      if (nmsgs == 0 && !(nxtchar == '?' || nxtchar == 'q' ||
		   nxtchar == 'j' || nxtchar == 'v'|| nxtchar == 'r'))
			error("\nNo file\n");   */
		switch(nxtchar)
		{
		case '\n':
			break;
		case 'h':
			if (verbose) printf("eaders ");
			rditer(&prhdr);
			break;
		case 'j':
			if (verbose) printf("ump ");
			getfn("to program: ",templine,0);
			xeq();
			break;
		case '?':
			if (verbose) printf("\n");
			help();
			break;
		case 'c':
			if (verbose) printf("urrent ");
			printf("message is %d of %d messages in file %s\n",
			   curmsg,nmsgs,filename);
			break;
		case 'd':
			if (verbose) printf("elete ");
			rditer(&delmsg);
			break;
		case 'u':
			if (verbose) printf("ndelete ");
			rditer(&undelmsg);
			break;
		case 't':
			if (verbose) printf("ype ");
			lcount=0;
			rditer(&prmsg);
			break;
		case 'n':
			if (verbose) printf("ext message is:\n");
			else suckup();
			if (curmsg == nmsgs)
				error("There is no next message\n");
			p = &msg[curmsg];
			msgno = ++curmsg;
			lcount=0;
			prmsg();
			break;
		case 'b':
			if (verbose) printf("acking up to:\n");
			else suckup();
			if (curmsg <= 1)
				error("There is no previous message\n");
			msgno = --curmsg;
			p = &msg[curmsg - 1];
			lcount=0;
			prmsg();
			break;
		case 'm':
			if (verbose) printf("ove ");
			rditer1();
			getfn("to file: ",outfile,defoutfile);
			cpymsg(&movmsg);
			break;
		case 'p':
			if (verbose) printf("ut ");
			rditer1();
			getfn("to file: ",outfile,defoutfile);
			cpymsg(&putmsg);
			break;
		case 'l':
			if (verbose) printf("ist ");
			rditer1();
			getfn("to file: ",outfile,defoutfile);
			cpymsg(&lstmsg);
			break;
		case 'w':
			if (verbose) printf("rite defaults ");
			getfn("to file: ",defoutfile,mbox);
			break;
		case 's':
			if (verbose) printf("ave ");
			confirm();
			if (str_cmp(filename,mbox))
			   error("Same file - ignored\n");
			save();
			overwrit();
			copy(outfile,filename);
			setup();
			break;
		case 'o':
			if (verbose) printf("verwrite ");
			confirm();
			overwrit();
			setup();
			break;
		case 'r':
			if (verbose) printf("ead ");
			getfn("from file: ",filename,maininbox);
			setup();
			break;
		case 'e':
			if (verbose) printf("xit and update file ");
			confirm();
			if (mainbox())
			   save();
			overwrit();
			stty(0,ttynorm);
			exit();
		case '\177':  /* DEL */
			onint();
		case 'q':
			if (verbose) printf("uit\n");
			stty(0,ttynorm);
			exit();
		case 'v':
			if (verbose)
			{	printf("erbose off\n");
				stty(0,ttynorm);
				verbose = 0;
				gotnl++;
			}
			else
			{	suckup();
				if (nottty) error("Input not tty\n");
				printf("verbose on\n");
				verbose++;
				stty(0,ttyraw);
			}
			break;
		case 'f':
			if (verbose) printf("orward -- ");
			printf("Sorry, command not yet implemented\n");
			break;
		case 'a':
			if (verbose) printf("nswer -- ");
			printf("Sorry, command not yet implemented\n");
			break;
		case '':
			nxtchar = -1;
		default:
			if (nxtchar >= 0)
			printf("\nUnrecognized command.  For help type ?\n");
			else printf("Type quit to exit\n");
		}
		if (!(verbose || gotnl || nxtchar == '\n')) suckup();
	}
}
/**/
delmsg()
{	p->flags =| DELETED ;
	ndeleted++;
	return;
}
/**/
undelmsg()
{	p->flags =& ~DELETED;
	--ndeleted;
	return;
}
/**/

save()  /* move undeleted messages in file filename to mbox file */
{	extern movmsg();
	flag1 = 'u';
	copy(mbox,outfile);
	if ((wfile=open(outfile,1)) < 0)
	{	if (errno != 2)
		{	printf("Can't open %s",outfile);
			error("\n");
		}
		if ((wfile=creat(outfile,0600)) < 0)
		{	printf("Can't create %s",outfile);
			error("\n");
		}
	}
	else (seek(wfile,0,2));
	doiter(&movmsg,1,nmsgs);
	close(wfile);
	wfile = 0;
}
/**/

getfn(s,f,def)
	char *s,*f,*def;
{	char tmpbuf[100];
	char *cp;
	if (verbose) stty(0,ttynorm);
	printf("%s",s);
	read(0,tmpbuf,100);
	for (cp = tmpbuf; *cp != '\n'; cp++);
	*cp = 0;
	if (cp == tmpbuf)
	{	if (def == 0) error("Invalid name\n");
		else  { copy(def,f); printf("%s\n",f); }
	}
	else copy(tmpbuf,f);
	if (verbose) stty(0,ttyraw);
}
/**/
cpymsg(fn)
	int (*fn)();
{	if ((wfile=open(outfile,1)) < 0)
	{	if (errno != 2)
			error("can't open file\n");
		if ((wfile=creat(outfile,0644)) < 0)
			error("can't create file\n");
	}
	else seek(wfile,0,2);
	rditer2(fn);
	close(wfile);
	wfile = 0;
}
/**/
movmsg()
{	putmsg();
	delmsg();
}
/**/
putmsg()
{	write(wfile,MSGSEP,MSGSEPL);
	lstmsg();
	write(wfile,MSGSEP,MSGSEPL);
}
/**/
lstmsg()
{	int size;
	int count;
	int temp;
	char tmpbuf[512];

	temp = p->start >> 9;
	seek(filebuf.fildes,temp,3);
	seek(filebuf.fildes,(p->start).loword&0777,1);
	for (size = p->len; size; size =- count)
	{	count = size<512? size : 512;
		if (read(filebuf.fildes,tmpbuf,count) < 0)
			error("read error\n");
		if (write(wfile,tmpbuf,count) < 0)
			error("write error\n");
	}
}
/**/

prmsg()
{	int size;
	int llen;
	int count;
	printf("\n(Message %d%s, %d bytes)\n",msgno,
		p->flags&DELETED ? "*" : "",p->len);
	write(1,"**Hit DEL to abort output\n",26);
	lcount =+ 3;
	fseek(&filebuf,p->start);
	size = p->len;
	if (verbose) stty(0,ttynorm);
	for (count = 0; count < size; )
	{	while (lcount++ < PLEN)
		{	write(1,line,(llen=gline(&filebuf)));
			if ((count =+ llen) >= size) break;
		}
		if (count < size)
		{	printf("\007");
			if (verbose)
				write(1,"**Hit RETURN to continue: ",27);
			getcc();
			lcount=0;
		}
	}
	if (verbose) stty(0,ttyraw);
	curmsg = msgno;
}
/**/
overwrit()  /* remove deleted messages from file filename */
{	char tmpbuf[512];
	int size;
	int count;
	int j;

	if (ndeleted == 0)
	{	printf("File unchanged, so not updated\n");
		return;
	}

	if (stat(filename,&statb2) >= 0 && statb1.modtime != statb2.modtime)
		error("File has been updated since last read\n");

	if (ndeleted == nmsgs)
	{	if (mainbox())
			creat(filename,0600); /* truncate if mail file */
		else
		{	printf("Deleting file!\n");
			unlink(filename);	/* otherwise delete */
		}

		return;
	}



	if ((wfile=creat(tempname,0600)) < 0)
		error("can't create temporary file\n");
	flag1 = 'u';
	doiter(&putmsg,1,nmsgs);
	close(wfile);
	if (unlink(filename) < 0)
		error("can't delete old file\n");
	if (link(tempname,filename) < 0)
		error("can't rename temp file\n");
	unlink(tempname);
}
/**/

xeq()
{	int process, status;
	int old1, old2, old3;
	if (verbose)
		stty(0,ttynorm);
	old1 = signal(1, 1);
	old2 = signal(2, 1);
	old3 = signal(3, 1);

	process = fork();
	if(!process)
	{	signal(1, old1);
		signal(2, orig);
		signal(3, old3);
		execl("/bin/sh", "-", "-c", templine, 0);
		printf("cannot execute %s\n", templine);
	}
	else
	{	wait(&status);
		signal(1, old1);
		signal(2, old2);
		signal(3, old3);
	}
	if (verbose) stty(0,ttyraw);
}
/**/

help()
{	int helpfd;
	int i;
	char helpbuf[512];
	if ((helpfd=open(helpfile,0)) < 0)
		error("Can't open help file\n");
	if (verbose) stty(0,ttynorm);
	while ((i=read(helpfd,helpbuf,512)) > 0)
		write(1,helpbuf,i);
	close(helpfd);
	if (verbose) stty(0,ttyraw);
}
/**/

rditer(fn)
int (*fn)();
{	rditer1();
	if (nmsgs==0)  {
		printf("Mailbox is empty\n");
		return;
	}
	rditer2(fn);
}
/**/

rditer1()
{	int n1, n2;
	int begin, end;

	switch (nxtchar = nxtfld1())
	{
	case '\n':
	/*      if (curmsg == 0 || curmsg > nmsgs)
			error("No current message\n");  */
		msgbeg = msgend = curmsg;
		break;
	case 'a':
		if (verbose) printf("ll messages\n");
		msgbeg = 1;
		msgend = nmsgs;
		break;
	case 'i':
		if (verbose) printf("nverse all messages\n");
		msgbeg = nmsgs;
		msgend = 1;
		break;
	case 'd':
	case 'u':
		flag1 = nxtchar;	 /* save 'u' or 'd' */
		if (verbose) printf(nxtchar == 'd' ?
			     "eleted%s" : "ndeleted%s"," messages\n");
		msgbeg = 1;
		msgend = nmsgs;
		break;
	default:
		flag2++;
		rditer2(0); /* indicate only checking */
	}
}
/**/
rditer2(fn)
int (*fn)();

{	int n1, n2, t;
	int begin, end;

	doflg = fn;
	tlp = templine;
	if (doflg) nxtchar = *tlp++;
	else *tlp++ = nxtchar;
	if (flag2)	    /* only do this if a msg-sequence is present */
	while (nxtchar != '\n')
	{	while (nxtchar == ' ' || nxtchar == ',')
			if (doflg) nxtchar = *tlp++;
			else *tlp++ = nxtchar = getcc();
		if (nxtchar == '\n') break;
		n1 = getnum(&begin);
		end = begin;
		switch(nxtchar)
		{
		case '-':
			if (n1 == 0)
			   error("'-' must be preceeded by number\n");
			if (begin == 0 || begin > nmsgs)
			   error(" Invalid starting number\n");
			if (doflg) nxtchar = *tlp++;
			else *tlp++ = nxtchar = getcc();
			n2 = getnum(&end);
			if (n2 == 0)
			   error("'-' must be followed by number\n");
			if (end > nmsgs)
			   error(" Invalid ending number\n");
			if (n1 > n2) /* here comes the class! */
			{	t = pwr10[n2];
				end =+ begin / t * t;
			}
			if (begin > end)
			   error("Invalid order for '-'\n");
			break;
		case '\n':
		case ' ':
		case ',':
			if (begin == 0 || begin > nmsgs)
			   error("Invalid number\n");
			break;
		case ':':
		case '>':
			if (n1 == 0) begin = 1;
			if (begin == 0 || begin > nmsgs)
			   error(" Invalid starting number\n");
			if (doflg) nxtchar = *tlp++;
			else *tlp++ = nxtchar = getcc();
			n2 = getnum(&end);
			if (n2 == 0) end = nmsgs;
			if (end == 0 || end > nmsgs)
			   error(" Invalid ending number\n");
			break;
		default:
			error(" Invalid number\n");
		}

		if (doflg) doiter(fn,begin,end);
	}
	else if (doflg)	                    /* no msg-sequence */
		doiter(fn,msgbeg,msgend);

}
/**/

getnum(result)
int *result;
{	register int i;
	register int n;

	i = n = 0;
	while (nxtchar >= '0' && nxtchar <= '9')
	{	n = n*10 + nxtchar - '0';
		i++;
		if (doflg) nxtchar = *tlp++;
		else *tlp++ = nxtchar = getcc();
	}
	*result = n;
	return(i);
}
/**/

doiter(fn,begin,end)
  int begin,end;
  int (*fn)();

{	if (nmsgs==0) return;
	if (begin <= end)
		for(msgno = begin; msgno <= end; msgno++)
		{	p = &msg[msgno-1];
			if (flag1 == 0 || (flag1 == 'd' && (p->flags&DELETED))
				|| (flag1 == 'u' && !(p->flags&DELETED)) )
			   (*fn)();
		}
	else
		for(msgno = begin; msgno >= end; --msgno)
		{	p = &msg[msgno-1];
			if (flag1 == 0 || (flag1 == 'd' && (p->flags&DELETED))
				|| (flag1 == 'u' && !(p->flags&DELETED)) )
			   (*fn)();
		}
}
/**/
str_cpy(s1,s2)
char *s1, *s2;
{	register char *p1, *p2;

	p1 = s1;
	p2 = s2;
	while(*p2++ = *p1++);
	return(p2-s2-1);
}
/**/
str_cmp(s1,s2)
char *s1, *s2;
{	while (*s1++ == *s2++)
		if (!(*s1 | *s2)) return(1);
	return(0);
}
/**/

begmsg()
{	if (nmsgs == NMSGS)
	{	printf("Too many messages!\n");
		 reset();
	}
	p = &msg[nmsgs++];
	p->from[0] = p->date[0] = p->subject[0] = p->flags = 0;
	p->start = curpos;
	p->len = 0;
	needhead = 1;
	inmsg++;
}
/**/

copy1(src,dst,maxlen)
char *src, *dst;
int maxlen;
{	register char *s, *d;
	register int count;
	s = src;
	d = dst;
	for (count = 0; count < maxlen; count++)
		if ( (*d++ = *s++) == '\n') break;
	*--d = 0;
}
/**/
gethead()
{	char line1[LINESIZE];
	if (line[0] == '\n' || line[0] == ' ') 
	{
		needhead = 0;
		return;
	}

	copy1(line,line1,LINESIZE);
	cnvlc(line1);

	if (prefix(line1,"date:"))
	{	copy1(line+6,p->date,SIZEDATE);
		return;
	}

	if (prefix(line1,"sender:"))
	{	copy1(line+8,p->from,SIZEFROM);
		return;
	}

	if (prefix(line1,"from:"))
	{	copy1(line+6,p->from,SIZEFROM);
		return;
	}

	if (prefix(line1,"subject:"))
	{	copy1(line+9,p->subject,SIZESUBJ);
		return;
	}

	if (prefix(line1,"to:"))
		if (prefix(p->from,userid))
			copy1(line,p->from,SIZEFROM);

}
/**/
cnvlc(linep)
	char *linep;
{	register char *s;
	for (s = linep; *s; s++)
		if (*s >= 'A' && *s <= 'Z') *s =| ' ';
}
/**/
fseek(bufptr,loc)
struct getcbuf *bufptr;
long int loc;
{	int temp;
	temp = loc >> 9;
	seek(bufptr->fildes,temp,3);
	seek(bufptr->fildes,loc.loword&0777,1);
	bufptr->nleft = 0;
	bufptr->nextp = 0;
}
/**/
getuser()
{	register char *s, *d;
	int i;
	char pwbuf[120];

	if (getpw(getuid(),pwbuf))
	{	printf("Can't locate user name\n");
		exit();
	}

	s = pwbuf;
	d = userid;
	while ((*d = *s++) != ':') d++; /* get user name */
	*d++ = ' ';	              /* pad with blank */
	*d = 0;

	for (i=4; i; --i)
		while(*s++ != ':');
	d = filename;
	while((*d = *s++) != ':')d++;
	*d = 0;
	copy(filename,tempname);
	copy(filename,mbox);
	copy(newmsgfn,d);
	copy(filename,maininbox);
	copy(savmsgfn,mbox+d-filename);
	copy(mbox,defoutfile);
	copy("/tmpmail",tempname+d-filename);
}
/**/
confirm()
{	char c;
	printf("[confirm] ");
	if ((c = getcc()) == 'y' && verbose) printf("es\n");
	if (!verbose) suckup();	           /* suck up rest of line */
	if (c != 'y' && c != '\n')
		error("command ignored\n");
}
/**/
suckup()
{
	while((nxtchar=getcc()) != '\n');
}
/**/
mainbox()	        /* return non-0 if last part of filename indicates */
{	char *cp;       /* it is the main mailbox                          */

	cp = filename;
	while(*cp++);   /* get to end of name */
	while(--cp != filename && *cp != '/'); /* find last part */
	if (*cp == '/') cp++;
	return (str_cmp(cp,newmsgfn+1));
}
/**/

getcc()
{
	static c;

	read(0, &c, 1);
	return(c);
}
