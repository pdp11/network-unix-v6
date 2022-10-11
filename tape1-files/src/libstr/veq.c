/* -------------------------- V E Q --------------------------------- */
/*
 * Compare the first N chars of two char arrays for equality.
 * Return nonzero if equal, zero if not.
 */
veq(a, b, n)
    char * a;
    char * b;
    int n;
{
    register i;

    for (i = 0; i < n; i++)
	if (*a++ != *b++)
	    return(0);
    return(1);
}
