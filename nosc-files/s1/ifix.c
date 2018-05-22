#
/*
	cc -O pgms/ifix.c
	chmod 755 a.out
	cp a.out bin/ifix
*/
/*
Name:
	ifix

Synopsis:
	ifix [file volume]

Description:
	Examine an i-node on the specified file volume and potentially
	modify (i.e., patch) the values contained.  Ifix asks for the
	number of the i-node to be examined, reads it off the specified
	file volume (/dev/rk1 is the default), displays the various field
	values, and asks for corrected values for each field.  Numbers
	are assumed to be decimal unless given with a leading zero, in
	which case they are assumed to be octal (i.e., the C convention).


See also:
	clri (VIII)

Bugs:
	If the file is open, ifix is likely to be ineffective.
*/
#include "/h/ino.h"

struct {
	char LOBYTE;
	char HIBYTE;
};

main(argc, argv)
int argc;
char **argv;
{
	register i;
	int fi, inum;
	struct inode inode;

	if((fi = open(i = argc>1?argv[1]:"/dev/rk1", 2)) < 0) {
		printf("Can't open file volume %s\n", i);
		exit();
	}
	inum = (getnum("I-node number", 1)+31)*32;
	seek(fi, inum, 0);
	if(read(fi, &inode, sizeof inode) != sizeof inode) {
		printf("Couldn't read I-node\n");
		exit();
	}
	printf("Flags = 0%o, Links = %d, UID = %d, GID = %d, Size = %o %d\nAddresses =",
		inode.i_mode,
		inode.i_nlink,
		inode.i_uid,
		inode.i_gid,
		inode.i_size0, inode.i_size1);
	for(i = 0; i < 8; i++) printf("  %d", inode.i_addr[i]);
	printf("\nTime(access) = 0%o 0%o, Time(mod) = 0%o 0%o\n",
		inode.i_atime[0], inode.i_atime[1],
		inode.i_mtime[0], inode.i_mtime[1]);
	if(getuid()) exit();	/* must be super to modify i-node */
	inode.i_mode = getnum("Flags", inode.i_mode);
	inode.i_nlink = getnum("Links", inode.i_nlink);
	inode.i_uid = getnum("UID", inode.i_uid);
	inode.i_gid = getnum("GID", inode.i_gid);
	if((i = inode.i_mode&IFMT) == IFCHR  ||  i == IFBLK) {
		inode.i_addr[0].HIBYTE = getnum("(Major) device code", inode.i_addr[0].HIBYTE);
		inode.i_addr[0].LOBYTE = getnum("(Minor) device number", inode.i_addr[0].LOBYTE);
		inode.i_size0 = inode.i_size1 = 0;
	} else {
		inode.i_size0 = 0;
		inode.i_size1 = getnum("Size", inode.i_size1);
		for(i = 0; i*512 < inode.i_size1 && i < 8; i++)
			inode.i_addr[i] = getnum("Addr", inode.i_addr[i]);
	}
	seek(fi, inum, 0);
	write(fi, &inode, sizeof inode);
}

getnum(type, ans)
char *type;
int ans;
{
	register i, val, t;
	int base, len;
	char buf[20];

again:
	printf("%s = ", type);
	len = read(0, buf, sizeof buf);
	if(len == 1) return ans;
	val = 0;  base = 10;
	for(i = 0; i < len; i++) {
		t = buf[i];
		if(t == '\n'  ||  t == ' '  ||  t == '\t') continue;
		if(t < '0'  ||  t > '9') {
			printf("Error, enter again\n");
			goto again;
		}
		if(val == 0  &&  t == '0')
			base = 8;
		else
			val = val*base + (t-'0');
	}
	return val;
}
