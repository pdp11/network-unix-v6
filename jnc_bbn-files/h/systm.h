/* for unibus mapping use */
#define MAPSIZE 31      /* actual number of registers in unibus map */
#define MAPLOW  8       /* number reserved for kernel use */

int ubmap[(MAPSIZE - MAPLOW) + 1];      /* for use with malloc */

/*
 * Random set of variables
 * used by more than one
 * routine.
 */
char	canonb[CANBSIZ];	/* buffer for erase and kill (#@) */
int	coremap[CMAPSIZ];	/* space for core allocation */
int	swapmap[SMAPSIZ];	/* space for swap allocation */
int	*rootdir;		/* pointer to inode of root directory */
int	cputype;		/* type of cpu =40, 45, or 70 */
int	execnt;			/* number of processes in exec */
int	lbolt;			/* time of day in 60th not in time */
int	time[2];		/* time in sec from 1970 */
int	fastime[2];		/* time in 1/60ths of sec */
int	tout[2];		/* time of day of next sleep */
/*
 * The callout structure is for
 * a routine arranging
 * to be called by the clock interrupt
 * (clock.c) with a specified argument,
 * in a specified amount of time.
 * Used, for example, to time tab
 * delays on teletypes.
 */
struct	callo
{
	int	c_time;		/* incremental time */
	int	c_arg;		/* argument to routine */
	int	(*c_func)();	/* routine */
} callout[NCALL];
/*
 * Mount structure.
 * One allocated on every mount.
 * Used to find the super block.
 */
struct	mount
{
	int	m_dev;		/* device mounted */
	int	*m_bufp;	/* pointer to superblock */
	int	*m_inodp;	/* pointer to mounted on inode */
} mount[NMOUNT];
int	mpid;			/* generic for unique process id's */
int	pgrpct;			/* generic for unique tty group numbers */
char	runin;			/* scheduling flag */
char	runout;			/* scheduling flag */
char	runrun;			/* scheduling flag */
char	curpri;			/* more scheduling */
int	maxmem;			/* actual max memory per process */
int	*lks;			/* pointer to clock device */
int	rootdev;		/* dev of root see conf.c */
int	swapdev;		/* dev of swap see conf.c */
int	swplo;			/* block number of swap space */
int	nswap;			/* size of swap space */
int	updlock;		/* lock for sync */
int	rablock;		/* block to be read ahead */
char	regloc[];		/* locs. of saved user registers (trap.c) */

#ifdef  LCBA            /* see map_page.c set_lcba.c */
char *lcba_address;     /* address, size, and max are in core clicks */
char *lcba_size;        /* and are initialized in map_page and main */
char *lcba_max;         /* maximum size of the lcba */
int lcba_count;         /* number of processes which have siezed the lcba */
#endif
