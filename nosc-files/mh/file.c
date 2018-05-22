#include "mh.h"
#include "stat.h"
#include "errors.h"
#define NFOLD 20                /* Allow 20 folder specs */

/* file [-src folder] [msgs] +folder [+folder ...]
 *
 * moves messages from src folder (or current) to other one(s).
 *
 *  all = first-last for a message sequence
 *  -preserve says preserve msg numbers
 *  -link says don't delete old msg
 */

char *anoyes[];         /* Std no/yes gans array        */

int fout, vecp, foldp, prsrvf;
char *vec[MAXARGS], maildir[128], *folder;
struct msgs *mp;

struct st_fold {
	char *f_name;
	struct msgs *f_mp;
} folders[NFOLD];

struct swit switches[] {
	"all",           -3,      /* 0 */
	"link",           0,      /* 1 */
	"nolink",         0,      /* 2 */
	"preserve",       0,      /* 3 */
	"nopreserve",     0,      /* 4 */
	"src +folder",    0,      /* 5 */
	"help",           4,      /* 6 */
	0,                0
};
main(argc, argv)
char *argv[];
{
	register int i, msgnum;
	register char *cp;
	char *msgs[128];
	int msgp, linkf;
	char *ap;
	char *arguments[50], **argp;

	fout = dup(1);
	if(argc < 2) {
 badarg:    printf("Usage: file [-src folder] [msg ...] +folder [+folder]\n");
	    goto leave;
	}
#ifdef NEWS
	m_news();
#endif
	folder = msgp = linkf = 0;
	vecp = 1;
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
			case -1:printf("file: -%s unknown\n", cp);
				goto leave;
						       /* -all */
			case 0: printf("\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: linkf = 1;  continue;  /* -link */
			case 2: linkf = 0;  continue;  /* -nolink */
			case 3: prsrvf = 1;  continue; /* -preserve */
			case 4: prsrvf = 0;  continue; /* -nopreserve */
			case 5: if(folder) {           /* -src */
					printf("Only one src folder.\n");
					goto leave;
				}
				if(!(folder = *argp++) || *folder == '-') {
		printf("file: Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				if(*folder == '+')
					folder++;
				continue;
							/* -help */
			case 6: help("file   [msgs] [switches]  +folder ...",
				     switches);
				goto leave;
			}
		if(*cp == '+')  {
			if(foldp < NFOLD)
				folders[foldp++].f_name = cp + 1;
			else {
				printf("Only %d folders allowed.\n", NFOLD);
				goto leave;
			}
		} else
			msgs[msgp++] = cp;
	}
	if(!foldp) {
		printf("No folder specified.\n");
		goto badarg;
	}
	if(!msgp)
		msgs[msgp++] = "cur";
	if(!folder)
		folder = m_getfolder();
	copy(m_maildir(folder), maildir);
	if(chdir(maildir) < 0) {
		printf("Can't chdir to: "); flush();
		perror(maildir);
		goto leave;
	}
	if(!(mp = m_gmsg(folder))) {
		printf("Can't read folder %s!?\n",folder);
		goto leave;
	}
	if(mp->hghmsg == 0) {
		printf("No messages in \"%s\".\n", folder);
		goto leave;
	}
	for(msgnum = 0; msgnum < msgp; msgnum++)
		if(!m_convert((cp = msgs[msgnum]), UNDELETED, UNDELETED))
			goto leave;
	if(mp->numsel == 0) {
		printf("No undeleted messages specified\n");       
		goto leave;
	}
	m_replace("folder", folder);
	if(mp->hghsel != mp->curmsg && ((mp->numsel != mp->nummsg) || linkf))
		m_setcur(mp->hghsel);
	if(opnfolds())
		goto leave;
	for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum] & SELECTED)
			if(process(getcpy(m_name(msgnum))))
				goto leave;
	if(!linkf) {
		if((cp = m_find("delete-prog")) != -1) {
			if(mp->numsel > MAXARGS-2) {
	  printf("file: more than %d messages for deletion-prog\n",MAXARGS-2);
				printf("[messages not unlinked]\n");
				goto leave;
			}
			for(msgnum= mp->lowsel; msgnum<= mp->hghsel; msgnum++)
				if(mp->msgstats[msgnum]&SELECTED)
					vec[vecp++] = getcpy(m_name(msgnum));
			vec[vecp] = 0;
			m_update();
			flush();
			vec[0] = cp;
			execvsrh(vec);
			printf("Can't exec deletion-prog--"); flush();
			perror(cp);
		} else {
			for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
				if(mp->msgstats[msgnum] & SELECTED)
					if(unlink(cp = getcpy(m_name(msgnum)))== -1) {
						printf("Can't unlink %s:",folder);
						flush(); perror(cp);
					}
		}
	}
leave:
	m_update();
	flush();
}


opnfolds()
{
	register int i;
	register char *cp;
	char nmaildir[128];
	struct inode stbuf;

	for(i = 0; i < foldp; i++) {
		copy(m_maildir(folders[i].f_name), nmaildir);
		if(stat(nmaildir, &stbuf) < 0) {
			cp = concat("Create folder \"", nmaildir, "\"? ", 0);
			if(!gans(cp, anoyes))
				goto bad;
			free(cp);
			if(!makedir(nmaildir)) {
				printf("Can't create folder.\n");
				goto bad;
			}
		}
		if(chdir(nmaildir) < 0) {
			printf("Can't chdir to: "); flush();
			perror(nmaildir);
			goto bad;
		}
		if(!(folders[i].f_mp = m_gmsg())) {
			printf("Can't read folder %s\n", folders[i].f_name);
			goto bad;
		}
	}
	chdir(maildir);         /* return to src folder */
	return(0);
bad:
	return(1);
}


process(msg)
{
	char newmsg[256], buf[512];
	register int i;
	register char *nmsg;
	register struct st_fold *fp;
	struct inode stbuf, stbf1;
	int n, o;

    for(fp = folders; fp < &folders[foldp]; fp++) {
	if(prsrvf)
		nmsg = msg;
	else
		nmsg = m_name(fp->f_mp->hghmsg++ + 1);
	copy(nmsg, copy("/", copy(m_maildir(fp->f_name), newmsg)));
	if(link(msg, newmsg) < 0) {
		if(errno == EEXIST ||
		  (errno == EXDEV && stat(newmsg, &stbuf) != -1)) {
			if(errno != EEXIST || stat(msg, &stbf1) < 0 ||
			   stat(newmsg, &stbuf) < 0 ||
			   stbf1.i_number != stbuf.i_number) {
				printf("Message %s:%s already exists.\n",
				     fp->f_name, msg);
				return(1);
			}
			continue;
		}
		if(errno == EXDEV) {
			if((o = open(msg, 0)) == -1) {
				printf("Can't open %s:%s.\n",
					folder, msg);
				return(1);
			}
			fstat(o, &stbuf);
			if((n = creat(newmsg, stbuf.i_mode&0777)) == -1) {
				printf("Can't create %s:%s.\n",
					fp->f_name, nmsg);
				close(o);
				return(1);
			}
			do
				if((i=read(o, buf, sizeof buf)) < 0 ||
				  write(n, buf, i) == -1) {
				    printf("Copy error on %s:%s to %s:%s!\n",
					    folder, msg, fp->f_name, nmsg);
				    close(o); close(n);
				    return(1);
				}
			while(i == sizeof buf);
			close(n); close(o);
		} else {
			printf("Error on link %s:%s to %s:",
			    folder, msg, fp->f_name);
			flush();
			perror(nmsg);
			return(1);
		}
	}
cont:   ;
    }
    return(0);
}
