/*****************************************************************************
* vcmp - string comparison on first length chars of s1 and s2                *
* returns <0 if s1 < s2, =0 if s1 IS s2, >0 if s1 > s2                       *
*****************************************************************************/

vcmp (s1, s2, length)
    register char *s1;
    register char *s2;
    register length;
    {
    while (length-- > 0 && *s1++ == *s2++)
	;
    return (*--s1 - *--s2);
    }
