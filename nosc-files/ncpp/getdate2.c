#/*
Module Name:
	getdate2.c -- get date off network during reboot

Installation:
	if $1x = newerx goto newer
	if $1e = finale goto finale
	cc getdate2.c
	exit
: newer
	if ! -r /usr/net/etc/getdate exit
	if ! { newer getdate2.c /usr/net/etc/getdate } exit
	echo ncpp/getdate2.c:
: finale
	cc -O -s getdate2.c
	if ! -r a.out exit
	if ! -r /usr/sys/ncpp/getdate2.c goto same
	if { cmp -s getdate2.c /usr/sys/ncpp/getdate2.c } goto same
	su cp getdate2.c /usr/sys/ncpp/getdate2.c
	su chmod 444 /usr/sys/ncpp/getdate2.c
	rm -f getdate2.c
: same
	if -r /usr/net/etc/getdate su rm -f /usr/net/etc/getdate
	su cp a.out /usr/net/etc/getdate
	rm -f a.out
	su chmod 555 /usr/net/etc/getdate
	su chown bin /usr/net/etc/getdate
	su chgrp bin /usr/net/etc/getdate

Function:
	Get the date from one of the server hosts that support it.  Used
	during a reboot so our date agrees with the rest of the world.

Compile time parameters and effects:
	none.

Module History:
	Written Nov 77 by Greg Noel
	Modified 6 Jan 78 by Greg Noel because server dates stuttered.
*/
#include "net_open.h"

char *hosts[] {	/* list of hosts to probe */
	"su-ai",
	"ames-67",
	"mit-ai",
	"mit-ml",
	"mit-mc",
	"mit-dms",
	0
};

int date[2];
int date70[2] {0101652, 0077200};	/* seconds from 1900 to 1970 */

struct ldate {long DATE;};	/* long to overlay above for arithmetic */

char name[] "/dev/net/xxxxxxxxxxxxxxx";

main(argc, argv)
int argc;
char **argv;
{
	register netfid;
	register char *p, *q;
	char **h;
	struct openparams openparams;
	int timeout();

	for(;;) for(h = hosts; q = *h++;) {
		openparams.o_op = 0;
		openparams.o_type = o_direct|o_init;
		openparams.o_id = 0;
		openparams.o_lskt = 0;
		openparams.o_fskt[0] = 0;
		openparams.o_fskt[1] = 37;
		openparams.o_frnhost = 0;
		openparams.o_bsize = 32;
		openparams.o_nomall = 4;
		openparams.o_timeo = 20;
		openparams.o_relid = 0;
		p = name + 9;
		while (*p++ = *q++);
		signal(14, &timeout);	/* NCP timeout doesn't work, so */
		alarm(120);		/* we simulate it with alarm */
		netfid = open( name, &openparams );
		alarm(0);
		if(netfid < 0) continue;
	
		if(read(netfid, date, 4) != 4) {
			printf("%s: read not right length\n", h[-1]);
			close( netfid );
			continue;
		}
		close( netfid );
		date[0] = swab(date[0]);
		date[1] = swab(date[1]);
		date->DATE =- date70->DATE;
		stime(date);
		printf("***** Time set from %s to %s", *--h, ctime(date));
		exit();
	}
}

timeout()
{
	/* Timeout causes open to return -1; that's enuf */
}
