#include "ftp.h"
#include "usr.h"
#include "ftptelnet.h"
#include "ctype.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/timeb.h"
#include "sgtty.h"
#include "setjmp.h"
#include "ftp_lib.h"

/*
 * User FTP
 *
 * Modified by Dan Franklin Sept 8 1978 (BBN) to take
 * commands from "getline" routine. This enables it
 * to have its input redirected to a file of commands.
 * Also moved declarations of strings for command prompting
 * to ahead of the command table so that our new C compiler
 * won't complain.
 *
 * Modified by Dan Franklin (BBN) August 11 19598 to put
 * blank line after "From:" line in send-mail command.
 * The blank line indicates the end of the header as
 * per RFC733.
 *
 * 3/79, added cwd and quote commands.	delete inside rwait returns
 * to prompt instead of saying (incorrectly) "Host exited"  ado/bbn
 *
 * changed to use long hosts jsq BBN 3-25-79
 * use fmodes for creat mode, give error on NCP open fail jsq BBN 19July79
 * init and subshl changed to handle signals differently:  if parent
 * shell ignored signals, this process does too.  jsq BBN 5Aug79
 * add arguments to allow passing process id of other half so that one
 *	can kill the other when it is ready to die; also do all host
 *	name-number conversions in ftp.c so we don't have so much stuff
 *	in here.  jsq BBN 18Aug79
 *
 * made f_retr check if destination already existed, and not delete on error
 * if it does. This should eliminate one cause of the /dev/tty disappearances.
 * Also fixed signal handling in fork exec'ing shell. Dan Franklin (BBN) 12/21/79
 *
 * Added NLST command (like LIST but different at server end). Also changed
 * CMDENTS to use sizeof cmdtab.
 *
 * Added XSEN and XSEM commands, which are just like MAIL (to user FTP) but
 * send messages instead of/along with mail on servers which implement
 * them. Dan Franklin BBN Feb 8 '80
 *
 * Added SEND and GET synonyms for STOR and RETR respectively.  Added
 * LOGIN command with 1, 2 or 3 args, and new argument list types
 * ARG1AND2 and ARG1OR2OR3.  Buz Owen, BBN, March 4 '80.
 *
 * Fixed bug whereby LIST and NLST were synonyms of RETR.  Changed QUOTE
 * command to take two args.  Changed ARGSIZ to 80, and LINSIZ to 250.
 * Changed quote and cwd to use putarg, putstr, putcmd more conventionally.
 * Buz Owen, BBN, March 11, 80.
 *
 * ca. Mar 25, 1980 (dm at BBN):
 *   changed issep (c) to return TRUE if (c == '\0'); this is so movarg
 *     can act on things its already acted on, so it could be used in the
 *     multiple store stuff
 *   added the multiple store stuff to server and user processes, working
 *     on unix-unix tree transfers
 *   in chekds(), which opens the data connection, turned off user-interrupts
 *     while waiting for the connection to be openned.  at least with tenex,
 *     failure to open this connection after you've said you were going to can
 *     get the server confused, and it stops listening to you (probably
 *     waiting for the connection to open).
 *   changed f_stor() to act as a front end for a separate subroutine, store()
 *     which does the actual work;  store() is called by the multiple store
 *     stuff.
 *
 * jsq BBN 5April80 declared cmdtab as struct ftucmd cmdtab[] in initiali-
 *   zation, fixed definition of ftucmd.ft_info, declared getcmd properly in
 *   main before calling it, fixed getcmd to use ft_info correctly.
 *
 * dm (bbn) 15 apr 1980:
 *   installed mretrieve() command; gets an NLST from the other site, and then
 *     does a retrieve on all the names of files returned.
 *     has subcommands to translate tenex-like file-names into unixable file-
 *     names, and work other transformations
 *   changed error() to accept fprintf-like arguments, so i could improve
 *     error messages it returns
 *   added the statistics package
 *     note that statistics aren't gathered on recursive stores (done in
 *     a lower fork.
 *   moved all the MGET internals to the end of the file, so they would be
 *     easier to find
 * dm (bbn) 31 May 1980:
 *   put a 1-second sleep into the loop in sendabunch() tenex requires time
 *     to become convinced the last data-socket was closed, so, cycling too
 *     quickly through several files will result in a "cannot open data
 *     connection" message, and Unix will be sitting there trying to open
 *     the data socket, anyway...
 *   changed rwait() to know about the proposed 257 reply code which tells
 *     you what the directory was which was created.  Also changed ftptty
 *     to know about such things.
 *   put the interrupt stuff back into checkds()--its abscence caused too
 *     many problems, and i guess people can restart their tenex ftps if
 *     tenex gets confused...
 * jsq BBN 27May80 make prstat give baud rate as well as bytes/sec.
 * dm (bbn) 5 Sept. 80: make sure indat() & outdat() close the dataconnection
 *     when the file is through.
 * dm (bbn) 25 Nov. 80: make a version which works with both TCP & NCP
 *      changed setspec() & calling routines to handle the length of the
 *       tables automatically (i.e., recognize that a null entry implies
 *       the end of a table)
 *      changed f_mlfl() to use putcmd() & sndcmd() instead of writing
 *       directly to the network
 *      dsfds & dsfdr are structures now
 *
 * ado (bbn) feb - jun, 81.  finished tcp version, v7ized everything and 
 *	stdio'd everything possible,
 *	following agn's changes to c70 version, and further urging.
 *	combined ftp (the command), ftpmain, and ftptty into one program.
 *	stripped out data transfer routines in favor of ftp_lib routines which
 *	are common to both user and server ftp, supplied by agn.
 *	completely new version rwait to deal with systematic reply code scheme
 *	of new protocol.
 * ado (bbn) jul 81.  added cmdget and cmdsend commands, to retrieve into a
 *	a filter, and send output from a command  respectively.
 *  */
#define MGET    1       /* turn on the multiple store junk */

struct net_stuff NetParams;

extern char * progname;
char * malloc(), * strcpy();

char * HOST_ID, * them, * us;
char * errmsg();
char * getarg();
char * desyntactify();

#define ARGSIZ  80
#define TTBSIZ  160
#define MAXSTRING  10           /* Length of the longest command name */

struct spctab
    {
	char *typnm;
	char *typarg;
	int typnum;
    };
    
struct spctab tycode[] =
    {
	{ "ASCII", "A", 0 },
	{ "TELNET", NULL, 1 },		/* non implimented values in */
	{ "PRINT", NULL, 2 },		/* anyway, so we can give */
	{ "EBCDIC", NULL, 3 },		/* slightly more intelligent */
	{ "IMAGE", "I", 6 },
	{ "LOCAL", "L8", 7 },
	{  NULL, NULL, -1 }
    };

struct spctab mdcode[] =
    {
	{ "STREAM", "S",  0 },
	{ "BLOCK", NULL, -1 },			/* error messages. */
	{ "COMPRESSED", NULL, -1 },		/* not supported... */
	{  NULL, NULL, -1 },
    };

struct spctab strucode[] =
    {
	{ "FILE", "F", 0 },
	{ "RECORD", "R", 1 },
	{ NULL, NULL, -1 },
    };
/*  */
/* Initial [default] settings: */

int mode = MODES;
int type = TYPEA;
int stru = STRUF;

struct net_stuff dsfds;
struct net_stuff dsfdr;
FILE *fdfil = NULL;
FILE *fdpip = NULL;
int pipnum = -1;

/* Uninitialized data */

int abrtxfer;
char ttbuf[TTBSIZ], *ttptr, ttcnt;              /* For tty input */
char arg1[ARGSIZ], arg2[ARGSIZ], arg3[ARGSIZ];  /* For string arguments */
char linbuf[LINSIZ];
char *linptr;		/* For Telnet output */

