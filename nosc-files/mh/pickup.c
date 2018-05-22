#include "mh.h"
#include "iobuf.h"
#include "stat.h"
#include "errors.h"

char   *anoyes[],       /* Std no/yes gans array        */
       *mypath,
	mailbox[],
	defalt[];

char    scanl[];
struct  msgs *mp;
struct  iobuf in, fout, *aud;
struct  inode stbuf;

struct  swit switches[] {
	"audit audit-file",     0,      /* 0 */
	"ms ms-folder",         0,      /* 1 */
	"help",                 4,      /* 2 */
	0,                      0
};

main(argc, argv)
char *argv[];
{

	char newmail[128], maildir[128], *folder, *from, *audfile;
	register char *cp;
	register int i, msgnum;
	long now;
	char *ap, *inp;
	char *arguments[50], **argp;

	fout.b_fildes = dup(1);
#ifdef NEWS
	m_news();
#endif
	from = folder = audfile = 0;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	inp = ap;
	if((cp = m_find(ap)) != -1) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							     /* unknown */
			case -1:printf("-%s unknown\n", cp);
				goto leave;
			case 0: if(!(audfile = *argp++)) {   /* -audit */
      missing:  printf("Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 1: if(!(from = *argp++))        /* -ms */
					goto missing;
				continue;
			case 2:                              /* -help */
				help(concat( inp, " [+folder]  [switches]", 0), switches);
				goto leave;
			}
		if(*cp == '+') {
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		} else {
			printf("Bad arg: %s\n", argp[-1]);
	printf(concat("Usage: ", inp, " [+folder] [-ms ms-folder] [-audit audit-file]\n", 0));
			goto leave;
		}
	}
	if(from)
		copy(from, newmail);
	else {
		mypath = getpath(getruid());
		copy(mailbox, copy(mypath, newmail));
		if(stat(newmail, &stbuf) < 0 ||
		   !(stbuf.i_size0 | stbuf.i_size1)) {
			printf("No new mail.\n");
			goto leave;
		}
	}
	if(fopen(newmail, &in) < 0) {
		printf("Can't read \"%s\"??\n", newmail);
		goto leave;
	}
	if(!folder) {
		folder = defalt;
		if(from && !equal(from, "inbox")) {
			cp = concat("Do you really want to convert from ",
				from, " into ", folder, "?? ", 0);
			if(!gans(cp, anoyes))
				goto leave;
		}
	}
	copy(m_maildir(folder), maildir);
	if(stat(maildir, &stbuf) < 0) {
		if(errno != ENOENT) {
			printf("Error on folder "); flush();
			perror(maildir);
			goto leave;
		}
/*		cp = concat("Create folder \"", maildir, "\"? ", 0);
		if(!gans(cp, anoyes))
			goto leave;     */
		if(!makedir(maildir)) {
			printf("Can't create folder \"%s\"\n", maildir);
			goto leave;
		}
	}
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: ");
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		printf("Can't read folder!?\n");
		goto leave;
	}
	if(audfile) {
		aud = alloc(sizeof *aud);
		cp = m_maildir(audfile);
		if(fopen2(cp, aud) == -1) {
			printf("Creating Receive-Audit: %s\n", cp);
			if(fcreat(cp, aud) == -1) {
				printf("Can't create "); flush();
				perror(cp);
				goto leave;
			}
			chmod(cp, 0600);
			putchar('\n');
		} else
			seek(aud->b_fildes, 0, 2);
		time(&now);
		puts("<<inc>> ", aud);
		cp = cdate(&now);
		cp[9] = ' ';
		puts(cp, aud);
		if(from) {
			puts("  -ms ", aud);
			puts(from, aud);
		}
		putc('\n', aud);
	}
	printf("Incorporating new mail into %s...\n\n", folder);
	flush();
	msgnum = mp->hghmsg;

	while((i = scan(&in, msgnum+1, msgnum+1, msgnum == mp->hghmsg))) {
		if(i == -1) {
			printf("command aborted!\n");
			if(aud)
				puts("inc aborted!\n", aud);
			goto leave;
		}
		if(aud)
			puts(scanl, aud);
		flush();
		msgnum++;
	}

	close(in.b_fildes);
	if(aud)
		fflush(aud);

	if(!from) {
		if((i = creat(newmail, 0600)) >= 0)     /* Zap .mail file */
			close(i);
		else
			printf("Error zeroing %s\n", newmail);
	} else
		printf("%s not zero'd\n", newmail);

	i = msgnum - mp->hghmsg;
   /*   printf("%d new message%s\n", i, i==1? "":"s");          */
	if(!i)
		printf("[No messages incorporated.]\n");
	else {
		if(!equal(m_getfolder(), folder))
			m_replace("folder", folder);
		m_setcur(mp->hghmsg + 1);
	}
leave:
	m_update();
	flush();
}




