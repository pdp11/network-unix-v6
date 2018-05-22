#include "/h/net/openparms.h"

char *hosts[] {	/* list of hosts to be probed */
	"isia",
	"isib",
	"isic",
	"isid",
	"isie",
	"sri-kl",
	"sri-ka",
	"bbn",
	"bbna",
	"bbnb",
	"bbnc",
	"bbnd",
	"bbne",
	0
};

struct months {
	int adjust;	/* one less than the number of days prior to this
				month in a leap year */
	char mo[3];	/* first three characters of month-name */
} months[12] {
	 -1, 'ja', 'n',
	 30, 'fe', 'b',
	 59, 'ma', 'r',
	 90, 'ap', 'r',
	120, 'ma', 'y',
	151, 'ju', 'n',
	181, 'ju', 'l',
	212, 'au', 'g',
	243, 'se', 'p',
	273, 'oc', 't',
	304, 'no', 'v',
	334, 'de', 'c'
};
/**/
main(argc, argv)
int argc;
char **argv;
{
	register char **p;
	register struct months *q;
	register a0;
	int a1, a2, day, year, hour, min, sec;
	long tyme;

	for(;;) for(p = hosts; *p; p++) if(get_date(*p)) {

		/* We've retrieved a date from some host; now parse it */

		while(get_char() != ' ');	/* skip past day-of-week */
		day = get_num(0);	/* reads number and delimiter */
		a0 = get_char();  a1 = get_char();  a2 = get_char();
		for(q = months; q < &months[12]; q++)
			if(q->mo[0] == a0 && q->mo[1] == a1 && q->mo[2] == a2) {
				day =+ q->adjust;
				goto more;
			}
		continue;	/* Couldn't crack it -- try another host */

		/* day now contains the zero-relative day in a leap year */
	more:
		while(get_char() != ' ');	/* skip rest of month */
		year = get_num(0) - 70;	/* read year and delimiter */
		if(day > 59 && (year+2)&3)day--; /* adjust for non-leap year */
		hour = get_num(2);  min = get_num(2); sec = get_num(2);
		if(get_char() != '-') continue;	/* Must be hyphen before zone */
		switch(get_char()) {	/* Convert hour to GMT */
		case 'p':  hour =+ 8;  break;	/* Pacific */
		case 'm':  hour =+ 7;  break;	/* Mountain */
		case 'c':  hour =+ 6;  break;	/* Central */
		case 'e':  hour =+ 5;  break;	/* Eastern */
		case 'a':  hour =+ 4;  break;	/* Atlantic */
		case 'y':  hour =+ 9;  break;	/* Yukon */
		case 'h':  hour =+ 10; break;	/* Hawaiian */
		default: continue;
		}
		if(get_char() == 'd') hour--;	/* Daylight savings */

		/* Calculate the number of seconds since the Epoch */

		tyme = year*365 + (year+1)/4 + day;
		tyme = tyme*24 + hour;
		tyme = tyme*60 + min;
		tyme = tyme*60 + sec;

		stime(&tyme);
		printf("***** Time set from host %s to %s", *p, ctime(&tyme));
		return;		/* That's all! */
	}
}

char hostname[50] {"/dev/net/\0\0\0"};
char buf[50];	/* space in which to accumulate text of date */
int len, indx;

get_date(host)
char *host;
{
	register char *p, *q;
	register i;
	struct openparms parms;
	int fi;

	p = &hostname[9];  q = host;
	while(*p++ = *q++);
	parms.o_type =o_init | o_duplex;
	parms.o_id = 0;
	parms.o_lskt = 0;
	parms.o_fskt[0] = 0;
	parms.o_fskt[1] = 015;
	parms.o_frnhost = 0;
	parms.o_bsize = 8;
	parms.o_nomall = 50;
	parms.o_timeo = 20;
	parms.o_relid = 0;
	if((fi = open(hostname, &parms)) < 0) return 0;
	len = 0;
	while((i = read(fi, &buf[len], sizeof buf - len)) > 0) len =+ i;
	close(fi);
	if(i<0) perror(host);
	if(buf[len-1] != '\n' || buf[len-2] != '\r' || buf[len-3] != 'T') return 0;
	indx = 0;
	return 1;
}

char get_char()
{
	register t;

	if(indx >= len) return 0;
	if((t = buf[indx++]) >= 'A' && t <= 'Z') t =+ 'a' - 'A';
	return t;
}

get_num(max)
int max;
{
	register val, t;

	while((val = get_char() - '0') < 0 || val > 9);
	while(--max) {
		if((t = get_char() - '0') < 0 || t > 9) break;
		val = val*10 + t;
	}
	return val;
}
