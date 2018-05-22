invo_name()
{
	register int *ip;

	ip = 0;

	while(*--ip != -1)
		;
	while(*--ip < 0)
		;
	return(ip[2]);
}
