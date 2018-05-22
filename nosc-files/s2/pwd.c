char dot[] ".";
char dotdot[] "..";
char root[] "/";
char name[512];
int file, off -1;
struct statb {int devn, inum, i[18];}x;
struct entry { int jnum; char name[16];}y;
struct mtabntry {char mounted_at [32]; char what [32];}mte;

main() {
	int n;

loop0:
	stat(dot, &x);
	if((file = open(dotdot,0)) < 0) prname();
loop1:
	if((n = read(file,&y,16)) < 16) prname();
	if(y.jnum != x.inum)goto loop1;
	close(file);
	if(y.jnum == 1) ckroot();
	cat(y.name);
	chdir(dotdot);
	goto loop0;
}

ckroot() {
	int i, n;

	if((n = stat(y.name,&x)) < 0) prname();
	i = x.devn;
	if((n = chdir(root)) < 0) prname();
	if((file = open(root,0)) < 0) prname();
loop:
	if((n = read(file,&y,16)) < 16) check_mtab(i);
	if(y.jnum == 0) goto loop;
	if((n = stat(y.name,&x)) < 0) prname();
	if(x.devn != i) goto loop;
	x.i[0] =& 060000;
	if(x.i[0] != 040000) goto loop;
	if(y.name[0]=='.')if(((y.name[1]=='.') && (y.name[2]==0)) ||
				(y.name[1] == 0)) goto pr;
	cat(y.name);
pr:
	cat ("");
	prname();
}

prname() {
	if(off<0)off=0;
	name[off] = '\n';
	write(1,name,off+1);
	exit();
}

cat(from) char *from; {
	int i, j;

	i = -1;
	while(from[++i] != 0);
	if((off+i+2) > 511) prname();
	for(j=off+1; j>=0; --j) name[j+i+1] = name[j];
	off=i+off+1;
	off = off ? off:1;
	name[i] = root[0];
	if (i > 0)
		for(--i; i>=0; --i) name[i] = from[i];
}

check_mtab (devno) {
	int	mfid;
	if ((mfid = open ("/etc/mtab", 0)) < 0) prname ();
	while (read (mfid, &mte, sizeof mte) == sizeof mte) {
		if (stat (mte.mounted_at,&x) < 0) continue;
		if (x.devn == devno) {
			cat (mte.mounted_at);
			prname ();
		}
	}
	prname ();
}
