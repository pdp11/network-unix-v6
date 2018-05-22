#include "/m/mh/mh.h"
#include "/m/mh/iobuf.h"

char    *mypath, defpath[128];
char    mh_defs[];
char    installproc[];

m_getdefs()
{
	register struct node *np;
	register int state, pid;
	int status;
	struct iobuf ib;
	char name[NAMESZ], field[128];

	if(defpath[0])
		return;         /* We've already been called!   */
	if(!mypath)
		mypath = getpath(getruid());
	copy(mh_defs, copy(mypath, defpath));

	if(fopen(defpath, &ib) < 0) {
		if((pid = fork()) == 0) {
			execl(installproc, "install-defs", "-detached", 0);
			printf("Can't install .mh_defs!!\n");
			flush();  exit(1);
		} else if(pid == -1) {
			printf("No forks!\n");
			flush();  exit(1);
		} else
			while((state = waita(&status)) != -1 && state != pid)
				;
		if(status || fopen(defpath, &ib) < 0) {
			printf("[install-defs aborted]\n");
			flush();  exit(1);
		}
	}

#ifdef NEWS
	fstat(ib.b_fildes, field);
	deftime = (&field)->i_atime;
#endif

	np = &m_defs;
	state = FLD;
    for(;;)
	switch(state = m_getfld(state,name,field,sizeof field,&ib)) {
	case FLD:
	case FLDEOF:
		np->n_next = alloc(sizeof *np);
		np = np->n_next;
		np->n_name = getcpy(name);
		np->n_field = trimcpy(field);
		np->n_next = 0;
		if(state == FLDEOF) {
			close(ib.b_fildes);
			return(0);
		}
		continue;
	case BODY:
	case BODYEOF:
		printf(".mh_defs must not contain a body--it can't \
end with a blank line!\n");
	default:
		printf("Bad format: .mh_defs!\n");
		flush();
		exit(1);
	}
}
