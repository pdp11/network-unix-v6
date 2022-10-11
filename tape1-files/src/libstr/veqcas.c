/*****************************************************************************
* veq_nocase (str1, str2, length) - like veq, but upper and lower case same  *
* Returns 1 if the strings are equal, and 0 if not.			     *
*									     *
* Modified to not use piostd.h by Dan Franklin (BBN) Dec 5 1980 	     *
* since it no longer compiles. (Source control, anyone?)		     *
*****************************************************************************/

#define upper(c)	(('a' <= (c) && (c) <= 'z') ? (c) - 'a' + 'A' : (c))

#define NULL 0

veq_nocase(str1, str2, length)
    register char *str1;
    register char *str2;
{
    register i;

    if (str1 == NULL)
	return(str2 == NULL);

    if (str2 == NULL)
	return(0);

    for (i = 0; i < length; i++, str1++, str2++)
	if (upper(*str1) != upper(*str2))
	    return(0);

    return(1);
}
