#define TRUE 1
#define FALSE 0

#define ALL ""

#define MAXARGS 152     /* Max messages to exec                 */

#define EXISTS    01      /*  existing msg msgstat bit - may be del or undel */
#define DELETED   02      /* msg has been deleted */
#define UNDELETED   04      /* msg is not deleted */
#define SELECTED  010      /* Message selected by an arg           */

#define READONLY  01      /* No write access to folder            */
#define DEFMOD    01      /* In-core profile has been modified    */

/*#define NEWS     1      /* Define for news inclusion            */
#define PROMPT    1     /* Use prompter in compose, etc */

struct  swit {
	char *sw;
	int minchars;
};

/*
 * m_gmsg() returns this structure.  It contains the per folder
 * information which is obtained from reading the folder directory.
 */

struct  msgs {
	int     hghmsg;         /* Highest msg in directory     */
	int     nummsg;         /* Actual Number of msgs        */
	int     lowmsg;         /* Lowest msg number            */
	int     curmsg;         /* Number of current msg if any */
	int     lowsel;         /* Lowest selected msg number   */
	int     hghsel;         /* Highest selected msg number  */
	int     numsel;         /* Number of msgs selected      */
	char   *foldpath;       /* Pathname of folder           */
	char    selist,         /* Folder has a "select" file   */
		msgflags,       /* Folder status bits           */
		filler,
		others;         /* Folder has other file(s)     */
	char    msgstats[];     /* Stat bytes for each msg      */
};

		/* m_getfld definitions and return values       */

#define NAMESZ  64      /* Limit on component name size         */
#define LENERR  -2      /* Name too long error from getfld      */
#define FMTERR  -3      /* Message Format error                 */

			/* m_getfld return codes                */
#define FLD      0      /* Field returned                       */
#define FLDPLUS  1      /* Field " with more to come            */
#define FLDEOF   2      /* Field " ending at eom                */
#define BODY     3      /* Body  " with more to come            */
#define BODYEOF  4      /* Body  " ending at eom                */
#define FILEEOF  5      /* Reached end of input file            */

#ifdef COMMENT          /* Commented out to reduce space        */
/*
 * These standard strings are defined in strings.c.  They are the
 * only system-dependent parameters in MH, and thus by redefining
 * their values in strings.c and reloading the various modules, MH
 * will run on any system.
 */

char    mailbox[],      /* Std incoming mail file (.mail)       */
	draft[],        /* Name of the normal draft file        */
	defalt[],       /* Name of the std folder (inbox)       */
	components[],   /* Name of user's component file (in mh dir) */
	stdcomps[],     /* Std comp file if missing user's own  */
	sndproc[],      /* Path of the send message program     */
	showproc[],     /* Path of the type (l) program         */
	scanproc[],     /* Path of the scan program             */
	prproc[],       /* Path of the pr program               */
	lsproc[],       /* Path of the Harvard ls program       */
	sbmitloc[],	/* Path of the submit program		*/
	sbmitnam[],	/* Name of submit program		*/
	mypath[],       /* User's log-on path                   */
	sysed[],        /* Path of the std (ned) editor         */
	msgprot[],      /* Default message protection (s.a. 0664) */
	foldprot[],     /* Default folder protection      "     */
	listname[],     /* Default selection list folder name   */
	mhnews[];       /* Name of MH news file                 */
#endif

/*
 * node structure used to hold a linked list of the users profile
 * information taken from logpath/.mh_defs.
 */

struct node {
	struct node *n_next;
	char        *n_name,
		    *n_field;
} *m_defs;

char  def_flags;


/*
 * The first char in the mhnews file indicates whether the program
 * calling m_news() should continue running or halt.
 */

#define NEWSHALT        '!'     /* Halt after showing the news  */
#define NEWSCONT        ' '     /* Continue  (ditto)            */
#define NEWSPAUSE       '\001'  /* Pause during news output...  */
