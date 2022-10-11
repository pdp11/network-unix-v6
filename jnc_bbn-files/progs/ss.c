#include "../h/param.h"
#include "../h/proc.h"
#include "../h/user.h"
#include "../h/tty.h"

char *ss_nlist "/tmp/ss.nlist";
char *ss_names "/tmp/ss.names";
char *ss_devices "/tmp/ss.devices";
char *mem "/dev/mem";
char core;
char pcore;
char swap;

int	ndev;
char	devc[65];
int	devl[65];
char	swapfile[16];
struct ibuf {
	char	idevmin, idevmaj;
	int	inum;
	int	iflags;
	char	inl;
	char	iuid;
	char	igid;
	char	isize0;
	int	isize;
	int	iaddr[8];
	char	*ictime[2];
	char	*imtime[2];
	int	fill;
};

char cmndbuf[4096];
char *cbfree &cmndbuf[0];

#define NSYMB 22

struct NL {
	char name[8];
	int type;
	char *value;
} nl[NSYMB + 1];

char *symbol[NSYMB] {
	"_proc",
	"_dc11",
	"_kl11",
	"_dh11",
	"_pty",
	"_buf",
	"_buffers",
	"_bfreeli",
	"_fp",
	"_rkaddr",
	"_rrkbuf",
	"_runin",
	"_runout",
	"_lbolt",
	"_time",
	"_tout",
	"_callout",
	"_imp",
	"_ncprq",
	"_ncpirq",
	"_inode",
	"_swapdev"
};

char *names[256];
char namebuf[1024];
char *nbfree &namebuf[0];

struct INFO {
	struct  proc p;
	char    ttynam;
	int     cptime;
	char    *cmnd;
	char    uid, ruid;
} info[NPROC], *lastinfo;

main(argc, argv) int argc; char *argv[]; {
	register struct INFO *ip;
	register int j;
	extern icomp();


	if ( (core=open(mem,0)) < 0 || (pcore=open(mem,0)) < 0 ) {
		printf("cannot open %s\n",mem);
		exit(1);
	}

	getnl();
	getnames();

	/* --- horrendous BTL code --- */

	seek(core, nl[NSYMB-1].value, 0);
	read(core, &nl[NSYMB-1].value, 2);
	if(chdir("/dev") < 0) { printf("cannot chdir to /dev\n"); exit(1); }
	getdev();

	printf("tty owner     pid status     size cptime command\n");
	printf("-   -----     --- ------     ---- ------ -------\n");

	seek(pcore, nl[0].value, 0);
	ip = &info[0];
	for(j = 0; j < NPROC; j++) {
		read(pcore, &ip->p, sizeof ip->p);
		if(ip->p.p_stat == 0) continue;
		getusr(&ip->p, ip);
		ip++;
	}
	lastinfo = ip - 1;

	qsort(info, lastinfo - info + 1, sizeof info[0], icomp);
	output(argc, argv);

}

icomp(ii1, ii2) {
	struct INFO *i1, *i2;

	i1= ii1;  i2 = ii2;
	if (i1->ttynam != i2->ttynam)
		return(i2->ttynam - i1->ttynam);
	if(i1->p.p_pgrp != i2->p.p_pgrp)
		return(i1->p.p_pgrp - i2->p.p_pgrp);
	return(i1->p.p_pid - i2->p.p_pid);
}

getnl() {
	int file;
	register int j;
	register char *dp, *sp;

	file = open(ss_nlist, 0);
	if(file >= 0)
		read(file, nl, sizeof nl);
	else {
		for(j = 0; j < NSYMB; j++) {
			sp = symbol[j];
			dp = nl[j].name;
			while(*dp++ = *sp++);
		}
		nlist("/unix", nl);
		file = creat(ss_nlist, 0444);
		write(file, nl, sizeof nl);
	}
	close( file );
}

