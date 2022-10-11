#include "ftp.h"

/************ F I L E S   U S E D   B Y   F T P ***************/
/******/						/******/
/******/        char *SNDMSG  =  "sndmsg";		/******/
/******/        char *NSNDMSG =  "nsndmsg";	        /******/
/******/        char *pwfile  =  "/etc/passwd";         /******/
/******/        char *afile   =  "/etc/net/aliases";    /******/
/******/        char *old_afile= "/usr/net/aliases";    /******/
/******/        char *MKDIR   =  "mkdir";		/******/
/******/        char *RMDIR   =  "rmdir";		/******/
/******/	char *SHELL   =  "sh";			/******/
/******/        char *LS      =  "ls";			/******/
/******/        char *UTMP    =  "/etc/utmp";		/******/
/******/						/******/
/************ F I L E S   U S E D   B Y   F T P ***************/

int     DOSNDMSG = 0;   /* -1 is old sndmsg, 1 is nsndmsg, 0 is new sndmsg */
char    *BUGS =    "bugs at bbn-unix";
char    *progname, *us, *them;
char    *errmsg();

struct net_stuff NetParams;

/*	Command fence between log and non-log required commands */
struct comarr *NOLOGCOMM;
/* communications buffers/counts/pointers */

#define BUFL 600        /* length of buf */
#define PIPELEN 512     /* used in do_list() */

int     netcount = 0;      /* number of valid characters in netbuf */
char    netbuf[512];       /* the place that has the valid characters */
char    *netptr =  netbuf; /* next character to come out of netbuf */
int     lastpid;           /* pid of last process */

char buf[BUFL];         /* general usage */
char pwbuf[512];        /* password entry for current user */
char username[512];     /* holds <user>:<password> for logging purposes */
char renmebuf[40];	/* holds RNFR argument waiting for RNTO comm */
int  rcvdrnfr;		/* when on says we received RNFR com ok to do RNTO */
int  guest;             /* non-zero if guest user */
/* 
 */
/*	Stat struct for finding out about file */
#include "sys/types.h"
#include "sys/stat.h"
struct stat statb;
#define S_ACCESS (S_IWRITE!S_IREAD_S_IEXEC)
#define S_WRLD(x) (x>>6)
#define S_GRP(x) (x>>3)
#define S_ANYACC (S_WRLD(S_ACCESS)!S_GRP(S_ACCESS)!S_ACCESS)

int     logtries;		/* current number of login tries */
/* Current User and Group ids */
int curuid;		/* set by the loguser procedure */
int curgid;		/* in resonse to a user pass sequence */

int just_con_data;	/* used by getline procedure to limit function */
char *arg;		/* zero if no argument - pts to comm param */
#define TREE 1

#define LSTARGSIZ       256     /* buffer for argument to shell in list & nlst  */
#define NLSTFLAG        1       /* flag to distinguish the list & nlst commands */

extern int errno;	/* Has Unix error codes */

#include "errno.h"

/* globals used by the mailing stuff */

#define XRSQ_D  0       /* default (no) scheme */
#define XRSQ_T  1       /* Text first scheme */
#define XRSQ_R  2       /* Recipient first scheme (not implimented yet) */
#define MBNAMSIZ 20

char mfname[MBNAMSIZ];  /* storage for mail file name */
int  xrsqsw =   XRSQ_D; /* semaphore for the xrsq/xrcp stuff */
/* 
 */
/* character defines */

#define CR	015	/* carriage return */
#define LF	012	/* line feed */
#define NUL	000	/* null */

char hex[] = "0123456789ABCDEF";  /* Hexadecimal */
/*  */
#include "ftp_lib.h"

/* the ABORT TRANSFER flag */
int abrtxfer;			/* not used by the server */

/* The TYPE variable	-	modified by type command */
int type;                      /* types of transfer */

/* The MODE variable	-	modified by the mode command */
int mode;                      /* modes of transfer */

/* The STRUCT variable	-	modified by the stru command */
int stru;                     /* structure of transfer */

