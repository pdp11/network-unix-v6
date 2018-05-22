#
/*
 * Inode structure as returned by
 * system call stat.
 */
struct	inode
{
	int	i_dev;
	int	i_number;
	int	i_mode;
	char	i_nlink;
	char	i_uid;
	char	i_gid;
	char	i_size0;
	char	*i_size1;
	int	i_addr[8];
	long	i_atime;
	long	i_mtime;
};
/* advisable to use this define to generate owner as 16-bit num */
#define IOWNER(i_n_buf) (((i_n_buf).i_uid<<8)|(i_n_buf).i_gid&0377)
/* modes */
#define	IALLOC	0100000
#define	IFMT	060000
#define		IFDIR	040000
#define		IFCHR	020000
#define		IFBLK	060000
#define	ILARG	010000
#define	ISUID	04000
#define	ISGID	02000
#define ISVTX	01000
#define	IREAD	0400
#define	IWRITE	0200
#define	IEXEC	0100

struct { char d_minor, d_major; };
