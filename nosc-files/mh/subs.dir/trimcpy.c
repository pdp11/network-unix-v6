trimcpy(str)
{
	register char *cp, *sp;

	cp = str;
	while(*cp == ' ' || *cp == '\t')
		cp++;
	sp = cp;
	while(*sp)
		if(*sp++ == '\n')
			break;
	*--sp = 0;
	sp = alloc(sp - cp + 1);
	copy(cp, sp);
	return(sp);
}