jmp_buf env;					/* for long jumping */
struct stat sttbf;                              /* For file status info */
int uicount =   0;                              /* No. of user interrupts */
int xreply;                                     /* Global reply location */

char mlbf[82], *mlptr;                  /* For mail headers */

struct ftucmd				/* Structure of the command table */
    {
	char  *ft_cnm;
	int   ft_nargs;
	char  **ft_info;
	int   (*ft_fn)();
	char  *ft_desc;
    };

/* Definitions for ft_nargs field */
#define ARG0    1
#define ARG1    2
#define ARG2    4
#define ARG0OR1 010
#define ARG1OR2 020
#define ARG1AND2 040
#define ARG1OR2OR3 100
/*  */
/* Strings for command prompting */

char locfil[] =  "localfile:  ";
char rmtfil[] =  "remotefile: ";
char persn[] = "person: ";

char *afsnd[] = { "Input filter (command):", rmtfil };
char *afget[] = { locfil, "Output filter (command): " };
char badcmd[] = "bad filter/command: %s\n";
FILE *popen();

char aborstr[] = { TNIAC, TNIP, TNIAC, TNDM, 'A', 'B', 'O', 'R', 0 };

char *arsto[] = { locfil,
		  rmtfil };

char *arcwd[] = { "remote pathname: " };
char *ardir[] = { "local pathname: "};
char *ardel[] = { rmtfil };
char *arlis[] = { "remote pathname: ",
		  locfil };

char *armod[] = { "transfer mode: "};
char *arren[] = { "old remotefile: ",
		  "new remotefile: " };

char *arret[] = { rmtfil, locfil };

char *arate[] = { "[reset] " };
char *arper[] = { persn };
char *arquo[] = { "command [argument]: " };
char *arstr[] = { "file structure: " };
char *artyp[] = { "data type: " };
char *aruse[] = { "Username: " };
char *armlf[] = { locfil, persn };

#ifdef MGET
char *armret[] = { "foreign file-group descriptor: ",
		 "flags: " };

char *armsto[] = {locfil,
		"remote directory-name, or <cr> if wild-carding: " };
/*  */
/* some other multiple send and store guys */
char recurstor;
char dialog;
char mretname[32];              /* storage for the NLST temporary file */
#define MADEDIR 251     /* reply code for XMKD protocol */

char MadeDirectory[ARGSIZ];     /* storage for the string returned by a */
				/* XMKD call */

/* variables for the file-name transformer */
#define FNARGSIZ        ARGSIZ/2
#define FNTENEX         01              /* flag tenex-mode transforms  */
#define FNITS           02              /* flag its mode transforms    */
#define FNTOLCASE       04              /* flag lower-case transforms  */
#define FNALPHA         010             /* flag alphanumerics only     */
#define FNASK           020             /* flag ask user for each name */
#define FNCPY           040             /* flag use the name literally */
#define CTLV            026             /* tenex quotes things with ^V */
int     fnflag  =       0;              /* where the flags live        */

char    fnprefix[FNARGSIZ/2];           /* potential prefix            */

/* declarations of the dispatch functions, so the compiler doesn't complain */
int f_msto(), f_mkd(), f_rmdir();

#endif MGET
int subshl(), help(), f_acct(), f_appe(), byedie(), f_cld(), f_cwd();
int f_dele(), f_list(), f_log(), f_mail(), f_mlfl(), f_mode(), f_pass();
int f_quot(), f_rena(), f_status(), f_stor(), f_stru(), f_type(), f_user();
int f_retr(), f_rate(), f_err(), f_nlst(), f_mret(), f_fsnd(), f_fget();
int abort();

/*  */
struct ftucmd cmdtab[] =	/* Command Table -- must be alphabetical */
{
  {     "!   ",         ARG0,           NULL,   subshl,
        "invoke shell"                             },
  {	"ABORT",	0,		NULL,	abort,
  	   "debugging aid: take a core dump."},
  {     "ACCOUNT",      ARG0OR1,        NULL,   f_acct,
          "specify account on foreign host"          },
  {     "ACCT",         ARG0OR1,        NULL,   f_acct,
          "specify account on foreign host"          },
  {     "APPEND",       ARG2,           arsto,  f_appe,
          "append local file to foreign file"        },
  {     "BYE ",         ARG0,           NULL,   byedie,
          "close the connection, and exit"           },
  {     "CD  ",         ARG1,           ardir,  f_cld,
           "change directory on local host"           },
  {     "CHDIR",        ARG1,           ardir,  f_cld,
           "change directory on local host"           },
  {     "CMDGET",         ARG2,           afget,  f_fget,
	"retrieve file from foreign host into filter (command)",     },
  {     "CMDSEND",        ARG2,           afsnd,  f_fsnd,
	"send command output to foreign host file",     },
  {     "CWD ",         ARG1,           arcwd,  f_cwd,
           "change directory on foreign host"         },
  {     "DELETE",       ARG1,           ardel,  f_dele,
          "remove a file from the foreign host"      },
  {     "GET ",         ARG2,           arret,  f_retr,
          "retrieve a file from the foreign host"    },
  {     "HELP",         ARG0OR1,        NULL,   help,
            "briefly describe commands"                },
  {     "LIST",         ARG2,           arlis,  f_list,
          "get a directory listing to a local file"  },
  {     "LOG ",         ARG1OR2OR3,     aruse,  f_log,
           "log onto a foreign host"                  },
  {     "MAIL",         ARG1,		arper,   f_mail,
          "send mail to a user on the foreign host"  },
#ifdef  MGET
  {     "MGET",         ARG1OR2,        armret, f_mret,
          "multiple retrieve"                        },
  {     "MKDIR",        ARG1,           arcwd,  f_mkd,
           "make a directory on the foreign host"     },
#endif  MGET
  {     "MLFL",         ARG1AND2,       armlf,  f_mlfl,
          "mail file to a foreign host"              },
  {     "MODE",         ARG1,           armod,  f_mode,
          "specify transfer mode (STREAM or BLOCK)"   },
#ifdef  MGET
  {     "MRETRIEVE",    ARG1OR2,        armret, f_mret,
          "multiple retrieve"                        },
  {     "MSEND",        ARG1OR2,        armsto, f_msto,
          "multiple store"                           },
  {     "MSTORE",       ARG1OR2,        armsto, f_msto,
          "multiple store"                           },
#endif  MGET
  {     "NLST",         ARG2,           arlis,  f_nlst,
          "get a directory listing to a local file"  },
  {     "PASSWORD",     ARG0OR1,        NULL,   f_pass,
          "tell the foreign host your password"      },
  {     "QUIT",         ARG0,           NULL,   byedie,
          "close the connection, and exit"           },
  {     "QUOTE",        ARG1OR2,        arquo,  f_quot,
          "send quoted string to ftp server"		},
  {	"RATE",		ARG0OR1,	arate,	f_rate,
	  "print accumulated transfer rate, and optionally reset."},
  {     "RENAME",       ARG2,           arren,  f_rena,
          "rename a file on the foreign host"        },
  {     "RETRIEVE",     ARG2,           arret,  f_retr,
          "retrieve a file from the foreign host"    },
#ifdef  MGET
  {     "RMDIR",        ARG1,           ardir,  f_rmdir,
         "remove a directory on the foreign host"   },
#endif  MGET
  {     "SEND",         ARG2,           arsto,  f_stor,
          "store a local file on the foreign host"   },
  {     "STATUS",       ARG0OR1,        NULL,   f_status,
        "tell status of the ftp connection"        },
  {     "STORE",        ARG2,           arsto,  f_stor,
          "store a local file on the foreign host"   },
  {     "STRUCTURE",    ARG1,           arstr,  f_stru,
          "specify structure of files (RECORD or FILE (unstructured))"},
  {     "TYPE",         ARG1,           artyp,  f_type,
          "specify representation type (ASCII, IMAGE, or LOCAL)"},
  {     "USER",         ARG1,           aruse,  f_user,
          "tell the foreign host your name"          },
  {     MSOM,           ARG0OR1,        arper,   f_mail,
          "send text to a user's tty on the foreign host"},
  {     MSND,           ARG0OR1,        NULL,   f_mail,
          "send text to a user's tty on the foreign host"},
  {     NULL,           0,              NULL,   f_err,
           "command processor error"                  }
};
/*  */
char *errtab[] = {
/* 0 */ 	"Unrecognized command\n",
/* 1 */ 	"Ambiguous command\n",
/* 2 */ 	"Command error\n",
/* 3 */         "Wrong number of arguments\n",
/* 4 */         "Command argument too long\n",
/* 5 */         "Bad specification\n",
/* 6 */         "Can't create file\n",
/* 7 */         "File not found\n"
};
/*  */
ftpmain (otherhalf)
int otherhalf;
{
    register struct ftucmd *sp;
    extern struct ftucmd *getcmd();

	NetInit (&dsfdr);
	NetInit (&dsfds);
	init (otherhalf);

	if (!setjmp (env))
	    rwait (1);			/* wait for 200 hello */

	for (;;)
	{
	    if (uicount) abterr();       /* Returns to setjmp() */
		prompt();
		sp = getcmd();
		putcmd (sp->ft_cnm);
		(*sp->ft_fn)();
	}
}

