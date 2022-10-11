/*****************************************************************************
* smove (source, destination, end)                                           *
*****************************************************************************/

smove (source, destination)
	register char *source, *destination;
	{
	if (source == 0)
		{
		*destination = '\0';
		return (0);
		}
	if (destination == 0)
		return (-1);
	while (*destination++ = *source++);
	return (0);
	}
