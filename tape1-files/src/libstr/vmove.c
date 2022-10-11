/*****************************************************************************
* vmove (source, destination, count) -  vector copy.  Copies first count     *
*  chars from source to destination, and returns count.                      *
*****************************************************************************/

vmove (source, destination, count)
    register char *source, *destination;
    register count;
    {
    register length;

    length = count;
    while (count--)
	*destination++ = *source++;
    return (length);
    }