prompt()
{
	printf ("> ");
}
/*  */
init (otherhalf)
int otherhalf;
{   /* signal handling changed jsq BBN 5Aug79 */
    extern exit(), usrint(), dieinit(), diequit();

	dieinit (otherhalf, stderr);  /* second arg is fd for fprintf */
	get_stuff (&NetParams);
	
	if (signal (SIGINT, usrint) == SIG_IGN)
		signal (SIGINT, SIG_IGN);/* if parent shell */
	if (signal (SIGQUIT, diequit) == SIG_IGN)
	       signal (SIGQUIT, SIG_IGN);/* ignored, we do too */
	signal (SIG_NETINT, SIG_IGN);

	inittime();
	ttcnt = 0;
	ttptr = ttbuf;
#ifdef  MGET
	*mretname = 0;
#endif  MGET
}
/*  */
logfn(s)			/* interfaces to ftp_lib, skips over reply
				   code.  */
char * s;
{
    printf (s+4);
}

usrint()        /* Collect user interrupts */
{
	uicount++;
	abrtxfer++;		/* stop any transfer in progress */
	signal (SIGINT, usrint);
}

f_err()
{
    puts ("ftpmain: command parser ERROR; dispatch on 0 command pointer");
    puts ("please report this to BUG-FTP @ BBN-Unix");
}
/*  */
help()
{
 register struct ftucmd *sp;
 register i,j;
 char flag;

 flag = i = j = 0;
 if (*arg1==0)
 {
	printf ("\nCommands known to this process:\n\n");
	for (sp = cmdtab; (sp->ft_cnm); sp++)
	{
	       printf ("%-*.*s", MAXSTRING, MAXSTRING, (sp->ft_cnm));
	       if (!(++j%6))printf ("\n");
	}
	printf ("\nany unambiguous substring will invoke the given command\n");
	printf ("for a brief description of a given command type \"help <command>\"\n");
	printf ("for a brief description of all commands type \"help all\", or \"help * \"\n");
	printf ("\"help server\" will ask the server process for help\n");
 }
 else {
	getuc (arg1);
	if (match (arg1, "SERVER"))
	{
		putcmd ("HELP");
		sndcmd();
		rwait (12);
		return;
	}
	else if (match (arg1, "ALL")||match (arg1, "*")) flag++;

	for (sp = cmdtab; (sp->ft_cnm); sp++)
	{
		if (flag||match (arg1, sp->ft_cnm))
		{
			printf ("%s\t %s\n", sp->ft_cnm, sp->ft_desc);
			i++;
		}
	}
	if (!i) printf ("command \"%s\" unrecognised\n", arg1);
 }
}
/*  */
f_mode()
{
	setspec ("Mode", &mode, mdcode);
}

f_type()
{
	setspec ("Type", &type, tycode);
}

f_stru()
{
	setspec ("Structure", &stru, strucode);
}

setspec (what, spec, tabl)
char *what; int *spec;
struct spctab tabl[];
{
    register struct spctab *p;

	getuc (arg1);
	for (p=tabl; p->typnm; p++)
	{
	    if (match (arg1,p->typnm)) break;
	}

	if (!p->typarg)
	   error
	   ( (p->typnm
		? "%s %s not implimented.\n"
		: "Bad %s: \"%s\"\n"
	     ), what, arg1
	   );
	else
	{
	    putstr (p->typarg);
	    sndcmd();
	    if (rwait (2) == 0) return;
	    *spec = p->typnum;
	}
}
/*  */
f_status()
{
	putstr (arg1);
	sndcmd();
	rwait (5);
	sleep (5);
}

f_rena()
{
	putcmd ("RNFR");
	putstr (arg1);
	sndcmd();
	if (rwait (2), xreply != 350) return;
	putcmd ("RNTO");
	putstr (arg2);
	sndcmd();
	rwait (6);
}
/*  */
f_cld()
{
	if (chdir (arg1)<0)
		printf ("%s; can't change to local dir %s\n",
		   errmsg (0), arg1);

}

f_cwd()         /* makes this compatible with the MSTO stuff */
{
 cwd (arg1);
}

cwd (str)
char *str;
{
	putcmd ("CWD "); putstr (str);    /* put destination in */
	sndcmd();
	return
	    (
		rwait (3), xreply != 251
		? -1
		: 0
	    );				    /* used by mstore() */
}
/*  */
f_rate()
{
    printf(dumptime());
    if (!strcmp("reset", arg1))
	inittime();
}

f_quot()
{
	putcmd (arg1);
	if (arg2[0]) putstr (arg2);
	sndcmd();
	rwait (-1);
}

f_dele()
{
	putstr (arg1);
	sndcmd();
	rwait (7);
}


f_user()
{
	putstr (arg1);
	sndcmd();
	rwait (1);
}
/*  */
f_log()
{
	putcmd ("USER");
	f_user();
	if (xreply == 331)
	    {
		putcmd ("PASS");
		strcpy (arg1,arg2);
		f_pass();
	    }
	if (xreply == 332)
	    {
		strcpy (arg1, arg3);
		f_acct();
	    }
}
/*  */
f_pass()
{
	secret ("Password: ");
}

f_acct()
{
	putcmd ("ACCT");
	secret ("Account #: ");
}


secret (s)
  char *s;
{
    struct sgttyb tmp;
    register int sav;
    register char *p;

	if (arg1[0] == '\0')
	{
	    ttcnt = 0;
	    gtty (1,&tmp);
	    sav = tmp.sg_flags;
	    tmp.sg_flags &= ~ECHO;
	    stty (1,&tmp);                /* Set noecho for password */
	    printf ("%s",s);
	    fflush(stdout);
	    p = getarg();
	    tmp.sg_flags = sav;
	    stty (1,&tmp);
	    printf ("\n");           /* Echo the absorbed newline */
	    if (p)
	    {
		movarg (p,arg1,ARGSIZ-1);
		getarg();               /* Skip the newline */
	    }
	}
	putstr (arg1);
	sndcmd();
	rwait (1);
}
/*  */
fetch (src,dest)
char    *src, *dest;
{
	int flag;
	int tfd;

/* First find out if the destination file already existed */

	if ((tfd = open (dest, 1)) == -1)
	    flag = 1;	/* Delete on error */
	else
	{
	    close (tfd);
	    flag = 0;	/* Don't delete on error, it already existed */
	}

	if ((fdfil = fopen (dest, "w")) == NULL)
		error ("Can't create \"%s\"; %s\n", dest, errmsg (0));
	errno = 0;
	putstr (src);
	sndcmd();

	if (!chekds (0))
	{
	    rcvdata (&dsfdr, fdfil, logfn);
	    net_close (&dsfdr);
	    fclose (fdfil);
	    fdfil = NULL;
	    return rwait (4);
	}

	fclose (fdfil);
	fdfil = NULL;
	if (flag)
	    unlink (dest);
	net_close (&dsfdr);
	if (errno == EINTR) abterr();
	return 0;
}
/*  */
f_nlst()
{
	nlst (arg1,arg2);
}

