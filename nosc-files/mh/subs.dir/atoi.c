atoi(str)
{
	register char *cp;
	register int i;

	i = 0;
	cp = str;
	while(*cp >= '0' && *cp <= '9') {
		i =* 10;
		i =+ *cp++ - '0';
	}
	return(i);
}