/* the CMDSTATE variable	defines generally what commands are expected */
int cmdstate;                   /* state of the nation */
#define EXPECTUSER	0	/* waiting for user command */
#define EXPECTPASS	1	/* got a user now need password */
#define EXPECTCMD	2	/* logged in now transfer commands */

#define MAXLOGTRIES     3       /* number of login tries */
/* 
 */
/* declarations of command functions, so pcc doesn't complain */
int user(), pass(), accept(), mail(), datamail(), cmd_abort();
int bye(), ftpstat(), help(), xsen(), xsem(), typecomm();
int modecomm(), structure(), byte(), retrieve(), store();
int append(), delete(), renme_from(), renme_to(), list();
int nlst(), cwd(), xrsq(), xrcp();

#ifdef  TREE
int xcup(), mkd(), rmdir(), xpwd();
#define QUOTDSIZ        80      /* size of static array for syntactify */
#endif  TREE

/*
/****************************************************************
 *                                                              *
 *      C O M M A N D   D I S P A T C H   T A B L E             *
 *                                                              *
 ****************************************************************/

struct comarr		/* format of the command table */
{
	char *cmdname;		/* ascii name */
	int (*cmdfunc)();       /* command procedure to call */
} commands[] = {

	"user", user,		"pass", pass,
	"acct", accept,		"mail", mail,
	"mlfl", datamail,        PORT , accept,
	"abor", cmd_abort,       QUIT ,  bye,
	"noop", accept,		"stat", ftpstat,
	"help", help,            MSND , xsen,
	 MSOM , xsem,		 MSRQ , xrsq,
	 MRCP , xrcp,		"type", typecomm,
	"stru", structure,	"allo", accept,
	"rest", accept,		"mode", modecomm,

#define LASTNOLOGCOM	19
/*
 *	This is the fence between log and non-log required commands.
 *	LASTNOLOGCOM is the index of the last command which does not
 *	require the user to be logged in. Be sure to change
 *	LASTNOLOGCOM above if commands are added above here.
 */
	"retr", retrieve,	"stor", store,
	"appe", append,		"dele", delete,
	"rnfr", renme_from,	"rnto", renme_to,
	"list", list,		"nlst", nlst,
	"cwd",  cwd,

#ifdef TREE
	"xcwd", cwd,		/* for compatability */
	"xmkd", mkd,		/* make a directory */
	"xrmd", rmdir,		/* remove a directory */
	"xcup", xcup,		/* change to parent of this directory */
	"xpwd", xpwd,		/* print working directory */
#endif TREE

	0,      0

};
/**/
#ifdef NCP
/****************************************************************************
 *                                                                          *
 *    N    N   CCC   PPPP         d           f    i                        *
 *    NN   N  C   C  P   P        d          f f                            *
 *    N N  N  C      P   P      ddd    ee    f    ii   nn nn     ee   sss   *
 *    N  N N  C      PPPP      d  d   eeee  ffff   i    nn  n   eeee  sss   *
 *    N   NN  C   C  P         d  d   e      f     i    n   n   e       s   *
 *    N    N   CCC   P          dd d   eee   f    iii  nn   nn   eee  sss   *
 *                                                                          *
 ****************************************************************************/

#define U4      4       /* offset from base socket */
#define U5      5       /* offset from base socket */

#define tel_iac 255


#endif NCP
/**/
#ifdef TCP
/****************************************************************************
 *                                                                          *
 *   TTTTTTT   CCC   PPPP         d           f    i                        *
 *   T  T     C   C  P   P        d          f f                            *
 *     T      C      P   P      ddd    ee    f    ii   nn nn     ee   sss   *
 *    T       C      PPPP      d  d   eeee  ffff   i    nn  n   eeee  sss   *
 *     T      C   C  P         d  d   e      f     i    n   n   e       s   *
 *      TT     CCC   P          dd d   eee   f    iii  nn   nn   eee  sss   *
 *                                                                          *
 ****************************************************************************/
#endif TCP