nlst (src,dst)                   /* called by f_mret(), f_nlst() */
char *src,*dst;
{
	putcmd ("NLST");
	return fetch (src,dst);
}


f_list()
{
	fetch (arg1,arg2);
}

f_retr()
{
	putcmd ("RETR");         /* because get synonym might be given */
	fetch (arg1,arg2);
}
/*  */
f_fsnd()
{
    if ((fdfil = popen (arg1, "r")) == NULL)
    {
	error (badcmd, arg1);
    }
    else
    {
	errno = 0;
	putcmd ("STOR");
	putstr (arg2);
	sndcmd ();

	    if (chekds (1))
	    {
		pclose (fdfil);
		fdfil = NULL;
		net_close (&dsfds);
		if (errno == EINTR)
		    abterr ();
	    } else
	    {
		    if (senddata (fdfil, &dsfds, logfn))
			net_pclose (&dsfds);
		    else
			net_close (&dsfds);
		    rwait (4);
	    }
	if (fdfil != NULL)
	{
	    pclose (fdfil);
	    fdfil = NULL;
	}
	return 0;
    }
    return -1;
}
/*  */
f_fget()
{
	if ((fdfil = popen(arg2, "w")) == NULL)
	    error(badcmd,arg2);
        else
	{
	    errno = 0;
	    putcmd("RETR");
	    putstr(arg1);
	    sndcmd();
		if (chekds (0))
		{
		    pclose (fdfil);
		    fdfil = NULL;
		    net_close (&dsfdr);
		    if (errno == EINTR)
			abterr ();
		}

		rcvdata (&dsfdr, fdfil, logfn);
		net_close (&dsfdr);
	    pclose (fdfil);
	    fdfil = NULL;
	    return rwait (4);
	}
	return -1;
 }
/*  */
f_stor()
{
    store (arg1, arg2);
}

store (s1,s2)
char *s1, *s2;
{
	sndallo (s1, 0);
	putcmd ("STOR");
	snddat (s2, 0);
}


f_appe()
{
	sndallo (arg1, 0);
	putcmd ("APPE");
	snddat (arg2, 0);
}
/*  */
sndallo (s, extra)
  char *s;
  unsigned extra;
{
        long size;
	char sizestr[20];
	if ((fdfil = fopen (s,"r")) == NULL)
		error ("can't open \"%s\"; %s\n", s, errmsg (0));
	if (fstat (fileno(fdfil), &sttbf) == -1)
		error ("File \"%s\" not found; %s\n", s, errmsg (0));
	size = 2*sttbf.st_size+extra;
	putcmd ("ALLO");
	sprintf (sizestr, "%D", size);
	putstr (sizestr);
	sndcmd();
	if (rwait (2) == 0 && xreply < 500)       /* Some hosts don't like ALLOs */
	{
	    fclose (fdfil);
	    fdfil = NULL;
	    longjmp (env,1);
	}
}

/*  */
snddat (s, mlflg)
  char *s;
  int mlflg;
{
    int child;

    errno = 0;
    putstr (s);
    sndcmd ();
	if (chekds (1))
	{
	    fclose (fdfil);
	    fdfil = NULL;
	    net_close (&dsfds);
	    if (errno == EINTR) 
		abterr ();
	} else 
	{
		if (mlflg)
		    net_write (&dsfds, mlbf, strlen (mlbf));
		if (senddata (fdfil, &dsfds, logfn))
		    net_pclose (&dsfds);
		else net_close(&dsfds);
		rwait (4);
	}
	    if (fdfil != NULL)
	    {
		fclose (fdfil);
		fdfil = NULL;
	    }
		
}
/*  */
f_mlfl()
{
	setmlnm();
	putstr ("TYPE A"); sndcmd();
	if (rwait (2) == 0) return;
	type = TYPEA;
	putstr ("MODE S"); sndcmd();
	if (rwait (2) == 0) return;
	mode = MODES;
	if ((fdfil = fopen (arg1,"r")) == NULL)
		error ("can't open \"%s\"; %s\n", arg1, errmsg (0));
	putcmd ("MLFL");
	snddat (arg2, 1);
}
/*  */
f_mail()
{
    register char *p, *q;

	if (arg1[0] == 0)
	{
	    printf  (persn);
	    if (p = getarg())
	    {
		movarg (p,arg1,ARGSIZ-1);
		getarg();
	    }
	}
	putstr (arg1);
	sndcmd();
	if (rwait (9), xreply != 354) return;
	setmlnm();
	putstr (mlbf); sndcmd();
	ttptr = ttbuf;
	for (;;)
	{
	    if ((ttcnt = strlen(fgets (ttbuf,TTBSIZ, stdin))) <= 0) break;
	    if (ttbuf[0] == '.' && ttbuf[1] == '\n')
	    {
		ttptr = &ttbuf[2];
		ttcnt -= 2;
		break;
	    }
	    p = ttbuf;
	    q = linbuf;
	    while (ttcnt--)
		if (*p == '\n')
		{
		    linptr = q;
		    sndcmd();
		    q = linbuf;
		    p++;
		}
		else *q++ = *p++;
	}

	linbuf[0] = '.';
	linptr = &linbuf[1];
	sndcmd();
	rwait (10);
}
/*  */
subshl()
{   /* changed to handle signals, exit, and wait properly jsq BBN 4Aug79*/
    int ps, pid, i;
    int (*oldemt)(), (*oldint)(), (*oldquit)();
    extern usrint();

	if ((pid = VFORK()) == 0)
	{
	    close (TTYMON);
	    net_vclose (&NetParams);
	    net_vclose (&dsfds);
	    net_vclose (&dsfdr);
	    signal (SIGEMT, SIG_DFL);
	    execl ("/bin/sh","ftp-sh",0);
	    fprintf (stderr, "/bin/sh missing.\n");
	    VEXIT (-1);
	}

	oldemt  = signal (SIGEMT, SIG_IGN);
	oldquit = signal (SIGQUIT, SIG_IGN);
	oldint  = signal (SIGINT, SIG_IGN);

	while (wait (&ps) != pid);

	signal (SIGEMT, oldemt);
	signal (SIGQUIT, oldquit);
	signal (SIGINT, oldint);
}
/*  */
rget()
{
    int i;

    errno = 0;
    if ((i = read (pipnum, &xreply, sizeof xreply /*, 1, fdpip*/)) <= 0)
	{
	    if (uicount) 
	    {
		uicount--;
		xreply = 0;
		longjmp(env,1);
	    }
	    if ((i<0) || (errno != 0)) abort();
	    die (1, "%s:  Host exited; %s\n", progname, errmsg (0));
	}
    return xreply;
}

#ifdef MGET
getdirname()			/* get name of created directory */
{
	int i, j;

	*MadeDirectory = '\0';
	i = read (pipnum, &j, sizeof j /*, 1, fdpip*/);
	if (i>0) i = read (pipnum, MadeDirectory, j /*, 1, fdpip*/);
	if (*MadeDirectory == '\0') return (0);
	MadeDirectory[j] = '\0'; /* convert to real string */
	return (1);
}
#endif MGET
/*  */
rwait (n)
int n;
{
    register int    rnum;

    while ((rnum = rget ()) < 200)
	if (n < 0) break;		/* special hack to return low reply */

    if (rnum == 421) byedie();

#   ifdef MGET
	if (rnum == 251) return getdirname();
#   endif MGET

    if (rnum < 200 && n < 0) return 0;	/* hack for now. */

    switch (rnum / 100)
    {
	case 1:
	case 2:
	    return 1;

	case 3:
	case 4:
	case 5:
	    return 0;

	default:
	    abort ();
    }
}
/*  */
byedie()
{
	putcmd (QUIT);
	sndcmd();
	rwait (1);
	printf(dumptime());
	die (0, NULL);
}

