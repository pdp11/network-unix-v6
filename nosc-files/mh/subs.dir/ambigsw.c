ambigsw(arg, swp)
char *arg;
{
	printf("%s: ", invo_name());
	printf("-%s ambiguous.  It matches \n", arg);
	printsw(arg, swp);
}

