#include "mh.h"
#include "iobuf.h"
#include "stat.h"
#include "signals.h"

/*#define TEST 1*/

char    draft[],
	sysed[],
#ifdef PROMPT
	prmtproc[],
#endif
	sndproc[];

char    *anysh[] {
	"no",   0,
	"yes",  0,
	"show", 0,
	0
};
char    *anyv[] {
	"no",           0,
	"yes",          0,
	"verbose",      0,
	0
};

int  *vec[MAXARGS], fout, anot;
int ccme 1;
struct msgs *mp;
char *ed;

struct swit switches[] {
	"annotate",           0,      /* 0 */
	"noannotate",         0,      /* 1 */
	"ccme",               0,      /* 2 */
	"noccme",             0,      /* 3 */
	"editor editor",      0,      /* 4 */
	"help",               4,      /* 5 */
	0,                    0
};

main(argc, argv)
char *argv[];
{
	char *inp, *folder, *nfolder, *msg, *maildir;
	register char *cp, *ap;
	register int cur;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	msg = anot = folder = 0;

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
			case 0: anot = 1;  continue;         /* -annotate */
			case 1: anot = 0;  continue;         /* -noannotate */
			case 2: ccme = 1;  continue;         /* -ccme */
			case 3: ccme = 0;  continue;         /* -noccme */
			case 4: if(!(ed = *argp++)) {        /* -editor */
		printf("Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
							     /* -help */
			case 5: help(concat( inp, " [+folder] [msg] [switches]", 0),
				     switches);
				goto leave;
			}
		if(*cp == '+') {
			if(folder) {
				printf("Only one folder at a time.\n");
				goto leave;
			} else
				folder = cp + 1;
		} else if(msg) {
			printf("Only one message at a time.\n");
			goto leave;
		} else
			msg = cp;
	}
	if(!msg)
		msg = "cur";
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
	if(!m_convert(msg, UNDELETED, UNDELETED))
		goto leave;
	if(mp->numsel == 0) {
		printf("No undeleted message specified\n");
		goto leave;
	}
	if(mp->numsel > 1) {
		 printf("Only one message at a time.\n");
		 goto leave;
	}
	m_replace("folder", folder);
	if(mp->lowsel != mp->curmsg)
		m_setcur(mp->lowsel);
	repl(getcpy(m_name(mp->lowsel)));
 leave:
	m_update();
	flush();
}


repl(msg)
{
	register char *cp;
	register int i,j;
	struct iobuf in;
	char name[NAMESZ], field[512], *from, *cc, *sub, *date, *to;
	char *drft, *msgid;
	int state, out, status, intr;
	struct inode stbuf;

	if(fopen(msg, &in) < 0) {
		printf("Can't open \"%s\"\n", msg);
		return;
	}
	drft = m_maildir(draft);
	if((out = open(drft, 0)) >= 0) {
/*
		cp = concat("\"", drft, "\" exists; delete? ", 0);
		while((i = gans(cp, anysh)) == 2)
			showfile(drft);
		if(!i)
			return;
		free(cp);
*/
		close(out);
	}
	if((out = creat(drft, m_gmprot())) < 0) {
		printf("Can't create \"%s\".\n", drft);
		return;
	}

	state = FLD;
	msgid = to = from = cc = sub = date = 0;

    for(;;) {

	switch(state = m_getfld(state, name, field, sizeof field, &in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(uleq(name, "from"))
			from = add(field, from);
		if(uleq(name, "cc"))
			cc = add(field, cc);
		if(uleq(name, "subject"))
			sub = add(field, sub);
		if(uleq(name, "date"))
			date = add(field, date);
		if(uleq(name, "to"))
			to = add(field, to);
		if(uleq(name, "message-id"))
			msgid = add(field, msgid);
		if(state == FLDEOF)
			goto done;
		break;

	case BODY:
	case BODYEOF:
	case FILEEOF:
		goto done;

	default:
		printf("getfld returned %d\n", state);
		return;
	}

    }

done:   if(!from) {
		printf("No one to reply to!!!\n");
		return;
	}
	close(in.b_fildes);
	type(out, "To: ");
	if(*from == ' ') from++;
	type(out, from);
	if(cc) {
		while(*cc == ' ') cc++;
		if(*cc == '\n' && cc[1] == 0) cc = 0;
	}
	if(to) {
		while(*to == ' ') to++;
		if(*to == 0) to = 0;
	}
	if(!ccme)
		to = 0;
	if(cc || to)
		type(out, "cc: ");
	if(cc) {
		if(to) {
			cp = cc+length(cc)-1;
			if(*cp == '\n') *cp = 0;
			while(*--cp == ' ') ;
			*++cp = 0;
		}
		type(out, cc);
	}
	if(to) {
		if(cc)
			type(out, ",\n    ");
		type(out, to);
	}
	if(sub) {
		type(out, "Subject: ");
		if(*sub == ' ') sub++;
		if((sub[0] != 'R' && sub[0] != 'r') ||
		   (sub[1] != 'E' && sub[1] != 'e') ||
		   sub[2] != ':')
			type(out, "Re: ");
		type(out, sub);
	}
	if(date) {
		type(out, "In-reply-to: Your message of ");
		date[length(date)-1] = '.';
		if(*date == ' ') date++;
		type(out, date);
		type(out, "\n");
		if(msgid) {
			type(out, "             ");
			if(*msgid == ' ') msgid++;
			type(out, msgid);
		}
	}
	type(out, "----------\n");
	close(out);
	if(!ed && (ed = m_find("editor")) == -1)
#ifdef PROMPT
		ed = prmtproc;
#else
		ed = sysed;
#endif
	intr = signal(SIGINT, 1);
	if((out = fork()) == 0) {
		unlink("@"); link(msg, "@");  /* An easy handle on cur msg */
		m_update();
		flush();
	/***    printf("execlsh(%s, %s, 0)\n", ed, drft); flush();   ***/
		execlsrh(ed, drft, 0);
		printf("Can't exec the editor:  ");
		flush(); perror(ed); exit(1);
	} else if(out == -1) {
		printf("No Forks\n");
		return;
	} else {
		while((state = waita(&status)) != -1 && state != out) ;
		signal(SIGINT, intr);
		if(status) {
			if(status > 0377)
				unlink(drft);
			printf("[command aborted--%s %s]\n", drft,
				status > 0377? "deleted" : "preserved");
			unlink("@");
			return;
		}
	}
#ifdef TEST
	printf("!! Test Version of SEND Being Run !!\n");
	printf("   Send verbose !\n\n");
#endif
/*	cp = concat("Send \"", draft, "\"? ", 0); */
	cp = "Send? ";
	if((j = gans(cp, anyv)) > 0) {
		stat("@", &stbuf);
		if(stbuf.i_nlink == 1)
			if(unlink(msg) == -1 || link("@", msg) == -1) {
				printf("Can't update %s from @ file!\n",msg);
				return(0);
			}
		unlink("@");            /* Remove this extra link       */
		if(anot > 0 || (!anot && m_find("reply-annotate") != -1)) {
			while((state = fork()) == -1) sleep(5);
			if(state) {
				while(out=wait()!= -1 && out != state);
				if(stat(drft, field) == -1)
					annotate(msg, "Replied", "");
				return;
			}
		}
		i = 0;
		vec[i++] = "mh-sndproc";
		vec[i++] = drft;
		if(j == 2)
			vec[i++] = "-verbose";
		vec[i++] = 0;
		m_update();
		flush();
		execv(sndproc, vec);
		printf("Can't exec send!!\n");
	} else
		unlink("@");            /* Remove this extra link       */
}

