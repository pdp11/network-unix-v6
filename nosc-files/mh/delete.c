/* formerly rmm */
#include "mh.h"

int vecp, *vec[MAXARGS], fout;
struct msgs *mp;
struct swit switches[] {
	"all",         -3,      /* 0 */
	"help",         4,      /* 1 */
	0,              0
};

main(argc, argv)
char *argv[];
{
	char *folder, *nfolder, *maildir, *msgs[100], buf[32];
	register int msgnum;
	register char *cp, *sp;
	int msgp;
	char *ap, *inp;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	folder = msgp = 0;
	vecp = 1;
	inp = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			inp = cp;
	ap = inp;
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
							 /* -all */
			case 0: printf("\"-all\" changed to \"all\"\n");
				goto leave;
							/* -help */
			case 1: help(concat( inp, " [+folder] [msgs] [switches]", 0),
				     switches);
				goto leave;
			}
		if(*cp == '+')  {
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		} else
			msgs[msgp++] = cp;
	}
	if(!msgp)
		msgs[msgp++] = "cur";
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
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert(msgs[msgnum], UNDELETED, UNDELETED))
			goto leave;
	if(mp->numsel == 0) {
		goto leave;
	}
	m_replace("folder", folder);
	if((cp = m_find("delete-prog")) == -1) {
		for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
			if(mp->msgstats[msgnum] & SELECTED) {
				sp = getcpy(m_name(msgnum));
				cp = copy(sp, buf);
				cp[1] = 0;
				do
					*cp = cp[-1];
				while(--cp >= buf && *cp != '/');
				*++cp = ',';            /* backup convention */
				unlink(buf);
				if(link(sp, buf) == -1 || unlink(sp) == -1)
					printf("Can't rename %s to %s.\n", sp, buf);
			}
	} else {
		if(mp->numsel > MAXARGS-2) {
  printf("More than %d messages for deletion-prog\n",MAXARGS-2);
			goto leave;
		}
		for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
			if(mp->msgstats[msgnum]&SELECTED)
				vec[vecp++] = getcpy(m_name(msgnum));
		vec[vecp] = 0;
		vec[0] = cp;
		m_update();
		flush();
		execvsrh(vec);
		printf("Can't exec deletion prog--"); flush();
		perror(cp);
	}
leave:
	m_update();
	flush();
}