getusr(pp, ip) struct proc *pp; struct INFO *ip; {
	int baddr, laddr, mf;
	int baddr2, laddr2;
	register int *i;
	register char *cp, *cp1;
	int c;
	static int stbuf[257];
	int j;
	struct { long lv; };
	long ltime;

	baddr2 = baddr = laddr2 = laddr = 0;
	if(pp->p_flag & SLOAD) {
		laddr2 = laddr = pp->p_addr;
		mf = core;
	} else {
		baddr2 = baddr = pp->p_addr;
		mf = swap;
	}
	laddr =+ pp->p_size - 8;
	baddr =+ laddr >> 3;
	baddr2 =+ laddr2 >> 3;
	laddr = (laddr & 07) << 6;
	laddr2 = (laddr2 & 07) << 6;

	seek(mf, baddr2, 3);
	seek(mf, laddr2, 1);
	read(mf, &u, sizeof u);
	ip->uid = u.u_uid;
	ip->ruid = u.u_ruid;
	ltime = (u.u_utime)->lv + (u.u_stime)->lv + 30;
	ip->cptime = ldiv(ltime, 60);
	/* --- BTL code to get tty name --- */
	ip->ttynam = '!';
	if(u.u_ttyp) for(j = 0; j < ndev; j++) if(devl[j] == u.u_ttyd) {
		ip->ttynam = devc[j];
		break;
	}

	seek(mf, baddr, 3);
	seek(mf, laddr, 1);
	read(mf, stbuf, 512);
	for(i = &stbuf[256]; i > &stbuf[0];) {
		if(*--i == -1) {
			cp = i + 1;
			if(*cp == 0) cp++;
			for(cp1 = cp; cp1 < &stbuf[256]; cp1++) {
				c = *cp1;
				if(c == 0) *cp1 = ' ';
				else if(c < ' ' || c > 0176) *cp1 = 0;
			}
			cp1 = cbfree;
			while(*cp1++ = *cp++);
			ip->cmnd = cbfree;
			cbfree = cp1;
			return;
		}
	}
	ip->cmnd = cbfree;
	*cbfree++ = '?'; *cbfree++ = 0;
	return;
}

output(argc, argv) int argc; char *argv[]; {
	register int i;
	register int j;
	register struct INFO *ip;

	if(argc == 1) {
		for(ip = lastinfo; ip >= &info[0]; ip--)
		if /***(ip->ruid) && ***/
		 (ip->p.p_pgrp == ip->p.p_pid ||
		  (ip->p.p_ppid == 1 && ip->p.p_pgrp != 0)
		      ) out1(ip, 0);
	} else 
		for(i = 1; i < argc; i++)
		{	j = 0;
			for(ip = &info[0]; ip <= lastinfo; ip++)
			if((ip->p.p_pgrp == ip->p.p_pid)
				&& seq(argv[i], names[ip->ruid])) 
			{	out1(ip, 0);
				j++;
			}
			if(j == 0) printf("%s not logged in\n", argv[i]);
		}
}

out1(p, level) struct INFO *p; {
	register struct INFO *rp, *ip;
	int j;
	ip = p;

	out2(ip, level);
	for(rp = &info[0]; rp <= lastinfo; rp++)
	if(rp->p.p_pid /* kludge for process 0 */
	&& rp->p.p_ppid == ip->p.p_pid /* direct descendant */
	&& rp->p.p_pgrp == ip->p.p_pgrp /* same session */
	) out1(rp, level+1);

/***    if(level == 0)
	for(rp = &info[0]; rp <= lastinfo; rp++)
	if(rp->p.p_ppid == 1
	&& rp->p.p_pgrp == ip->p.p_pgrp
	&& rp->p.p_pid != ip->p.p_pid
	) out2(rp, 0);                          ***/
}

out2(ip, level) struct INFO *ip; int level; {
	int j;
	static lasttty;

	if(lasttty != ip->ttynam) {
		lasttty = ip->ttynam;
		printf("%c ", lasttty);
	} else printf("  ");
	if(level == 0) {
		printf("- ");
		puid(ip->p.p_uid);
	} else {
		printf("-");
		j = level;
		while(j--) putchar('>');
		j = level;
		while(j++ <= 8) putchar(' ');
	}
	printf("%5d ", ip->p.p_pid);
	pstatus(&ip->p);
	printf("%2dk ", ip->p.p_size >> 5);
	printf("%3d:", ip->cptime/60);
	ip->cptime =% 60;
	printf(ip->cptime < 10 ? "0%d " : "%d ", ip->cptime);
	printf("%-.30s ", ip->cmnd);
	printf("\n");
}

gwchan(wchan) char *wchan; {
	register struct NL *trial, *guess;

	trial = 0;
	for(guess = &nl[0]; guess <= &nl[NSYMB]; guess++)
	if(guess->value > trial->value && guess->value <= wchan)
		trial = guess;
	return(trial);
}

