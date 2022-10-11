/*
 * sappend -- Like scopy, but append source
 * to end of destination.
 */
char *
sappend(in, out, outend)
    register char *in;
    register char *out;
    register char *outend;
{
    while (*out != '\0')
	out++;
    while (*in != '\0' && (out < outend || outend == (char *)0))
	*out++ = *in++;
    *out = '\0';
    return(out);
}
