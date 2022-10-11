/* -------------------------- S R E V E R S E ----------------------- */
/*
 * sreverse(string) returns the reverse of its argument. It allocates
 * a new copy for the purpose.
 */
char *
sreverse(as)
    char *as;
{
    register char *s;
    register char *t;
    register int len;
    char *r;
    extern char *sauto();

    s = as;
    len = slength(s);
    r = t = sauto(len);
    s += (len-1);
    while (len--)
	*t++ = *s--;
    *t = '\0';
    return(r);
}
