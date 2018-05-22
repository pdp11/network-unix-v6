/*
	cc -O -s nice_telnet.c; su.cp a.out /etc/bin/nice_telnet
*/
main(argc, argv)
int argc;
char **argv;
{
	char host[21];

	nice(127);	/* set priority way down */
	write(1, "Host: ", 6);
	host[read(0, host, 20) - 1] = 0;	/* null-terminate name */
	execl("/usr/bin/telnet", "-Telnet", host, 0);	/* make connection */
}
