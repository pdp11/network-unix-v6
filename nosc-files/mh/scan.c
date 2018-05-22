#include "mh.h"
#include "iobuf.h"

int hdrflag 1;
struct msgs *mp;
struct iobuf fout;
struct swit switches[] {
	"all",         -3,      /* 0 */
	"ff",           0,      /* 1 */
	"noff",         0,      /* 2 */
	"header",       0,      /* 3 */
	"noheader",     0,      /* 4 */
	"help",         4,      /* 5 */
	0,              0
};

main(argc, argv)
char *argv[];
{
	char *inp, *folder, *maildir, *msgs[100];
	register int msgnum;
	register char *cp, *ap;
	int msgp, ff;
	struct iobuf in;
	long now;
	char *arguments[50], **argp;

	fout.b_fildes = dup(1);
#ifdef NEWS
	m_news();
#endif
	ff = msgp = folder = 0;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	inp = ap;
	if((cp = m_find(ap)) != -1) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	  /*    printf("1:\n");    flush();                 ##*/
	  /*    pr_array("arguments", arguments); flush();  ##*/
	} else
		ap = arguments;
	copyip(argv+1, ap);
  /*    printf("2:\n");  flush();                   ##*/
  /*    pr_array("arguments", arguments); flush();  ##*/
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							/* unknown */
			case -1:printf("-%s unknown\n", cp);
				goto leave;
							 /* -all */
			case 0: printf("\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: ff = 1;  continue;      /* -ff */
			case 2: ff = 0;  continue;      /* -noff */
			case 3: hdrflag = 0;  continue; /* -header */
			case 4: hdrflag = 1;  continue; /* -noheader */
			case 5: help(concat( inp, " [+folder] [msgs] [switches]", 0),
				     switches);
				goto leave;
			}
		if(*cp == '+') {
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		} else
			msgs[msgp++] = cp;
	}
	if(!folder)
		folder = m_getfolder();
	maildir = m_maildir(folder);
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: "); flush();
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		printf("Can't read folder!?\n");
		goto leave;
	}
	if(mp->hghmsg == 0) {
		printf("No messages in \"%s\".\n", folder);
		goto leave;
	}
	if(!msgp)
		msgs[msgp++] = "first-last";
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert(msgs[msgnum], EXISTS, EXISTS))
			goto leave;
	m_replace("folder",folder);
	for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)  {
		if(mp->msgstats[msgnum]&SELECTED) {
			if(fopen(cp = m_name(msgnum), &in) < 0)
				printf("--Can't open %s\n", cp);
			else {
				if(!hdrflag++) {
					time(&now);
					cp = cdate(&now);
					cp[9] = ' '; cp[15] = 0;
printf("\
		       Folder %-32s%s\n\n\
  #   Date    From             Subject       [<<Body]\n\n", folder, cp);
				}
				scan(&in, msgnum, 0, msgnum == mp->curmsg);
				close(in.b_fildes);
				if(fout.b_nextp + 80 >= (&fout) + 1)
			/*      if(msgnum & 1)  */
					flush();
			}
		}
	}
	if(ff)
		putchar('\014');
leave:
	m_update();
	flush();
}

