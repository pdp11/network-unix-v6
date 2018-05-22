#include "mh.h"
#include "signals.h"


char    draft[],
	components[],
	stdcomps[],
	sysed[],
#ifdef PROMPT
	prmtproc[],
#endif
	sndproc[];

int	fout;

char    *anyus[] {
	"no",   0,
	"yes",  0,
	"use",  0,
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
	"editor editor",  0,      /* 0 */
	"form formfile",  0,      /* 1 */
	"use",            0,      /* 2 */
	"nouse",          0,      /* 3 */
	"help",           4,      /* 4 */
	0,                0
};

main(argc, argv)
char *argv[];
{
	register char *cp;
	register int in, out;
	int use, cnt, status, intr;
	char buf[512], path[128], *ed, *file, *vec[10], *form;
	char *inp, *ap;
	char *arguments[50], **argp;

	fout = dup(1);
#ifdef NEWS
	m_news();
#endif
	form = use = file = ed = 0;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	inp = ap;
	if((cp = m_find(ap)) != -1) {
		ap = brkstring(cp = getcpy(cp), " ", '\n');
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-') {
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							/* unknown */
			case -1:printf("-%s unknown\n", cp);
				goto leave;
			case 0: if(!(ed = *argp++)) {   /* -editor */
      missing:  printf("Missing argument for %s switch\n", argp[-2]);
					goto leave;
				}
				continue;
			case 1: if(!(form = *argp++))   /* -form */
					goto missing;
				continue;
			case 2: use = 1;  continue;     /* -use */
			case 3: use = 0;  continue;     /* -nouse */
			case 4: help(concat( inp, " [file] [switches]", 0),
				     switches);
				goto leave;
			}
		}
		file = cp;
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
	if(!file)
		file = draft;
	copy(m_maildir(file), path);
	if((out = open(path, 0)) >= 0) {
		if(use || fdcompare(in, out))
			goto editit;
/*
		cp = concat("\"", path, "\" exists; delete? ", 0);
		while((status = gans(cp, anyus)) == 3)
				showfile(path);
		if(status == 2) {
			use++;
			goto editit;
		}
		if(status == 0)
			goto leave;
*/
		close(out);
	} else if(use) {
		printf("\"%s\" doesn't exist!\n", path);
		goto leave;
	}
	if((out = creat(path, m_gmprot())) < 0) {
		printf("Can't create \"%s\"\n", path);
		goto leave;
	}
	do
		if(cnt = read(in, buf, sizeof buf))
			write(out, buf, cnt);
	while(cnt == sizeof buf);
	close(in);
editit:
	close(out);
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
		execlsrh(ed, path, 0);
		printf("Can't exec editor!!\n");
		flush();  exit(1);
	} else if(in == -1) {
		printf("No forks!\n");
		goto leave;
	} else
		while((out = waita(&status)) != -1 && out != in) ;
	if(status) {
		printf("[command aborted--%s ", file);
		if(!use && status > 0377) {
			unlink(path);
			printf("deleted]\n");
		} else
			printf("preserved]\n");
		goto leave;
	}
	signal(SIGINT, intr);
#ifdef TEST
	printf("!! Test Version of SEND Being Run !!\n");
	printf("   Send verbose !\n\n");
#endif
/*	in = concat("Send \"", file, "\"? ", 0); */
	in = "Send? ";
/***    printf("Send \"%s\"? ", file);  ***/
	if((out = gans(in, anyv)) > 0) {
		in = 0;
		vec[in++] = "mh-sndproc";
		vec[in++] = path;
		if(out == 2)
			vec[in++] = "-verbose";
		vec[in++] = 0;
		m_update();
		flush();
		execv(sndproc, vec);
		printf("Can't exec send process.\n");
	}
leave:
	m_update();
	flush();
}





