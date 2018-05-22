#include "mh.h"
#include "iobuf.h"
#include "errors.h"
#include "sgtty.h"
#include "signals.h"

/*#define CEDIT 01 */	/* use following editing chars */
#define CKILL   006     /* @ => <CLOSE>         */
#define CERASE  001      /* # => <CTRL A>        */

/*define EDIT   001*/	/* edit after filling in fields */
char    sysed[];

int     fout;
struct  iobuf in, out;
struct  sgtty sg;
struct swit switches[] {
	"editor editor",  0,      /* 0 */
	"erase chr",      2,      /* 1 */
	"kill chr",       0,      /* 2 */
	"help",           4,      /* 3 */
	0,                0
};


main(argc, argv)
char *argv[];
{
	char tmpfil[32], *drft, name[NAMESZ], field[512], *ed;
	int exitstat;
	char skill, serase;
	char *killp, *erasep;
	register int i, state;
	register char *cp;
	char *ap;
	char *arguments[50], **argp;
	int sig();
	int status, pid, wpid, intr;

	fout = dup(1);
	skill = exitstat = 0;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	if((cp = m_find(ap)) != -1) {
		ap = brkstring(cp = getcpy(cp), " ", '\n');
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++)
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto badleave;
							/* unknown */
			case -1:printf("prompter: -%s unknown\n", cp);
				goto badleave;
			case 0: if(!(ed = *argp++)) {   /* -editor */
      missing:  printf("prompter: Missing argument for %s switch\n", argp[-2]);
					goto badleave;
				}
				continue;
			case 1: if(!(erasep = *argp++))     /* -erase */
					goto missing;
				continue;
			case 2: if(!(killp= *argp++))      /* -kill */
					goto missing;
				continue;
			case 3: help("prompter    [file]  [switches]",
				     switches);
				goto badleave;
			}
		else
			drft = cp;
	if(!drft) {
		printf("prompter: missing skeleton\n");
		goto badleave;
	}
	if(fopen(drft, &in) == -1) {
		printf("Can't open %s\n", drft);
		goto badleave;
	}
	copy(makename("prmt", ".tmp"), copy("/tmp/", tmpfil));
	if(fcreat(tmpfil, &out) == -1) {
		printf("Can't create %s\n", tmpfil);
		goto badleave;
	}
	chmod(tmpfil, 0700);
	signal(SIGINT, &sig);
	gtty(0, &sg);
	skill = sg.sg_kill;
	serase = sg.sg_erase;
#ifdef CEDIT
	sg.sg_kill =    killp ?  chrcnv(killp) : CKILL;
	sg.sg_erase =   erasep ? chrcnv(erasep) : CERASE;
#else
	sg.sg_kill =    killp ?  chrcnv(killp) : skill;
	sg.sg_erase =   erasep ? chrcnv(erasep) : serase;
#endif
	stty(0, &sg);
	if ( skill != sg.sg_kill || serase != sg.sg_erase ) {
		printf("Erase Char="); chrdisp(sg.sg_erase);
		printf("; Kill Line="); chrdisp(sg.sg_kill);
		printf(".\n"); flush();
	}

	state = FLD;
	for(;;) switch(state = m_getfld(state,name,field,sizeof field,&in)) {

	case FLD:
	case FLDEOF:
	case FLDPLUS:
		if(field[0] != '\n' || field[1] != 0) {
			printf("%s:%s", name, field);
			puts(name, &out);
			putc(':', &out);
			puts(field, &out);
			while(state == FLDPLUS) {
				state=m_getfld(state,name,field,sizeof field,&in);
				printf("%s", field);
				puts(field, &out);
			}
		} else {
			printf("%s: ", name);
			flush();
			i = getln(&field);
			if(i == -1)
				goto badleave;
			if(i == 0 && (field[0] == '\n' || !field[0]))
				continue;
			puts(name, &out); putc(':', &out);
			do {
				if(field[0] != ' ' && field[0] != '\t')
					putc(' ', &out);
				puts(field, &out);
			} while(i == 1 && (i = getln(&field)) >= 0);
			if(i == -1)
				goto badleave;
		}
		field[0] = 0;
		if(state == FLDEOF)
			goto body;
		continue;

	case BODY:
	case BODYEOF:
	case FILEEOF:
  body:         puts("--------\n", &out);
		printf("--------\n");
		if(field[0]) {
			do {
				puts(field, &out);
				/*printf("%s", field);*/
			} while(state == BODY &&
				(state=m_getfld(state,name,field,sizeof field,&in)));
		}
		flush();
		for(;;) {
			i = getln(&field);
			if (i == -1) goto badleave; /**/
			if(field[0] == 0)
				break;
			puts(field, &out);
		}
		goto done;

	default:
		printf("Bad format file!\n");
		goto badleave;
	}