abterr()
{
	register char *p, *q;

	uicount = 0;
	q = linbuf;
	p = aborstr;            /* <IAC> <IP> <IAC> <DM> ABOR */
   	while (*p)
		*q++ = *p++;
	linptr = q;
	urgon (&NetParams);       /* send INS to match DM */
	sndcmd();
	urgoff (&NetParams);       /* send INS to match DM */
	if (fdfil != NULL)
	{
	    fclose (fdfil);
	    fdfil = NULL;
	}
	while (rwait (8) && xreply!= 225 && xreply!=226);
#ifdef MGET
	if (*mretname)
	{
		unlink (mretname);
		*mretname = 0;
	}
#ifdef MGET
	longjmp (env,1);
}
/*  */
/* VARARGS */
error (n)
  char *n;
{

	ttcnt = 0;
	ttptr = ttbuf;
	printf ("%r", &n);
	fflush(stdout);
	if (fdfil != NULL)
	{
	    fclose (fdfil);
	    fdfil = NULL;
	}
	longjmp (env,1);
}

putcmd (s)                       /* put in 4 chars of cmd */
  char *s;
{
    register char *p, *q;
    register int i;

	p = linbuf;
	q = s;
	for (i=0; i<4; i++) *p++ = *q++;
	linptr = p;
}

putarg (s)                       /* put in argument with no space */
  char *s;
{
    register char *p, *q;
	p = linptr;     q = s;
	while (*q) *p++ = *q++;
	linptr = p;
}


putstr (s)                       /* put in space, then arg */
  char *s;
{
	*linptr++ = ' ';
	putarg (s);
}
/*  */
struct ftucmd *getcmd()
{
    register struct ftucmd *sp;
    register char *p;

	while ((p = getarg()) == 0) prompt();
	movarg (p,arg1,MAXSTRING);
	getuc (arg1);
	for (sp = cmdtab; (sp->ft_cnm); sp++)
	{
	    if (match (arg1, sp->ft_cnm)) goto win;
	}
	error (errtab[0]);
    win:
	if ((sp+1)->ft_cnm && match (arg1, (sp+1)->ft_cnm)) error (errtab[1]);
	if ((p = getarg()) == 0)
	    switch (sp->ft_nargs)
	    {
		case ARG0OR1:
		    arg1[0] = '\0';
		case ARG0:
		    return (sp);
		case ARG1:
		case ARG2:
		case ARG1AND2:
		case ARG1OR2:
		case ARG1OR2OR3:
		    printf ((sp->ft_info)[0]);
		    if ((p = getarg()) == 0) error (errtab[2]);
	    }
	else if (sp->ft_nargs == ARG0) error (errtab[3]);
	movarg (p, arg1, ARGSIZ-1);

	if ((p = getarg()) == 0)
	    switch (sp->ft_nargs)
	    {
		case ARG1AND2:
		    printf ((sp->ft_info)[1]);
		    if (p = getarg()) break;
		case ARG1OR2:                   /* arg 2 is optional */
		case ARG1OR2OR3:
		    arg2[0] = '\0';
		case ARG1:
		case ARG0OR1:
		    return (sp);
		case ARG2:
		    printf ((sp->ft_info)[1]);
		    if ((p = getarg()) == 0) error (errtab[2]);
	    }
	else if (sp->ft_nargs & (ARG1|ARG0OR1)) error (errtab[3]);
	movarg (p, arg2, ARGSIZ-1);
	if (p = getarg())
	    {
		if (sp->ft_nargs == ARG1OR2OR3)
			movarg (p,arg3,ARGSIZ-1);
		else error (errtab[3]);
	    }
	else arg3[0] = '\0';
	return (sp);
}
/*  */
movarg (s, buf, maxch)
  char *s, *buf;
  int maxch;
{
    register int i;
    register char *p, *q;
    char quote;

	p = s;  q = buf;   i = 0;
	if (*p == '"' || *p == '\'')
	{
	    quote = *p++;
	    i += 2;
	    maxch += 2;
	}
	else quote = '\0';
	if (quote) do if ((*q++ = *p++) == quote) goto out; while (i++ < maxch);
	else do if (issep (*q++ = *p++)) goto out; while (i++ < maxch);
	error (errtab[4]);
    out:
	*--q = '\0';
	ttcnt -= i;
	ttptr += i;
}
/*  */
char *getarg()
{
	for (;;)
	{
	    if (ttcnt <= 0)
	    {
		if (fgets (ttbuf, TTBSIZ, stdin) == NULL)
		    ttcnt = 0;
		else
		    ttcnt = strlen (ttbuf);
		if (ttcnt <= 0)
		{
		    if (uicount) longjmp (env,1);
		    byedie();
		}
		ttptr = ttbuf;
	    }
	    while (ttcnt-- > 0)
		switch (*ttptr++)
		{
		    case '\n':
			return (0);
		    case ' ':
		    case '\t':
			continue;
		    default:
			++ttcnt;
			return (--ttptr);
		}
	}
}
/*  */
match (s1, s2)
  char *s1, *s2;
{
    register char *p1, *p2;

	p1 = s1;  p2 = s2;
	while (*p1 == *p2++ && *p1) p1++;
	return (*p1 ? 0 : 1);
}

issep (c)
  char (c);
{
	return (!c) || isspace (c);
}

/*  */
setmlnm()
{
    register int c;
    FILE *ib;
    register char *p;
    char *q, ruid, noflg, pbuf[120], xbuf[10];

	ruid = getuid();
	if (getpw (ruid, pbuf)) pbuf[0] = 0;
	p = pbuf;       q = xbuf;
	while (*p && *p != ':') *q++ = *p++;
	*q = '\0';
	if ((ib = fopen ("/change/users", "r")) == NULL) noflg = 0;
	else for (;;)
	{
	    p = pbuf;
	    while ((c = getc (ib)) >= 0)
	    {
		if (c == '\n') break;
		*p++ = c;
	    }
	    *p = '\0';
	    if (c < 0)
	    {
		noflg = 0;
		break;
	    }
	    p = pbuf;
	    while (issep (*p) == 0) p++;
	    *p = '\0';
	    p = pbuf; q = xbuf;
	    while ((c = *q++ - *p) == 0 && *p++);
	    if (c > 0) continue;
	    if (c < 0) noflg = 0;
	    else noflg = 1;
	    break;
	}
	if (noflg)
	{
	    while (issep (*p++));
	    q = p - 1;
	    p = &pbuf[39];
	    while (issep (*p--));
	    *(p+2) = '\0';
	}
	mlptr = mlbf;
	mlstr ("From: ");
	mlstr (xbuf);
	if (noflg)
	{
	    mlstr (" (");
	    mlstr (q);
	    mlstr (")");
	}
	mlstr (" at ");
	mlstr (HOST_ID);         /* AGN */
	mlstr ("\r\n\r\n");	/* Blank line for end of header */
	*mlptr = '\0';
}
/*  */
mlstr (s)
  char *s;
{
    register char *p, *q;

	p = s;  q = mlptr;
	while (*q++ = *p++);
	mlptr = --q;
}

#ifdef MGET

#define SUCCESS 1
#define FAIL    0
#define ERROR   -1
#define CRLF    "\r\n"
#define NLIST   1       /* flags to next_filstr to tell it what its   */
#define DIRLIST 2       /* looking at--either a local directory or a nlst */
			/* from the server                                */

