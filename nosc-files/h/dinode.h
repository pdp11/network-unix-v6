/*
 * Inode structure as it appears on
 * the disk. Not used by the system,
 * but by things like check, df, dump.
 */
/* U. of C. buffer mod:  this file is now used by the
 * system.  As a result, the 'i_' prefixes in the inode
 * structure have been changed to 'di_' (for disk inode),
 * and the structure name itself to 'dinode'.
 * Also, the ..size0 and ..size1 attributes had to be
 * changed to ..siz0 and ..siz1 due to C limitations.
 */
struct	dinode
{
	int	di_mode;
	char	di_nlink;
	char	di_uid;
	char	di_gid;
	char	di_siz0;
	char	*di_siz1;
	int	di_addr[8];
	int	di_atime[2];
	int	di_mtime[2];
};