done:
	printf("--------\n"); flush();
	fflush(&out);
	close(out.b_fildes);
	fopen(tmpfil, &out);
	close(in.b_fildes);
	fcreat(drft, &in);              /* Truncate prior to copy back */
	do
		if((i = read(out.b_fildes, field, sizeof field)) > 0)
			write(in.b_fildes, field, i);
	while(i == sizeof field);
	close(in.b_fildes);
	close(out.b_fildes);
	unlink(tmpfil);
again:
	if(skill) {
		sg.sg_kill = skill;
		sg.sg_erase = serase;
		stty(0, &sg);
		skill = 0;
	}

#ifdef EDIT
	printf("Edit? ");
	flush();
	getln(&field);
	if(field[0] == 0)
		goto badleave;
	field[length(field) - 1] = 0;   /* zap <lf> */
	if(field[0] == 0) {
		printf("Options are:\n  no\n  yes\n  show\n  <any editor>\n");
		goto again;
	}
	if(ssequal(field, "no"))
		goto leave;
	if(ssequal(field, "show")) {
		if(showfile(drft))
			goto badleave;
		goto again;
	}
	if(ssequal(field, "yes")) {
		if(!ed &&
		   ((ed = m_find("editor")) == -1 ||  equal(invo_name(), ed)))
			ed = sysed;
	} else
		ed = field;
	intr = signal(SIGINT, 1);
	if((pid = fork()) == 0) {
	    /*  m_update();  */
		flush();
		execlsrh(ed, drft, 0);
		exit(-1);               /* can't exec editor */
	} else if(pid == -1) {
		printf("prompter: no forks!\n");
		goto leave;
	} else
		while((wpid = waita(&status)) != -1 && wpid != pid) ;
	if((status & 0177400) == 0177400) goto again;   /* -1 */
	signal(SIGINT, intr);
#endif
	goto leave;

badleave:
	exitstat = 1;
leave:
	m_update();
	flush();
	if(skill) {
		sg.sg_kill = skill;
		sg.sg_erase = serase;
		stty(0, &sg);
	}
	exit(exitstat);
}




getln(buf)
char *buf;
{
	register char *cp;
	register int c;

	cp = buf;
	*cp = 0;
	for(;;) {
		c = getchar();
		if(c == 0)
			if(errno == EINTR)
				return(-1);
			else
				return(0);
		if(c == '\n') {
			if ( cp != buf ) /* if not first char in line */
				if(cp[-1] == '\\') {
					cp[-1] = c;
					return(1);
				}
				if((buf==&(cp[-1]))&&(cp[-1] == '.')) {
					buf[0] = 0;
					return(0);
				}
				*cp++ = c;
				*cp   = 0;
				return(0);
		}
		if(cp < buf + 500)
			*cp++ = c;
		*cp = 0;
	}
}


sig()
{
	signal(SIGINT, &sig);
	return;
}


chrcnv(str)
{
	register char *cp;
	register int c;

	cp = str;
	if((c = *cp++) != '\\')
		return(c);
	c = 0;
	while(*cp && *cp != '\n') {
		c =* 8;
		c =+ *cp++ - '0';
	}
	return c;
}


chrdisp(chr)
{
	register int c;

	c = chr;
	if(c < ' ')
		printf("<CTRL-%c>", c + '@');
	else
		printf("%c", c);
}
