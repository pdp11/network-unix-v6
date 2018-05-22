showfile(file)
{
	register int i, cnt;
	char buf[512];

	type(1, "\n");
	if((i = open(file, 0)) == -1) {
		printf("Can't open %s\n", file);
		return 1;
	}
	do
		if((cnt = read(i, buf, sizeof buf)) > 0)
			write(1, buf, cnt);
	while(cnt == sizeof buf);
	close(i);
	type(1, "\n");
	return 0;
}
