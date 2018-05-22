#include "mh.h"
#include "stat.h"

char *anoyes[];         /* Std no/yes gans array        */
char defalt[];

int     fout;

struct msgs *mp;
struct dirent {
	 int    inum;
	 char   name[14];
	 int    pad;
};
struct swit switches[] {
	"help",         4,      /* 0 */
	0,              0
};

main(argc, argv)
char *argv[];
{
	register char *cp, *ap;
	char *folder, *inp, buf[128];
	int i, def_fold;
	struct inode stbf;
	struct dirent ent;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	folder = 0;
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
							/* -help */
			case 0: help(concat( inp, " [+folder] [switches]", 0), switches);
				goto leave;
			}
		if(*cp == '+')
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		else {
			printf(concat("Usage: ", inp, " [+folder]\n",0));
			goto leave;
		}
	}
	if(!folder) {
		folder = m_getfolder();
		def_fold++;
	}
	expfold(folder);
 leave:
	m_update();
	flush();
}


expfold(fold)
char *fold;
{
	char *maildir;
	int i, expcount, nxtmsg, compactok;
	register int msgnum;
	register char *cp, *sp;
	char nambuf[10];
	struct inode stbf;

	expcount = 0;
	maildir = m_maildir(fold);
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: "); flush();
		perror(maildir);
		return(1);
	}
	if(access(".", 2) == -1) {
		printf("You cannot manipulate %s\n", fold);
		return(1);
	}
	if(!(mp = m_gmsg(fold))) {
		printf("Can't read folder!\n");
		return(1);
	}
	if(mp->hghmsg == 0) {
		printf("No messages in \"%s\".\n", fold);
		return(1);
	}
	nxtmsg = 0;
	compactok = TRUE;
	for(msgnum = 1; msgnum<=mp->hghmsg; msgnum++)
		if(mp->msgstats[msgnum]&EXISTS) {
			cp = getcpy(m_name(msgnum));
			if (mp->msgstats[msgnum]&DELETED)
			    if(unlink(cp) == -1)
				printf("Can't unlink %s: %s\n", fold,cp);
    			    else {
				mp->msgstats[msgnum] =& ~EXISTS & ~DELETED;
				if(msgnum==mp->curmsg)
					m_setcur(0);
				expcount++;
				if (nxtmsg == 0) nxtmsg = msgnum;
			   }
			else if ((nxtmsg > 0) && compactok) {
				if(link(cp,sp=m_name(nxtmsg)) == -1) {
				   compactok = FALSE;
				   printf("Can't rename %s to %s.\n", cp, sp);
				}
				else if(unlink(cp) == -1) {
				   compactok = FALSE;
				   printf("Can't rename %s to %s.\n", cp, sp);
				}
				else {
				   if(msgnum==mp->curmsg)
					m_setcur(nxtmsg);
				   nxtmsg++;
				}
				}
			}
		else if(nxtmsg==0) nxtmsg = msgnum;
	printf("%d message%s gone\n", expcount, expcount==1 ? "":"s");
	return(0);
}