f_mkd()
{
    mkd (arg1);
}

mkd (s)
char *s;
{
    putcmd ("XMKD");
    putstr (s);
    sndcmd();
    return (rwait (13) ? -1 : 0);	/* make sure the directory is created */
}

f_rmdir()
{
    putcmd ("XRMD");
    putstr (arg1);
    sndcmd();
    rwait (7);
}
/*  */
f_msto()
{
    char dirplate[ARGSIZ]; /* if the wild-carded string includes a path */
    char template[ARGSIZ]; /* where to store the wild-carded string */
    char remroot[ARGSIZ]; /* where to store the foreign root */
    int ps, child, dirflg, i;
    register char *p, *lastslash;

    recurstor = ps = dirflg = 0;
    /* copy the args to a safe place */
    movarg (arg1, dirplate, ARGSIZ-1);

    if (arg2[0] != '\0')
    {
	movarg (arg2, remroot, ARGSIZ-1);
	recurstor++;
    }

    /* now, scan the first argument, and if it is a pathname rather than a */
    /* file in this directory, juggle the books, a bit.                    */
    for (p = lastslash = dirplate; *p; p++)
	if (*p == '/')
	{
		lastslash = p;
		dirflg++;
	}

    if (dirflg)
	*lastslash++ = '\0';    /* terminate the directory string */
    movarg (lastslash, template, ARGSIZ-1);
    if (dirflg)
    {
	if (*dirplate == '\0')	/* oops, a file in the root directory... */
	{
		dirplate[0] = '/';
		dirplate[1] = '\0';
	}
    }
    if (recurstor||dirflg)
    {
	 /* done in a lower fork so we can chdir freely */
	 if ((child = fork()) == 0)
	 {
		signal (SIGINT, SIG_DFL); /* let interrupts clobber child */
		progname = "ftpmain_child";
		if (dirflg) if (chdir (dirplate)<0)
		{
			printf ("%s: can't get to \"%s\"; %s\n",
				progname, dirplate, errmsg (0));
			exit (-1);
		}
		sendabunch (template,remroot);
		printf(dumptime());
		exit (0);
	} else if (child < 0)
	{
		printf ("%s: can't fork; %s\n", progname, errmsg (0));
	} else {
		errno = 0;
		while ((i = wait (&ps))!=child && i>0)
			{ ; }
		if (errno == EINTR)
		{
			kill (child, 9); /* make sure the child does indeed die, in */
				/* case it was ignoring interrupts at the  */
				/* time (see the checkds()) routine        */
		}
	}
    }
    else sendabunch (template,NULL);

    recurstor = dialog = 0;
}
/*  */
sendabunch (str,foroot)

/* basically for sending a whole directory (masked by template) full of */
/* stuff. */

