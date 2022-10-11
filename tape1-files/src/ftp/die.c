#
#include "stdio.h"
#include "signal.h"

struct {int pid; FILE *file;} diepid = { 0, stdout };

diequit()
{
	extern char *progname;
	die(SIGQUIT, "%s: killed by quit signal\n", progname);
}

dieother()
{
    die(SIGEMT, NULL); /* this is the ordinary state, so don't announce it*/
}

dieinit(other, errfd)
int other;
FILE *errfd;
{
	diepid.pid = other;      /* id of the process to kill */
	diepid.file = errfd;      /* fd to print messages on */
	signal(SIGEMT, dieother);
	signal(SIGQUIT, diequit);
}

/* VARARGS */
die(status, s1, s2, s3, s4, s5)
int status;         
char *s1, *s2, *s3, *s4, *s5;
{
	if (s1 != NULL) fprintf(diepid.file, s1, s2, s3, s4, s5);
	if (diepid.pid) kill (diepid.pid, SIGEMT);
	exit(status);
}
