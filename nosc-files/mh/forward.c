/* formerly forw */
#include "mh.h"
#include "iobuf.h"
#include "signals.h"

/*#define TEST 1*/

char    draft[],
	components[],
	stdcomps[],
	sysed[],
#ifdef PROMPT
	prmtproc[],
#endif
	sndproc[];

int *vec[MAXARGS], fout;
struct msgs *mp;
char drft[128];

char    *anysh[] {
	"no",   0,
	"yes",  0,
	"show", 0,
	0,
};

char    *anyv[] {
	"no",           0,
	"yes",          0,
	"verbose",      0,
	0,
};
struct swit switches[] {
	"all",               -3,      /* 0 */
	"annotate",           0,      /* 1 */
	"noannotate",         0,      /* 2 */
	"editor editor",      0,      /* 3 */
	"form formfile",      0,      /* 4 */
	"help",               4,      /* 5 */
	0,                    0
};

main(argc, argv)
char *argv[];
{
	char *folder, *maildir, *msgs[100], *ed, *form;
	register int msgnum;
	register char *cp, *ap;
	char *inp;
	int msgp, status, anot;
	int in, out, intr;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	form = anot = folder = msgp = ed = 0;
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
							     /* -all */
			case 0: printf("\"-all\" changed to \"all\"\n");
				goto leave;
			case 1: anot = 1;  continue;         /* -annotate */
			case 2: anot = 0;  continue;         /* -noannotate */
			case 3: if(!(ed = *argp++)) {        /* -editor */
      missing:  printf("Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 4: if(!(form = *argp++))        /* -form */
					goto missing;
				continue;
							     /* -help */
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
		printf("No undeleted messages specified\n");
		goto leave;
	}
	if(form) {
		if((in = open(m_maildir(form), 0)) < 0) {
			printf("Can't open form file: %s\n", form);
			goto leave;
		}
	} else if((in = open(m_maildir(components), 0)) < 0 &&
		   (in = open(stdcomps, 0)) < 0) {
			printf("Can't open default components file!!\n");
			goto leave;
	}
	copy(m_maildir(draft), drft);
	if((out = open(drft, 0)) >= 0) {
/*
		if(!fdcompare(in, out)) {
			cp = concat("\"", drft, "\" exists; Delete? ", 0);
			while((msgnum = gans(cp, anysh)) == 2)
				showfile(drft);
			if(!msgnum)
				return;
		}
*/
		close(out);
	}
	if((out = creat(drft, m_gmprot())) < 0) {
		printf("Can't create \"%s\"\n", drft);
		goto leave;
	}
	cpydata(in, out);
	close(in);
	printf("Forwarding message%s ", mp->numsel > 1 ? "s" : "");
	for(msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		if(mp->msgstats[msgnum]&SELECTED)  {
			if((in = open(cp = m_name(msgnum), 0)) < 0) {
				printf("Can't open message \"%s\"\n", cp);
				unlink(drft);
				goto leave;
			}
			printf("%d ", msgnum);
			type(out, "-------");
			if(msgnum == mp->lowsel) {
				type(out, " Forwarded Message");
				if(mp->numsel > 1)
					type(out, "s");
			}
			type(out, "\n");
			cpydata(in, out);
			close(in);
		}
	type(out, "------- End of Forwarded Message");
	if(mp->numsel > 1)
		type(out, "s");
	type(out, "\n");
	close(out);
	printf("\n");
	flush();
	m_replace("folder", folder);
	if(mp->lowsel != mp->curmsg)
		m_setcur(mp->lowsel);
	if(!ed && (ed = m_find("editor")) == -1)
#ifdef PROMPT
		ed = prmtproc;
#else
		ed = sysed;
#endif
	intr = signal(SIGINT, 1);
	if((in = fork()) == 0) {
		m_update();
		flush();
		execlsrh(ed, drft, 0);
		printf("Can't exec the editor!!\n");
		flush(); exit(1);
	} else if(in == -1) {
		printf("No forks!\n");
		goto leave;
	} else
		while((out = waita(&status)) != -1 && out != in) ;
	signal(SIGINT, intr);
	if(status) {
		if(status > 0377)
			unlink(drft);
		printf("[command aborted--%s %s]\n", drft,
			status > 0377? "deleted" : "preserved");
		goto leave;
	}
#ifdef TEST
	printf("!! Test Version of SEND Being Run !!\n");
	printf("   Send verbose !\n\n");
#endif
/*	cp = concat("Send \"", draft, "\"? ", 0); */
	cp = "Send? ";
	if((out = gans(cp, anyv)) > 0) {
		if(anot) {
			while((in = fork()) == -1) sleep(5);
			if(in) {
				while(msgnum = wait() != -1 && msgnum != in);
				doano();
				goto leave;
			}
		}
		in = 0;
		vec[in++] = "mh-sndproc";
		vec[in++] = drft;
		if(out == 2)
			vec[in++] = "-verbose";
		vec[in++] = 0;
		m_update();
		flush();
		execv(sndproc, vec);
		printf("Can't exec send process.\n");
		flush(); exit(1);
	}

 leave:
	m_update();
	flush();
}


cpydata(in, out)
{
	char buf[512];
	register int i;

	do
		if((i = read(in, buf, sizeof buf)) > 0)
			write(out, buf, i);
	while(i == sizeof buf);
}


doano()
{
	struct iobuf in;
	char name[NAMESZ], field[256];
	register int ind, state;
	register char *text;

	if(stat(drft, field) != -1) {
		printf("%s not sent-- no annotations made.\n", drft);
		return;
	}
	text = copy(drft, field);
	text[1] = 0;
	do
		*text = text[-1];
	while(--text >= field && *text != '/');
	*++text = ',';                  /* New backup convention */
	if(fopen(field, &in) < 0) {
		printf("Can't open %s\n", field);
		return;
	}
	state = FLD;
	text = 0;
   for(;;) switch(state = m_getfld(state, name, field, sizeof field, &in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(uleq(name, "to") || equal(name, "cc")) {
			if(state == FLD) {
				text = add(name, text);
				text = add(":", text);
			}
			text = add(field, text);
		}
		if(state == FLDEOF)
			goto out;
		continue;
	case BODY:
	case BODYEOF:
		goto out;
	default:
		printf("Getfld returned %d\n", state);
		return;
	}

out:
	close(in.b_fildes);

	for(ind = mp->lowsel; ind <= mp->hghsel; ind++)
		if(mp->msgstats[ind] & SELECTED)
			annotate(m_name(ind), "Forwarded", text);
}
