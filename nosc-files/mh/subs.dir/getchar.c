getchar()
{
	int c;

	c = 0;
	if(read(0, &c, 1) != 1)
		return(0);
	return(c);
}
