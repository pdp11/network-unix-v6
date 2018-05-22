#include "mh.h"
#include "stat.h"

char *anoyes[];         /* Std no/yes gans array        */
char defalt[];

int     fout;
int     subf;

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
	char *folder, buf[128];
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
			case -1:printf("rmf: -%s unknown\n", cp);
				goto leave;
							/* -help */
			case 0: help("rmf [+folder]  [switches]", switches);
				goto leave;
			}
		if(*cp == '+')
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		else {
			printf("Usage: rmf [+folder]\n");
			goto leave;
		}
	}
	if(!folder) {
		folder = m_getfolder();
		def_fold++;
	}
	subf = !((!any('/', folder)) | (*folder == '/') | (*folder == '.'));
	if(def_fold && !subf) {
		cp = concat("Remove folder \"", folder, "\" ?? ", 0);
		if(!gans(cp, anoyes))
			goto leave;
		free(cp);
	}
	if(rmfold(folder))
		goto leave;
	if(subf) {
		cp = copy(folder, buf);
		while(cp > buf && *cp != '/') --cp;
		if(cp > buf) {
			*cp = 0;
			printf("[+%s now current]\n", buf);
			m_replace("folder", buf);
		}
	}
 leave:
	m_update();
	flush();
}

struct dirent ent;

rmfold(fold)
char *fold;
{
	register char *maildir;
	int i, leftover;
	register char *cp, *sp;
	char nambuf[10];
	struct inode stbf;

	leftover = 0;
	if(!subf && equal(m_find("folder"), fold)) {  /* don't re-do */
		printf("[+%s now current]\n", defalt);
		flush();                                 /*??*/
		m_replace("folder", defalt);
	}
	maildir = m_maildir(fold);
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: "); flush();
		perror(maildir);
		return(1);
	}
	if(access(".", 2) == -1) {
		if(!m_delete(concat("cur-", fold, 0)))
			printf("[Folder %s de-referenced]\n", fold);
		else
 printf("You have no profile entry for the read-only folder %s\n", fold);
		return(1);
	}
	i = open(".", 0);
	ent.pad = 0;
	while(read(i, &ent.inum, sizeof ent.name + sizeof ent.inum))
		if(ent.inum)
		    if((ent.name[0] >= '0' && ent.name[0] <= '9') ||
		       ent.name[0] == ','     ||
		       equal(ent.name, "cur") ||
		       equal(ent.name, "@")) {
			    if(unlink(ent.name) == -1) {
				printf("Can't unlink %s:%s\n", fold,ent.name);
				leftover++;
			    }
		    } else if(!equal(ent.name,".")&& !equal(ent.name,"..")) {
			printf("File \"%s/%s\" not deleted!\n",
				fold, ent.name);
			leftover++;
		    }
	close(i);
	if(!leftover && removedir(maildir))
		return(0);
	else
		printf("Folder %s not removed!\n", fold);
	return(1);
}


removedir(dir)
{
	register int i, j;
	int status;

	if((i = fork()) == 0) {
		m_update();
		flush();
		execl("/bin/rmdir", "rmdir", dir, 0);
		printf("Can't exec rmdir!!?\n");
		flush();
		return(0);
	}
	if(i == -1) {
		printf("Can't fork\n");
		flush();
		return(0);
	}
	while((j = waita(&status)) != i && j != -1) ;
	if(status) {
		printf("Bad exit status (%o) from rmdir.\n", status);
		flush();
	    /*  return(0);   */
	}
	return(1);
}
