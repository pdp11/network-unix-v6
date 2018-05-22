#include "mh.h"
#include "signals.h"

char    draft[],        /* Std strings  */
	showproc[],
	prproc[];

int vecp, *vec[MAXARGS], fout;
struct msgs *mp;
struct swit switches[] {
	"all",         -3,      /* 0 */
	"draft",        2,      /* 1 */
	"pr",           2,      /* 2 */
	"nopr",         2,      /* 3 */
	"help",         4,      /* 4 */
	0,              0
};

main(argc, argv)
char *argv[];
{
	char *inp, *folder, *maildir, *msgs[100], *proc;
	register int msgnum;
	register char *cp, *ap;
	int msgp, all, drft, pr, infrk, status, frk;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	pr = all = msgp = folder = 0;
	vecp = 1;
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
			case -1:vec[vecp++] = --cp;  continue;
							     /* -all   */
			case 0: printf("\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: drft = 1;  continue;         /* -draft */
			case 2: pr = 1;  continue;           /* -pr    */
			case 3: pr = 0;  vecp = 1;  continue;/* -nopr  */
			case 4:                              /* -help  */
  help(concat( inp, " [+folder] [msgs] [switches] [switches for l or pr (see /doc) ]", 0),
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
	if (!pr)  {
		vec[vecp++] = "-t"; /* no header */
	}
	if(drft)
		maildir = m_maildir("");
	else {
		if(!msgp)
			msgs[msgp++] = "cur";
		if(!folder)
			folder = m_getfolder();
		maildir = m_maildir(folder);
	}
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: "); flush();
		perror(maildir);
		goto leave;
	}
	if(!pr) {
		vec[0] = "mh-type";
		proc = showproc;
	} else {
		vec[0] = "mh-pr";
		proc = prproc;
	}
	if(drft) {
		vec[vecp++] = draft;
		vec[vecp] = 0;
		m_update();
		flush();
		execv( proc, vec);
		perror("Can't exec type");
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
	if(msgp)
		for(msgnum = 0; msgnum < msgp; msgnum++)
			if(!m_convert(msgs[msgnum], UNDELETED, UNDELETED))
				goto leave;
	if(mp->numsel == 0) {
		printf("No undeleted messages specified.\n");     
		goto leave;
	}
	vec[vecp+1]=0;
	for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum]&SELECTED) {
			vec[vecp] = getcpy(m_name(msgnum));
			printf("\nMessage %s\n", m_name(msgnum));
			flush();
			if((infrk = fork())==0) { /* inferior */
				execv( proc, vec);
				exit(1); /* execute didnt work */
			} else if(infrk== -1) {
				printf("No forks\n");
				break;
			} else
			   while((frk=waita(&status)) != -1 && frk != infrk);
			if(status) {
				printf("[command aborted]\n");
				break;
			}
		}
	m_replace("folder", folder);
	if(mp->hghsel != mp->curmsg)
		m_setcur(mp->hghsel);
 leave:
	m_update();
	flush();
}
