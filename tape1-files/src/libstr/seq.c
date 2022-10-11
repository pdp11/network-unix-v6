/* -------------------------- S E Q --------------------------------- */
/*
 * int = seq(str1, str2)
 * Compares str1 and str2; returns 1 if equal, 0 if not
 * fixed 2.15.80 to handle null pointers gracefully - rwells, bbn.
 */
int seq (cp1, cp2)
    register char *cp1, *cp2;
    {
    /* if pointers are the same, no need to look at chars */
    if (cp1 == cp2)
	return (1);

    /* if one pointer is null, they are obviously different */
    /* (we could consider a null pointer equal to an empty string...) */
    /* (but why bother;  we just want to protect against null ptrs) */
    if (cp1 == 0 || cp2 == 0)
	return (0);

    while (*cp1 == *cp2++)
        if (*cp1++ == 0)
            return(1);
    return(0);
    }
