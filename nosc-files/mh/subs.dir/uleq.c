uleq(s1, s2)
{
	register char *c1, *c2;
	register int c;

	c1 = s1; c2 = s2;
	while(c = *c1++)
		if(c != *c2 && (c =| 040) != *c2)
			return(0);
		else
			c2++;
	return(*c2 == 0);
}