char *str, *foroot;
{
    register FILE *dfds;      /* fds for the directory */
    register int i;
    struct stat statbuf;
    struct
    {
	int inode;
	char name_ptr[15];      /* extra one for a null at the end */
    } file;

    if (recurstor)
    {
	if (chdir (str)<0)
	{
		printf ("%s; can't chdir to \"%s\"\n", errmsg (0), str);
		return;
	}
	printf ("make directory \"%s\"\n", ((foroot==NULL)?foroot:str));
	if (mkd ((foroot==NULL)?foroot:str)<0)
	{
		printf ("making directory %s failed\n", str);
		chdir ("..");    /* undo the effects of the chdir above */
		return;
	} else if (cwd (desyntactify (MadeDirectory))<0)
	{
		printf ("moving to directory \"%s\" failed\n", str);
		chdir ("..");    /* undo the effects of the chdir above */
		return;
	}
 }

    /* open up the directory, and iterate through its contents, masking */
    /* each with template to determine whether it should be asked for  */
    if ((dfds = fopen (".", "r")) == NULL)
    {
	char dot[80];   /* place for abspath to stringify */
	abspath (".", dot, &dot[sizeof (dot)]);
	printf ("can't open directory \"%s\"; %s\n", dot, errmsg (0));
	return;
 }
/*  */
  /* skip the directory over the '.' & '..' entries */
  fseek (dfds,(long)(2*(sizeof file)), 0);
  file.name_ptr[14] = '\0';      /* make sure there's a null on the end */
  /* iterate through the directory */
  while ((i = fread (&file, sizeof file, 1, dfds)) <= 0)
  {
    if (file.inode != 0)
    {
	sleep (1);       /* allow time for socket to be fully closed */
	if (uicount)
	{
		recurstor = 0;
		fclose (dfds);
		send();
		abterr();
	}
	if (recurstor)
	{
		/************************************************************/
		/* note that an interrupt here does not require us to clean */
		/* up all the files we have openned, as interrupts are not  */
		/* caught, and this child process simply exits...           */
		/************************************************************/
		stat (file.name_ptr, &statbuf);
		if (statbuf.st_mode&S_IFDIR)
		{
			/* send the command to make the directory, &
			/* ship the directory */
			/* recurse into the subdirectory */
			sendabunch (file.name_ptr,NULL);
		}
		else{
			printf ("store \"%s\" as \"%s\"\n",file.name_ptr, file.name_ptr);
			store (file.name_ptr,file.name_ptr);
		}
	}
	else if (glob (str, file.name_ptr) == SUCCESS)
	{
		printf ("store \"%s\" as \"%s\":\n", file.name_ptr, file.name_ptr);
		store (file.name_ptr,file.name_ptr);
	}
    }
 }
/*  */
    if (i == NULL)
    {
	char dot[80];   /* place for abspath to stringify */
	abspath (".", dot, &dot[sizeof (dot)]);
	printf ("error reading directory \"%s\"; %s\n", dot, errmsg (0));
    }

    /* if nothing left in this directory, change back to parent */
    fclose (dfds);
    if (recurstor)
    {
	putcmd ("XCUP");
	sndcmd();
	rwait (2);
	chdir ("..");
    }
}
/*  */
char *
desyntactify (str)       /* strip the quotes from the returned */
char *str;              /* directory.  NOTE: acts directly on its argument */
{
	register char *p, *q;

	q = p = str;
	while (*p)
	{
		if (*p == '"') p++;
		*q++ = *p++;
	}
	*q = '\0';
	return (str);
}
/*  */
glob (tmpl,str)
char *str;
char *tmpl;
 /* str is a candidate to match tmpl.  tmpl contains junk in the normal
    glob-type format, e.g. '*' matches anything in str, '?' matches any
    single char, and "[...]" in tmpl gives an acceptable set of guys in
    str to match, "[...a-z...]" matches any char between a & z.
  */
{
	register char *t, *s;

	t = tmpl; s = str;

	while (*s && *t == *s ) { t++; s++; }
	if (*t == '\0' && *s == '\0') return (SUCCESS);

	switch (*t++)
	{   /* tmpl & str have diverged; if it is because *tmpl is a */
			/*  globular character, then do the appropriate thing.   */
			/*  otherwise, return FAIL                               */
	 case '*':
	   /****************************************************************/
	   /* algorithm for '*':                                           */
	   /*  if we've used up string,                                    */
	   /*     if there's nothing left to tmpl, success.                */
	   /*     if tmpl expects more, fail.                              */
	   /*  otherwise, recurse on the stuff after the asterisk in tmpl  */
	   /*   and str++                                                  */
	   /****************************************************************/
		while (1)		/* mung */
		{
			if (*t == '\0') return (SUCCESS);
			if (*s == '\0')
			{
				if (*t != '\0') return (FAIL);
				return (SUCCESS);
			}
			if ((glob (t, s++)) == SUCCESS) return (SUCCESS);
		}

	 case '?':
		return (glob (t, ++s));

	 case '[':
		if (one_of (t, *s++) == SUCCESS)
		{
			while (*t && *t++ != ']');
			return (glob (t, s));
		}
		return (FAIL);

	 default:
		return (FAIL);

	 }
}
/*  */
one_of (tmpl, ch)
char *tmpl;
int ch;
/* [...a-z...] handler */
{
 register char *t;

 t = tmpl;

 while (*t)
 {
	switch (*t)
	{
	 case ']':
		return (FAIL);

	 case '-':
		if (ch >= *(t-1) && ch <= *(t+1) && *(t+1) != ']')
			return (SUCCESS);
		goto next;

	 default:
		if (ch == *t) return (SUCCESS);
	}
 next:  t++;
 }
 return (ERROR);
}
/*  */
f_mret()
{
    char    name[256];
    char    dstn[256];
    FILE   *mretfile;

 /* generate a unique filename */
    sprintf (mretname, "/tmp/ftp=%d.%d", getpid (), getuid());
    if(mretfile != NULL)	/* if an earlier mret was interrupted, then */
    {
	fclose(mretfile);	/* this file already exists... */
	unlink(mretname);
    }

    if (fnedinit (arg2) < 0)
	return;

    printf ("getting list of names from server....\n");
    if (nlst (arg1, mretname) == 0)
    {
	printf ("nlst failed\n");
	unlink (mretname);
	abterr ();
	return;
    }
    if ((mretfile = fopen (mretname, "r")) == NULL)
    {
	printf ("can't open temporary file \"%s\"; %s\n",
		mretname, errmsg (0));
	unlink (mretname);
	return;
    }

    while (mretfile != NULL)	/* until mretfile is exhausted */
    {
	sleep (20);		/* give system enough time to clean up
				   from */
	if (uicount)		/* the last transfer */
	{
	    fclose (mretfile);
	    unlink (mretname);
	    mretfile = NULL;
	    abterr ();
	}
	if (fgets (name, sizeof name, mretfile) != NULL)
	{
	    name[strlen(name)-1] = '\0';	/* backup over newline */
	    if (fned (name, dstn, sizeof (dstn)) < 0)
	    {
		printf ("\"%s\" not retrieved\n", name);
	    }
	    else
	    {
		if (!(fnflag & FNASK))
		    printf ("retrieve \"%s\" as \"%s\"\n", name, dstn);
		putcmd ("RETR");
		fetch (name, dstn);
	    }
	}
    }

    fclose (mretfile);
    unlink (mretname);
    *mretname = 0;
}
/*  */
char * mrethlp[] =
{
  "'?' prints this informatio;n'-flags' sets up a string-processor to",
  "transform a foreign file-name into something Unix would understand",
  "\nFlags to set up file-name processor:\n",
  "  T - Tenex/Tops20-mode: strips the <...> directory, and the protection",
  "      information from the filename; also it lowers the case of the",
  "      letters in the file-name, removes control-V's, and replaces",
  "      Tenex's semi-colons before the version number with periods",
  "  I - ITS mode: strips the \"DSK: DIR;\" from the beginning of the",
  "      filename, lowers the case of the letters in the filename, and",
  "      replaces spaces with '_'",
  "  M - Multics mode: not implimented yet",
  "  a - alphanumerics only: non-alphanumeric letters in the remote",
  "      filename are stripped (NOTE: '.' is regarded as alphabetic)",
  "  l - convert uppercase to lower-case",
  "  p/foo/ -",
  "      prefix the string 'foo' to each file-name ('\\' will escape '/')",
  "      this flag may be combined with any other",
  "  ? - rather than trying to mold the remote filename into a Unix",
  "      filename, just ask the user for a name to store this file",
  NULL
};
/*  */
fnedinit (key)
char *key;
{
    register char  *p;
    register char ** q;

    fnflag = 0;
    *fnprefix = '\0';
    if (!*key)
    {
	fnflag |= FNCPY;
	return (1);
    }
    if (*key == '?')
    {
	for (q=mrethlp; *q != NULL;  q++)
		puts(*q);
	return (-1);
    }
    if (*key++ != '-')
    {
	printf ("Flags must begin with hyphens\n");
	return (-1);
    }
    while (*key)
    {
	switch (*key++)
	{
	    case 'T': 
		fnflag |= FNTENEX | FNTOLCASE;
		break;
	    case 'I': 
		fnflag |= FNITS | FNTOLCASE;
		break;
	    case 'l': 
		fnflag |= FNTOLCASE | FNCPY;
		break;
	    case 'a': 
		fnflag |= FNALPHA | FNCPY;
		break;
	    case '?': 		/* asking precludes all others */
		*fnprefix = '\0';
		fnflag = FNASK;
		return 0;
	    case 'p': 
		fnflag |= FNCPY;
		p = fnprefix;
		if (*key++ != '/')
		{
		    printf ("no '/' bordering prefix\n");
		    return (-1);
		}
		while (*key && *key != '/')
		{
		    if (*key == '\\')
			key++;
		    *p++ = *key++;
		}
		if (*key)
		    key++;
		else
		    return (1);
		*p = '\0';
		break;
	    default: 
		printf ("bad flag: '%c'\n", *(--key));
		return (-1);
	}
    }
    return (0);
}
/*  */
fned (src, dest, destsize) /* file-name editor -- does transformations on file-names */
char *src, *dest;
{
    register char *p, *d;
    register int c;

	d = dest;
	/* enter prefix */
	if (*(p = fnprefix)) while (*p && d < &(dest[destsize-1])) *d++ = *p++;
	if (fnflag & FNASK)
	{
		printf ("retrieve \"%s\" as: ", src);
		if ((p = getarg()) <= 0) /* ignore newline */
			if ((p = getarg()) <= 0) return (-1);
		movarg (p, d, (destsize-1));
		return (1);
	}
	p = src;
	if (fnflag & FNTENEX)
	{
		/* strip the leading directory, and the trailing protection */
		/* first, the directory, if its there... */
		if (*p == '<')
			while (*p && *p++ != '>')
				;
		/* look for the third dot-or-semicolon */
		c = 0;
		while (*p && d < &(dest[destsize-1]))
		{
			if ((*p == ';' || *p == '.') && c++ >= 2) break;
			else {  /* control-V is a quoting character */
				if (*p == CTLV) p++;
				if (*p == ';')
				{
					p++;
					*d++ = '.';
				} else *d++ = *p++;
			}
		}
	} else if (fnflag & FNITS)
	{
		while (*p && *p++ != ';')
			;
		do {
			if (*p == ' ') *d++ = '_';
			else *d++ = *p;
		} while (*p++ && d < &(dest[destsize-1]));

	} else if (fnflag & FNCPY)
		 while (*p && d < &(dest[destsize-1])) *d++ = *p++;
	*d = '\0';

	/* now do individual character transformations */
	d = dest;
	/* skip past the prefix */
	if (*(p = fnprefix)) while (*p++) d++;
	do {
		if ((fnflag & FNALPHA)
		     && !(isdigit (c = *d) || isalpha (c) || c == '.'))
		{
			p = d;
			do {                    /* flush this character */
				*p = *(p+1);    /* bump up the rest */
			} while (*p++);
		}
		if ((fnflag & FNTOLCASE) && (isupper (*d)))
			*d++ = tolower (*d);
		else d++;
	} while (*d);
	return (1);
}
#endif  MGET
/*  */
#define IOBSIZ  120
char ibuf[IOBSIZ], *iptr;
int icnt = 0;
int cmdflg = 0;
extern int synchno;

int ins();

