/*
 * Globals shared between files of telnet
 */
#include "globdefs.h"
#include "tnio.h"
#include "ttyctl.h"
char * progname;	    /* Program name, set in main */
int escape = '^';	  /* Escape char */
char prompt[16] = "* "; /* Prompt sequence printed when escape typed */
int needprompt = 1;   /* Initially, prompt for user input */
int termfd = 2;
int inputfd;
int fsocket;/* Default foreign socket for open commands */
int debug;  /* Debugging the protocol -- print a lot */
int verbose;/* Makes TELNET library print info */
int udone;  /* Set when user done */
int ndone;  /* Set when network connection done */
int xdone;  /* Set to abort without waiting for net */
#ifndef AWAIT
int other_pid;	/* Two-process version */
int par_uid;    /* parent id */
#endif

NETCONN * NetConP;  /* Current connection */
TTYMODE *NetModeP;    /* TTY mode block for connection */
int usercr = 1;    /* Follow CR with LF on input */
