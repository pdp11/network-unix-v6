/* This is a structure that can be used to overlay the data
 * returned from stat() and fstat() system calls.  It is not
 * used anywhere in the kernel.
 */
struct statbuf
{
	char s_minor;
	char s_major;
	int s_inumber;
	int s_flags;
	char s_nlinks;
	char s_uid;
	char s_gid;
	char s_size0;
	char *s_size1;	/* Simulates unsigned int */
	int s_addr[8];
	long s_actime;
	long s_modtime;
};

/* flags */
#define	SLOCK	01		/* inode is locked */
#define	SUPD	02		/* inode has been modified */
#define	SACC	04		/* inode access time to be updated */
#define	SMOUNT	010		/* inode is mounted on */
#define	SWANT	020		/* some process waiting on lock */
#define	STEXT	040		/* inode is pure text prototype */

/* modes */
#define	SALLOC	0100000		/* file is used */
#define	SFMT	060000		/* type of file */
#define		SFDIR	040000	/* directory */
#define		SFCHR	020000	/* character special */
#define		SFBLK	060000	/* block special, 0 is regular */
#define	SLARG	010000		/* large addressing algorithm */
#define	SSUID	04000		/* set user id on execution */
#define	SSGID	02000		/* set group id on execution */
#define SSVTX	01000		/* save swapped text even after use */
#define	SREAD	0400		/* read, write, execute permissions */
#define	SWRITE	0200
#define	SEXEC	0100