ftp_printer(otherhalf)
int otherhalf;          /* the arguments to this process */
{
    register char *p;
    int n;

    extern exit(), diequit(), dieinit();
    extern char *atoiv(), *get_dirstr();

	dieinit(otherhalf, stdout);
	signal(SIGINT, SIG_IGN);	/* Ignore interrupt--for main FTP */
	signal(SIGQUIT, SIG_IGN);	/* Ignore quit, so quit in inferior
					shell of ftpmain works. */
	signal(SIGURG,ins);
	fclose(stdin);
	fclose(stdout);
	for (;;)
	{
	    linein();
	    n = iconv(linbuf);
	    fputs(linbuf, stderr);
	    if (n)
	    {
		if (write(pipnum, &n, sizeof n) != sizeof(n))
			die(1, "%s:  can't write reply-code to ftpmain; %s\n",
			    progname, errmsg(0));
		if (n == MADEDIR)
		{
		    if (p = get_dirstr(linbuf))
		    {
			n = strlen(p);
			write(pipnum, &n, sizeof n);
			write(pipnum, p, n);
		    }
		    else	   /* write a 0 if no directory string */
		    {
			n = 0;
			write(pipnum, &n, sizeof n);
		    }
		}
	    }
	}
}

iconv(s)
  char *s;
{
    register int k, i, c;

	k = 0;
	for (i = 0; i < 3; i++)
	{
	    c = s[i];
	    if(c < '0' || c > '9') return(0);
	    k = k*10 + c - '0';
	}
	if(s[3] == '-') return(0);      /* this is a continued comment */
	return(k);
}

char *get_dirstr(s)
char *s;
{
     register char *p, *q;

	q = p = &(s[4]);
	if (*p++ != '"') return 0;
	while (*p)
	{
	    if (*p++ == '"')	/* check if this is an escaped quote */
		{
		    if (*p == '"')
			{ p++; continue; }
		    else		/* not an escaped quote, return */
			{
			    *p = '\0';
			    return q;
			}
		}
	}
	*p = '\0';
	return q;
}
/*  */
linein()                                /* Get a line into linbuf */
{
    register int c, ovflg;
    register char *linptr;
    int retflg;

	linptr = linbuf;
	retflg = ovflg = 0;

	for (;;)
	{
	    if(linptr >= (linbuf + LINSIZ - 2)) ovflg++;
	    c = getch();
	    if(cmdflg)
	    {
		cmdflg = 0;

		/* "Interesting" Commands */
		if(c == TNDM)
		{
#		    ifdef TCP
			extern tsturg();
			synchno = tsturg(&NetParams);
#		    endif TCP
#		    ifdef NCP
			--synchno;
#		    endif NCP
		    linptr = linbuf;
		    ovflg = 0;
		    continue;
		}

		/* Other Telnet Commands */
		if(synchno == 0)
		{
		    switch(c)
		    {
			case TNEC:
			    if(linptr>linbuf)
			    {
				linptr--;
				ovflg = 0;
			    }
			    continue;
			case TNEL:
			    linptr = linbuf;
			    ovflg = 0;
			    continue;
			case TNDO:
			case TNWILL:
			    tpopt(c);
			    continue;
			case TNIAC:
			    if(ovflg == 0) *linptr++ = c;
		    }
		}
		continue;  /* Command ignored if synchno or not implemented */
	    }

	    if(c == TNIAC) cmdflg++;
	    else if(synchno == 0) switch(c)
	    {
		case '\r':
		    retflg++;
		    continue;
		case '\n':
		    *linptr++ = c;
		    *linptr++ = '\0';
		    return;
		case '\0':
		    if(retflg)
		    {
			if(ovflg == 0) *linptr++ = '\r';
			retflg = 0;
		    }
		    continue;
		default:
		    if(ovflg == 0) *linptr++ = c;
	    }
	}
}
/*  */
tpopt(c)
  char c;
{
    char bf[3];

	bf[0] = TNIAC;
	bf[1] = (c == TNDO ? TNWONT : TNDONT);
	bf[2] = getch();
	if (net_write(&NetParams, bf, 3) < 0)
	    die(3, "%s:  can't write options to net; %s\n", errmsg(0));

}

getch()
{
	if(--icnt < 0)
	{
retry:
		if ((icnt = net_read(&NetParams, ibuf, IOBSIZ)) < 0) {
		    if (errno == EINTR)
			goto retry;
		    else if (errno == ENETSTAT) {
			get_stuff(&NetParams);
			if (NetParams.ns.n_state & URXTIMO) {
				printf("Host not responding\n");
				goto retry;
			} else if (NetParams.ns.n_state & UURGENT)
				goto retry;
		    }
		}
		if(icnt-- < 0)
		  die(4, "%s:  net input closed; %s\n",
		    progname, errmsg(0));
		iptr = ibuf;
	}
	return(*iptr++ & 0377);
}
/*  */
/* User Telnet and FTP - modified for Illinois NCP from old Rand user telnet.
 *
 * Note: BBN-UNIX only uses it to invoke User FTP. User Telnet is now a
 * completely different program. Dan Franklin (BBN)
 *
 * Changed to use long host numbers jsq BBN 3-27-79.
 *
 * Leave signals alone: child will set according to what parent had set
 *  jsq BBN 5Aug79
 *
 * Pass arguments to both sides of ftp:  pid of other side, name of other
 *  host, name of this host.  All host name-number conversions are now in this
 *  process, and children can now mop up on each other when either one dies.
 *
 * Changed to flush the telnet code, as we don't use it any more dm 3-19-80
 *
 * jsq BBN 5April80 reformatted using indent, put proper names for files
 * to exec back.
 *
 * changed to work with NCP or TCP dm 11-19-80
 */
char ttibuf[32];

main (argc, argv)
int   argc;
char *argv[];
{
	int badhost;
  int   pid, parent;
  portsock socket;
  char  *host;
  int   ftpip[2];
  netaddr hnum;
  register int  count;

  signal (SIG_NETINT, SIG_IGN);
  progname = argv[0];
  socket = (portsock)0;
  if (argc > 1)
  {
	host = argv[1];
	hnum = gethost(host);
	badhost = isbadhost(hnum);
	if (badhost)
		printf ("Unknown host name: \"%s\"\n", host);
	if (argc > 2) socket = ATOSOCK (argv[2]);
  }

  while (badhost)
  {
	printf ("Host: ");
	if (fgets (ttibuf, sizeof ttibuf, stdin) == NULL) exit (1);
	count = strlen (ttibuf);
	if (ttibuf[count-1] == '\n') ttibuf[count-1] = 0;
	while (count-- >= 0)
	    if (ttibuf[count] == '?')
		{
			printf("use host(1) or prhost(1) to find host names\n");
		    break;
		}
	if ((count < 0) && (ttibuf[0]))
	{
		hnum = gethost(ttibuf);
		badhost = isbadhost(hnum);
		if (badhost)
			printf ("Unknown host name: \"%s\"\n", ttibuf);
		else host = ttibuf;
	}
  }

  printf ("Trying %s (%s)\n", host, hostfmt(hnum, 1));
  if (net_open(&NetParams, hnum, (socket?socket:(portsock)FTPSOCK)) < 0)
     {
	printf ("%s cannot connect: %s\n", progname, errmsg (0));
	exit (1);
      }

  printf ("Connections established.\n\n");
  us = thisname (0);
  them = hostname (hnum);
  strcpy ((HOST_ID = malloc(strlen(them)+1)), them);
  getuc (HOST_ID);
  parent = getpid();
  pipe (ftpip);
  if ((pid = fork()) == -1)
  {
	printf ("%s: can't fork %s\n", progname, errmsg (0));
	exit (1);
  }
  if (pid == 0)
  {
	close(ftpip[0]);
/*	fdpip = fdopen(ftpip[1],"w"); */
	pipnum = ftpip[1];
	argv[0] = progname = "ftp_printer";
	ftp_printer(parent);
	exit (1);
  }

  close(ftpip[1]);
/*  fdpip = fdopen(ftpip[0]); Sorry, this loses. */
  pipnum = ftpip[0];
  argv[0] = progname = "ftpmain";
  ftpmain(pid);
}

/*
 * convert to upper case
 */
getuc(s)
register char *s;
{
	register int c;

	while (c = *s)
		*s++ = islower(c)? toupper(c) : c;
}
