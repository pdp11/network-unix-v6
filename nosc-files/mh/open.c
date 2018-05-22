/* formerly folder */
#include "mh.h"
#include "stat.h"
#define NFOLDERS 100

char    lsproc[],
	listname[];

int fout, all, hdrflag, foldp;
struct msgs *mp;
char folder[128], *folds[NFOLDERS];
int msgtot, foldtot, totonly, shrt;
struct swit switches[] {
	"all",          0,      /* 0 */
	"down",         0,      /* 1 */
	"fast",         0,      /* 2 */
	"nofast",       0,      /* 3 */
	"header",       0,      /* 4 */
	"noheader",     0,      /* 5 */
	"short",        0,      /* 6 */
	"total",        0,      /* 7 */
	"nototal",      0,      /* 8 */
	"up",           0,      /* 9 */
	"help",         4,      /*10 */
	0,              0
};

main(argc, argv)
char *argv[];
{
	register char *cp, *curm;
	register int i;
	char *argfolder;
	int up, down, j, def_short;
	char *ap, *inp;
	char *arguments[50], **argp;
	struct inode stbf, *np;
	struct { int    inum;
		 char   name[14];
		 int    pad;
	} ent;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	up = down = argfolder = 0;
	curm = 0;
	if(argv[0][length(argv[0])-1] == 's')   /* Plural name?? */
		all++;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	inp = ap;
	if(!uleq("folder", ap) && (cp = m_find(ap)) != -1) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);     /* ambiguous */
				goto leave;
							   /* unknown */
			case -1:printf("-%s unknown\n", cp);
				goto leave;
			case 0: all++;  continue;          /* -all      */
			case 1: down++;  continue;         /* -down     */
			case 2:                            /* -fast     */
			case 6: shrt = 1;  continue;      /* -short    */
			case 3: shrt = 0;  continue;      /* -nofast   */
			case 4: hdrflag = -1;  continue;   /* -header   */
			case 5: hdrflag = 0;  continue;    /* -noheader */
			case 7: all++; totonly = 1;        /* -total    */
				continue;
			case 8: if(totonly) all--;         /* -nototal  */
				totonly =0;  continue;
			case 9: up++;  continue;           /* -up       */
							   /* -help     */
			case 10:help(concat( inp, " [+folder]  [msg] [switches]",0),
				     switches);
				goto leave;
			}
		if(*cp == '+') {
			if(argfolder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				argfolder = cp + 1;
		} else if(curm) {
			printf("Only one current may be given.\n");
			goto leave;
		} else
			curm = cp;
	}
	if(all) {
		hdrflag = 0;
		cp = m_maildir("");
		m_getdefs();
		for(np = m_defs; np; np = np->n_next) {
			if(!ssequal("cur-", np->n_name))
				continue;
			if(shrt) {
				def_short++;
				printf("%s\n", np->n_name+4);
			} else
				addfold(np->n_name+4);
		}
		if(def_short)
			putchar('\n');
		if(shrt) {
			m_update();
			flush();
			execl(lsproc, "mh-ls", "-x", cp, 0);
			printf("Can't exec: "); flush();
			perror(lsproc);
			goto leave;
		}
		if(chdir(cp) < 0) {
			printf("Can't chdir to: "); flush();
			perror(cp);
			goto leave;
		}
		if((cp = m_find("folder")) == -1)
			*folder = 0;
		else
			copy(cp, folder);
		i = open(".", 0);
		ent.pad = 0;
		while(read(i, &ent.inum, sizeof ent.name + sizeof ent.inum))
			if(ent.inum && ent.name[0] != '.' &&
			   stat(ent.name, &stbf) >= 0 &&
			   (stbf.i_mode&IFMT) == IFDIR)
				addfold(ent.name);
		close(i);
		for(i = 0; i < foldp; i++) {
			pfold(folds[i], 0); flush();
		}
		if(!totonly)
			printf("\n\t\t     ");
		printf("TOTAL= %3d message%c in %d Folder%s.\n",
			msgtot, msgtot!=1? 's':' ',
			foldtot, foldtot!=1? "s":"");
	} else  {
		hdrflag++;
		if(argfolder)
			cp = copy(argfolder, folder);
		else
			cp = copy(m_getfolder(), folder);
		if(up) {
			while(cp > folder && *cp != '/') --cp;
			if(cp > folder)
				*cp = 0;
			argfolder = folder;
		} else if(down) {
			copy(listname, copy("/", cp));
			argfolder = folder;
		}
		if(pfold(folder, curm) && argfolder)
			m_replace("folder",argfolder);
	}

 leave:
	m_update();
	flush();
}


addfold(fold)
char *fold;
{
	register int i,j;
	register char *cp;

	if(foldp >= NFOLDERS) {
		printf("More than %d folders!!\n", NFOLDERS);
		return(1);
	}
	cp = getcpy(fold);
	for(i = 0; i < foldp; i++)
		if(compare(cp, folds[i]) < 0) {
			for(j = foldp - 1; j >= i; j--)
				folds[j+1] = folds[j];
			foldp++;
			folds[i] = cp;
			return(0);
		}
	folds[foldp++] = cp;
	return(0);
}


pfold(fold, curm)
char *fold;
{
	register char *mailfile;

	mailfile = m_maildir(fold);
	if(chdir(mailfile) < 0) {
		printf("Can't chdir to: "); flush();
		perror(mailfile);
		return(0);
	}
	if(shrt) {
		printf("%s\n", fold);
		return(0);
	}
	mp = m_gmsg(fold);
	foldtot++;
	msgtot =+ mp->nummsg;
	if(totonly)
		goto out;
	if(curm) {
		if(!m_convert(curm))
			return(0);
		if(mp->numsel > 1) {
			printf("Can't set current msg to %s\n", curm);
			return(0);
		}
		m_setcur(mp->curmsg = mp->hghsel);
	}
	if(!hdrflag++)
  printf("\t\tFolder   # of messages   ( range ); cur msg (other files)\n");
	printf("%22s", fold);
	if(equal(folder, fold))
		printf("+ ");
	else
		printf("  ");
	if(mp->hghmsg == 0)
		printf("has  no messages");
	else {
		printf("has %3d message%s (%3d-%3d)",
			mp->nummsg, (mp->nummsg==1)?" ":"s",
			mp->lowmsg, mp->hghmsg);
		if(mp->curmsg >= mp->lowmsg && mp->curmsg <= mp->hghmsg)
			printf("; cur=%3s", m_name(mp->curmsg));
	}
	if(mp->selist || mp->others) {
		printf("; (");
		if(mp->selist) {
			printf("%s", listname);
			if(mp->others)
				printf(", ");
		}
		if(mp->others)
			printf("others");
		putchar(')');
	}
	putchar('.');
	putchar('\n');
out:
	free(mp);
	mp = 0;
	return(1);
}


compare(s1, s2)
char *s1, *s2;
{
	register char *c1, *c2;
	register int i;

	c1 = s1; c2 = s2;
	while(*c1 || *c2)
		if(i = *c1++ - *c2++)
			return(i);
	return(0);
}
