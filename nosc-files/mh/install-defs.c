#define TRUE 1
#include "mh.h"
#include "stat.h"

char   *anoyes[],       /* Std no/yes gans array        */
	mh_defs[],
	defalt[];

int     fout;
char    *mypath, defpath[128];

main(argc, argv)
char *argv[];
{
	register char *cp, *path;
	register struct node *np;
	int autof, detached, exitstat;
	struct inode stbuf;

	fout = dup(1);
	autof = (argc == 2 && equal(argv[1], "-auto"));
	detached = (argc == 2 && equal(argv[1], "-detached"));
	exitstat = 1;                 /* Assume errors will occur */
	mypath = getpath(getruid());  /* to prevent recursion via m_getdefs */
	copy(mh_defs, copy(mypath, defpath));
	if(stat(defpath, &stbuf) != -1) {
	    if(autof)
		printf("Install-defs invocation error!\n");
	    else if (!detached)
		printf("You already have an MH profile... use an editor \
to modify it.\n");
	    goto leave;

	}
	if(!detached && (autof || gans("Do you want help? ", anoyes))) {
printf("\nPrior to using MH, it is necessary to have a file in your login\n");
printf("directory (%s) named .mh_defs which contains information\n",mypath);
printf("to direct certain MH operations.  The only item which is required\n");
printf("is the path to use for all MH folder operations. The suggested MH\n");
printf("path for you is %s/mail...\n\n", mypath);
	}
	cp = concat(mypath, "/", "mail", 0);
	if(stat(cp, &stbuf) != -1) {
	    if((stbuf.i_mode&IFMT) == IFDIR) {
		cp = concat("You already have the standard MH directory \"",
			cp, "\".\nDo you want to use it for MH? ", 0);
		if( detached ? TRUE : gans(cp, anoyes))
		    path = "mail";
		else
		    goto xyz;
	    }
	} else {
	    cp = concat("Do you want the standard MH path \"", mypath,
			 "/", "mail\"? ", 0);
	    if( detached ? TRUE : gans(cp, anoyes))
		    path = "mail";
	    else {
    xyz:        if(gans("Do you want a path below your login directory? ",
		    anoyes)) {
		    printf("What is the path ??  %s/", mypath);
		    path = getans();
		} else {
		    printf("What is the whole path??  /");
		    path = concat("/", getans(), 0);
		}
	    }
	}
	chdir(mypath);
	if(chdir(path) == -1) {
		cp = concat("\"", path, "\" doesn't exist; Create it? ", 0);
		if( detached ? TRUE : gans(cp, anoyes))
			if(makedir(path) == 0) {
				if (!detached) printf("Can't create it!\n");
				goto leave;
			}
	} else
		if (!detached) printf("[Using existing directory]\n");

	np = m_defs = alloc(sizeof *np);
	np->n_name = "Path";
	np->n_field = path;
	np->n_next = 0;
	m_replace("folder", defalt);
	exitstat = 0;

leave:
	m_update();
	flush();
	exit(exitstat);
}


getans()
{
	static char line[128];
	register char *cp;
	register int c;

	flush();
	cp = line;
	while(c = getchar()) {
		if(c == '\n') {
			*cp = 0;
			return(line);
		}
		if(cp < (&line) + 1)
			*cp++ = c;
	}
	exit(1);
}
