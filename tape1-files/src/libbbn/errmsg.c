/*
 * Return a pointer to the error message
 * corresponding to the argument.
 * If the argument is 0 or -1, errno is used.
 *
 * Changed to accept -1 in addition to 0 by BBN:dan Nov 15, 1980.
 */

int	errno;
int	sys_nerr;
char	*sys_errlist[];
char *errmsg(n)
int n;
{
	register char *c;

	if (n == 0 || n == -1)
		n = errno;
	c = "Unknown error";
	if ((n >= 0) && (n < sys_nerr))
		c = sys_errlist[n];
	return(c);
}
