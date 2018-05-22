getcpy(str)
{
	register char *cp;

	cp = alloc(length(str) + 1);
	copy(str, cp);
	return(cp);
}
