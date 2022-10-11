main(ac, av)
char **av;
{
	register int i;
	extern char *_mapname;

	if (ac > 1)
		_mapname = av[1];
	for (i = 0; i < 100; ++i) {
		if (!_loadmap())
			ecmderr(-1, "error on %dth load\n", i);
		_freemap();
	}
	exit(0);
}
