/*
Module Name:
	df -- print number of free blocks on device

Installation:
	if $1e = finale goto finale
	cc df.c
	exit
: finale
	cc -O -s df.c
	if ! -r a.out exit
	su cp a.out /bin/df
	rm -f a.out

Synopsis:
	df [ block-device-name ... ]

Function:
	Print out the number of free blocks on a list of filesystems.  If
	the file system is unspecified, the free space on the normally
	mounted file systems is printed.

Restrictions:

Diagnostics:

Files:

See Also:

Bugs:

Module History:
	Original source from Bell Labs.
	Modified for NOSC default devices Nov77 by Greg Noel.  Also cleaned
	    up some of the logic, simplifying default calculation.
	Modified 24Oct78 by Greg Noel to search the /dev directory if the
	    given name isn't sufficient.
*/
char	*dargv[]	/* list of default devices */
{
	"root",
	"user",
	0
};

char	devname[20]	{"/dev/               "};

struct
{
	char	*s_isize;
	char	*s_fsize;
	int	s_nfree;
	int	s_free[100];
	int	s_ninode;
	int	s_inode[100];
	char	s_flock;
	char	s_ilock;
	char	s_fmod;
	int	time[2];
	int	pad[50];
} sblock;

int	fi;

main(argc, argv)
char **argv;
{
	argv[argc] = 0;
	if(argc <= 1) argv = dargv - 1;

	while(*++argv) {
		dfree(*argv);
	}
}

dfree(file)
char *file;
{
	int i;
	char *p, *q;

	if( (fi = open(file, 0)) < 0) {
		for(p = devname+5, q = file; *p++ = *q++;);
		if( (fi = open(devname, 0)) < 0) {
			printf("cannot open %s\n", file);
			return;
		}
	}
	printf("%s ", file);
	sync();
	bread(1, &sblock);
	i = 0;
	while(alloc())
		i++;
	printf("%l\n", i);
	close(fi);
}

alloc()
{
	int b, i, buf[256];

	i = --sblock.s_nfree;
	if(i<0 || i>=100) {
		printf("bad free count\n");
		return(0);
	}
	b = sblock.s_free[i];
	if(b == 0)
		return(0);
	if(b<sblock.s_isize+2 || b>=sblock.s_fsize) {
		printf("bad free block (%l)\n", b);
		return(0);
	}
	if(sblock.s_nfree <= 0) {
		bread(b, buf);
		sblock.s_nfree = buf[0];
		for(i=0; i<100; i++)
			sblock.s_free[i] = buf[i+1];
	}
	return(b);
}

bread(bno, buf)
{
	int n;
	extern errno;

	seek(fi, bno, 3);
	if((n=read(fi, buf, 512)) != 512) {
		printf("read error %d\n", bno);
		printf("count = %d; errno = %d\n", n, errno);
		exit();
	}
}
