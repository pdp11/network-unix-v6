/*
 * globals -- external variables shared between files of thp/telnet
 */
char *progname;    /* Set to name of program (argv[0]) */
char *arglist[50];  /* Set to arg list (argv[1...]) */

int termfd 2;         /* Set to fd to use for terminal modifying */
int prompt '^';      /* User command prompt, or -1 if none */
int verbose;          /* Set to get more info about state of connection */

int NetEscape;        /* Scanned for in from-net data stream */
int NoUsrEOF;             /* Set if EOF from user should not cause stop */
char *NetConP;        /* Pointer to connection data block */
char *PreP;            /* Pointer to conn block for preempting conn */
