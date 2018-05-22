/*
 * Random set of variables
 * used by more than one
 * routine.
 */
#ifdef CMAPSIZ
int	coremap[CMAPSIZ];	/* space for core allocation */
#endif CMAPSIZ
#ifdef SMAPSIZ
int	swapmap[SMAPSIZ];	/* space for swap allocation */
#endif SMAPSIZ
int	*rootdir;		/* pointer to inode of root directory */
int	cputype;		/* type of cpu =40, 45, or 70 */
int	execnt;			/* number of processes in exec */
int	lbolt;			/* time of day in 60th not in time */
int	time[2];		/* time in sec from 1970 */
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
};
#ifdef NCALL
struct callo callout[NCALL];
#endif NCALL
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
#ifdef UCBUFMOD
	int	m_isize;	/* incore copy of s_isize */
	char	*m_fsize;	/* incore copy of s_fsize */
	int	m_nfree;	/* incore copy of s_nfree */
	int	m_ninode;	/* incore copy of s_ninode */
	char	m_flock;	/* lock during free list manipulation */
	char	m_ilock;	/* lock during I list manipulation */
	char	m_fmod;		/* super block modified flag */
	char	m_ronly;	/* mounted read-only flag */
/*PWB	char	*m_tfree;	/* total free blocks available */
/*PWB	char	*m_tinode;	/* total free inodes avaliable */
#endif UCBUFMOD
};
#ifdef NMOUNT
struct mount mount[NMOUNT];
#endif NMOUNT
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
