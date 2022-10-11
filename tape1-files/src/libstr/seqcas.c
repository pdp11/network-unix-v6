/*****************************************************************************
* seq_nocase (str1, str2) - like seq, but upper and lower case equivalent    *
* Returns 1 if the strings are equal, and 0 if not.                          *
*$$$$$ entered by rwells 5-may-80, not tested yet $$$$$$                     *
*****************************************************************************/

#define upper(c)        (('a' <= (c) && (c) <= 'z') ? (c) - 'a' + 'A' : (c))

seq_nocase (str1, str2)
    register char *str1;
    register char *str2;
    {
    while (upper (*str1) == upper (*str2))
	{
	if (*str1++ == '\0')  /* exact match */
	    return (1);
	str2++;
	}
    return (0);  /* not a match */
    }
