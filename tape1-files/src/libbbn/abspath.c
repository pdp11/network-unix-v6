#
/*
 * char *abspath(relpath, pathlow, pathtop)
 * Take any legal pathname, relpath, and put the equivalent most
 * compressed absolute pathname in the area starting at pathlow,
 * but not exceeding pathtop.  A pointer to the null of the absolute
 * pathname string is returned.
 *
 * All occurences of ".", "..", and "...", all multiple adjacent
 * slashes, and any trailing slashes are removed.
 *
 * Any error is indicated by the absence of a slash at the beginning
 * of the absolute pathname.  This should occur only if there are
 * protection problems encountered in reading directories or /etc/mtab,
 * or if /etc/mtab is in bad format or does not exist, or "..." is not
 * implemented.  Many other conditions are also checked, however.
 *
 * The mount table, /etc/mtab, is read only once, unless it is discovered
 * to have been modified since the last time.
 *
 * Do internal bufferring for directory reads instead of using getc
 *      jsq BBN 23Jan80
 * Adjust for V7/V32 bjw BBN 28 Aug 80.
 *
 */


#define V7
#ifdef V6
#include <statbuf.h>
#define STAT statbuf
#define NLINKS s_nlinks
#define FLAGS s_flags
#define FMT SFMT
#define FDIR SFDIR
#define INO s_inumber
#define ROOTINO 1
#define DEV s_minor
#define GID s_gid
#define MTIME s_modtime
#define ino_t unsigned
#define DEVTEST(a,b) ((a.s_minor == b.s_minor) && (a.s_major == b.s_major))
#define ROOTTEST(a,b) ((a.s_minor == b.s_addr[0].s_minor) && (a.s_major == b.s_addr[0].s_major))
#define DIRECT direct
#endif 
#ifdef V7
#include <sys/types.h>
#include <sys/stat.h>
#define STAT stat
#define NLINKS st_nlink
#define FLAGS st_mode
#define FMT S_IFMT
#define FDIR S_IFDIR
#define INO st_ino
#define ROOTINO 2
#define DEV st_dev
#define RDEV st_rdev
#define GID st_gid
#define MTIME st_mtime
#define DEVTEST(a,b) (a.DEV == b.DEV)
#define ROOTTEST(a,b) (a.DEV == b.RDEV)
#define DIRECT dir
#endif
#define NMOUNT 16	     /* don't use param.h in a library function */

char   *abspath (relpath, pathlow, pathtop)
char   *relpath;
char   *pathlow;
char   *pathtop;
{
    register char  *cp,
                   *path;
    register int    ch;
    int ix;
    char   *ep,
           *slash,
           *root,
           *dotdot,
           *dotpoint;
    char    dotbuf[128];
    char    pathbuf[512];
#   define low (&pathbuf[0])
#   define top (&pathbuf[sizeof(pathbuf)-1])
#   define dotlow (&dotbuf[0])
#   define dottop (&dotbuf[sizeof(dotbuf)-1])
    extern  seq (), slength (), smatch ();
    extern char *scopy (), *sfind (), *sbcopy ();
    int fd;

 /* this stuff is for reading directories */
    char  inbuf[512];           /* must be a multiple of DIRSIZE */
    char *np;
#   define DIRSIZE 16
#   define SDIR 16
#   define SDIRNAME 14
    struct {
	ino_t   d_ino;
	char    d_name[SDIR];  /* oversize to hold a null */
    }       dir;
    struct STAT rootstat,
                    filestat;

 /* this is used for reading the mtab table */
#   define NAMSIZ  32
#   define MTAB     "/etc/mtab"
    static struct {
	char    file[NAMSIZ];
	char    spec[NAMSIZ];
    }                   mtab[NMOUNT];
    static int  nmount;
    static  long mtabmod;    /* modification time of MTAB */

#ifdef V6
    struct {
	char    d_minor;
	char    d_major;
    };
#endif

    root = "/";
    slash = "/";
    dotdot = "..";
    dotpath (relpath, low, top);
    if (smatch (low, root) == slength (root)) {
	path = scopy (low, pathlow, pathtop);
	return (path);
    }
    path = low;
    cp = sfind (path, slash);
    dotpoint = scopy (path, dotlow, &dotlow[cp - path]);
    path = cp;
    *top = 0;		     /* make sure we'll have a final null */
    path = sbcopy (0, path, top, low);
    fd = -1;
    for (;;) {
	if (stat (dotlow, &filestat) == -1)
	    break;
	if (filestat.INO == ROOTINO) {
			     /* reached root of an fs */
	    if (stat (root, &rootstat) == -1)
		break;
	    if (DEVTEST(filestat,rootstat)) {
		path = sbcopy (0, root, path, low);
			     /* it's the root fs */
		dotpath (path, low, top);
		path = low;
		break;
	    }
	    if (stat (MTAB, &rootstat) == -1)
		break;
	    if ((nmount == 0) || (rootstat.MTIME  != mtabmod)) {
		mtabmod = rootstat.MTIME;
		if ((fd = open (MTAB, 0)) == -1)
		    break;
		ch = read (fd, &mtab[0], sizeof (mtab));
		if (ch <= 0)
		    break;
		nmount = ch / sizeof (mtab[0]);
		close (fd);
		fd = -1;
	    }
	    dotpoint = scopy ("/dev/", dotlow, dottop);
	    for (ch = 0; ch < nmount; ch++) {
		scopy (&mtab[ch].spec[0], dotpoint, dottop);
		if (stat (dotlow, &rootstat) == -1)
		    break;
		if (ROOTTEST(filestat,rootstat)){
		    if (*path)
			path = sbcopy (0, slash, path, low);
		    path = sbcopy (0, &mtab[ch].file[0], path, low);
		    dotpath (path, low, top);
		    path = low;
		    break;
		}
	    }
	    break;
	}
	dotpoint = scopy (slash, dotpoint, dottop);
	dotpoint = scopy (dotdot, dotpoint, dottop);

	if ((fd = open (dotlow, 0)) == -1)
	    break;
	for (ch = sizeof (inbuf), np = &inbuf[ch];;) {
	    if (np >= &inbuf[ch])
	    {
		if ((ch = read (fd, &inbuf[0], sizeof(inbuf))) <= 0)
		    break;
		if ((ch % DIRSIZE) != 0)
		{
		    ch = 0;
		    break;
		}
		np = &inbuf[0];
	    }
            cp = (char *)&dir;
	    for (ix = 0; ix < DIRSIZE ; *cp++ = *np++,ix++) /*move exactly
			DIRSIZE bytes, keeping np in step */
		;
	    *cp = 0;    /*assure the name is a string*/
	    if (dir.d_ino == filestat.INO)
		break;
	}
	if (ch <= 0)
	    break;
	close (fd);
	fd = -1;
	if (*path)
	    path = sbcopy (0, slash, path, low);
	path = sbcopy (0, &dir.d_name[0], path, low);
    }
    if (fd >= 0)
	close (fd);
    path = scopy (path, pathlow, pathtop);
    return (path);
}
