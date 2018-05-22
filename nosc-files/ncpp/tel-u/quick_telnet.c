/*
	cc -O -s quick_telnet.c
	supr cp a.out /etc/bin/quick_telnet
	rm a.out
*/
main(argc, argv)
int argc;
char **argv;
{
	register char *host, *p, c;

	nice(127);	/* set priority way down */

		/* Extract last component of pathname */
	host = p = argv[0];
	while(c = *p++) if(c == '/') host = p;

	execl("/usr/bin/telnet", "-Telnet", host, 0);	/* make connection */
}
