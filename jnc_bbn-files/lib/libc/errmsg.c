/*
 * Return a pointer to the error message
 * corresponding to the argument.
 * If the argument is 0, errno is used.
 */

int	errno;
int	sys_nerr;
char	*sys_errlist[];
errmsg(n)
int n;
{
	register char *c;

	if (n == 0)
		n = errno;
	c = "Unknown error";
        if(n < sys_nerr)
                c = sys_errlist[n];
	return(c);
}