getnames() {
	int file;
	register char *sp, *dp;
	int j, c;

	file = open(ss_names, 0);
	if(file >= 0) {
		read(file, names, sizeof names);
		read(file, namebuf, sizeof namebuf);
	} else {
		close(0);
		open("/etc/passwd", 0);
		for(;;) {
			dp = nbfree;
			c = getchar(); if(c < 'a' || c > 'z') break;
			while(c != ':') {
				*dp++ = c;
				c = getchar();
			}
			*dp++ = 0;
			while(getchar() != ':');
			j = 0;
			while((c = getchar()) >= '0' && c <= '9')
				j = j * 10 + c - '0';
			if (names[j] == 0)
			{       names[j] = nbfree;
				nbfree = dp;
			}
			while(getchar() != '\n');
		}
		names[0] = --dp;        /* don't print user-id 0 name */
		file = creat(ss_names, 0444);
		write(file, names, sizeof names);
		write(file, namebuf, sizeof namebuf);
	}
	close(file);
}

seq(s1, s2) char *s1, *s2; {
	while(*s1) if(*s1++ != *s2++) return(0);
	return(1);
}

puid(uid) char uid; {
	if(names[uid & 0377])
		printf("%-8.8s", names[uid & 0377]);
	else
		printf("%-8d", uid & 0377);
}
pstatus(pp) struct proc *pp; {
	register struct NL *nlp;
	register char *mess;
	register i;

	switch(pp->p_stat) {
	case SSLEEP: mess = pp->p_pri > 0 ? "slp" : "SLP"; break;
	case SRUN:   mess = "run"; break;
	case SIDL:   mess = "idl"; break;
	case SZOMB:  mess = "zmb"; break;
	case SSTOP:  mess = "stp"; break;
	}
	printf("%s%c",mess,(pp->p_flag & SLOAD) ? '*' : ' ');
	mess = " ";
	if(pp->p_stat == SSLEEP) {
		nlp = gwchan(pp->p_wchan);
		switch(nlp - &nl[0]) {
		case 0:
			mess = "fork";
			break;
		case 1: case 2: case 3: case 4:
			mess = "tty";
			break;
		case 5: case 6: case 7:
			mess = "I/O";
			break;
		case 8:
			mess = "file";
			break;
		case 9: case 10:
			mess = "disk";
			break;
		case 11:
			mess = "runin";
			break;
		case 12:
			mess = "runout";
			break;
		case 13: case 14: case 15: case 16:
			mess = "clock";
			break;
		case 17: case 18: case 19: case 20:
			mess = "net";
			break;
		default:
			mess = "?";
		}
	}
	printf("%-7.7s ", mess);
}

/* --- from ps.c for V6 to get teletype names --- */


getdev()
{
	register struct { int dir_ino; char dir_n[14]; } *p;
	register i, c;
	int f;
	int k;
	char dbuf[512];
	int sbuf[20];

	/* see if we have a file containing all the neat information */
	f = open(ss_devices, 0);
	if (f >= 0)
	{	read(f, devc, sizeof devc);
		read(f, devl, sizeof devl);
		read(f, swapfile, sizeof swapfile);
		close( f );
		goto swapopen;
	}
	f = open("/dev", 0);
	if(f < 0) {
		printf("cannot open /dev\n");
		exit(0);
	}
	c = 0;

loop:
	i = read(f, dbuf, 512);
	if(i <= 0) {
		close(f);
		f = creat(ss_devices, 0444);
		write(f, devc, sizeof devc);
		write(f, devl, sizeof devl);
		write(f, swapfile, sizeof swapfile);
		close(f);
		goto swapopen;
	}
	while(i < 512)
		dbuf[i++] = 0;
	for(p = dbuf; p < dbuf+512; p++) {
		if(p->dir_ino == 0)
			continue;
		if(p->dir_n[0] == 't' &&
		   p->dir_n[1] == 't' &&
		   p->dir_n[2] == 'y' &&
		   p->dir_n[4] == 0 &&
		   p->dir_n[3] != 0) {
			if(stat(p->dir_n, sbuf) < 0)
				continue;
			devc[c] = p->dir_n[3];
			devl[c] = sbuf->iaddr[0];
			c++;
			continue;
		}
		if(swapfile[0]) continue;
		if(stat(p->dir_n, sbuf) < 0)
			continue;
		if((sbuf->iflags & 060000) != 060000)
			continue;
		if(sbuf->iaddr[0] == nl[NSYMB-1].value) {
			for(k = 0; k < 14; k++)
				swapfile[k] = p->dir_n[k];
		}
	}
	goto loop;
swapopen:
	swapfile[14] = '\000';
	swap = open(swapfile, 0);
	if (swap < 0)
	{	printf("Cannot open swap device %o\n", nl[NSYMB-1].value);
		exit(1);
	}
	c = 0;
	while( devc[c] ) c++;
	ndev = c;
	return;
}
