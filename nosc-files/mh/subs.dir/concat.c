concat(args)
int args[];
{
	register char **a;
	register char *cp;
	register int len;

	len = 1;
	for(a = &args; *a; )
		len =+ length(*a++);
	len = cp = alloc(len);
	for(a = &args; *a; )
		cp = copy(*a++, cp);
	return(len);
}
