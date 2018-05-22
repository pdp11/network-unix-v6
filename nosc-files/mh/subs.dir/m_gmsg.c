#include "/m/mh/mh.h"

char    current[];
char    listname[];
struct  msgs *mp;

m_gmsg(name)
char *name;

{
	register int i, j;
	register char *cp;
	int  curfil;
	char buf[132];

	struct {
		struct {
			int d_inum;
			char d_name[14];
		} ent;
		int terminator;
	} dir;

	struct {
		int xhghmsg,
		    xnummsg,
		    xlowmsg,
		    xcurmsg;
		char xselist,
		     xflags,
		     xfiller,
		     xothers;
		char xmsgs[1000];
	} msgbuf;

	if((i = open(".", 0)) < 0)
		return(0);
	for(j = 0; j < 1000; j++)
		msgbuf.xmsgs[j] = 0;
	msgbuf.xcurmsg = 0;
	msgbuf.xnummsg = 0;
	msgbuf.xselist = 0;
	msgbuf.xothers = 0;
	msgbuf.xlowmsg = 5000;
	msgbuf.xhghmsg = 0;
	msgbuf.xflags  = (access(".",2) == -1)? READONLY:0;  /*RAND sys call*/
	curfil = 0;
	dir.terminator = 0;
	cp = dir.ent.d_name;
	for(;;) {
		if(read(i, &dir, sizeof dir.ent) != sizeof dir.ent)
			break;
		if(dir.ent.d_inum) {
			if(j = mu_atoi(cp)) {
				if(j > msgbuf.xhghmsg)
					msgbuf.xhghmsg = j;
				msgbuf.xnummsg++;
				if(j < msgbuf.xlowmsg)
					msgbuf.xlowmsg = j;
				msgbuf.xmsgs[j] = EXISTS;
				if(*cp == ',') msgbuf.xmsgs[j] =| DELETED;
				else msgbuf.xmsgs[j] =| UNDELETED;
			} else if(!equal(cp, ".")  &&
				  !equal(cp, "..") &&
				  *cp != ',')
					if(equal(cp, current))
						curfil++;
					else if(equal(cp, listname))
						msgbuf.xselist++;
					else
						msgbuf.xothers++;
		}
	}
	if(!msgbuf.xhghmsg)
		msgbuf.xlowmsg = 0;
	close(i);
	if(msgbuf.xflags&READONLY) {
		copy(name, copy("cur-", buf));
		if((cp = m_find(buf)) != -1)
			if(j = mu_atoi(cp))
				msgbuf.xcurmsg = j;
	} else if(curfil && (i = open(current, 0)) >= 0) {
		if((j = read(i, dir.ent.d_name, sizeof dir.ent.d_name)) >= 2){
			dir.ent.d_name[j-1] = 0;    /* Zap <lf> */
			if(j = mu_atoi(dir.ent.d_name))
				msgbuf.xcurmsg = j;
		}
		close(i);
	}
	if((i = alloc(sizeof *mp + msgbuf.xhghmsg + 2)) == -1)
		return(0);
	i->hghmsg   = msgbuf.xhghmsg;
	i->nummsg   = msgbuf.xnummsg;
	i->lowmsg   = msgbuf.xlowmsg;
	i->curmsg   = msgbuf.xcurmsg;
	i->selist   = msgbuf.xselist;
	i->msgflags = msgbuf.xflags;
	i->others   = msgbuf.xothers;
	i->foldpath = name;
	i->lowsel   = 5000;
	i->hghsel   = 0;
	i->numsel   = 0;
	for(j = 0; j <= msgbuf.xhghmsg; j++)
		i->msgstats[j] = msgbuf.xmsgs[j];
	return(i);
}


mu_atoi(str)
char *str;
{
	register char *cp;
	register int i;

	i = 0;
	cp = str;
	if(*cp==',') cp++;
	while(*cp) {
		if(*cp < '0' || *cp > '9' || i > 99)
			return(0);
		i =* 10;
		i =+ *cp++ - '0';
	}
	return(i);
}
