/*
 * sunslash(in) looks for C-style escapes in the input.
 * It recognizes and converts \b, \n, \r, \t, \f,
 * and \# (octal number). It returns a pointer to its output
 * string, which is allocated on the string stack.
 */

#include "string.h"
extern struct sctrl _ScB;

char *
sunslash(in)
register char *in;
{
    register char *out;
    int len;
    char *aout;

    len = slength(in);
    get(len + 1, aout);

    for (out = aout; ; in++, out++)
    {
	switch (*in)
	{
	case '\0':
	    *out = '\0';
	    goto DONE;
	case '\\':
	    switch (*++in)
	    {
	    case 'b':
		*out = '\b';
		break;
	    case 'n':
		*out = '\n';
		break;
	    case 'r':
		*out = '\r';
		break;
	    case 't':
		*out = '\t';
		break;
	    case 'f':
		*out = '\f';
		break;
	    case '\0':
		*out = '\0';
		goto DONE;
	    default:
		if ('0' <= *in && *in <= '7')
		{
		    *out = 0;
		    do
			*out = *out * 8 + (*in++ - '0');
		    while ('0' <= *in && *in <= '7');
		    in--;
		}
		else
		    *out = *in;
		break;
	    }
	    break;

	default:
	    *out = *in;
	    break;
	}
    }
DONE:
    _ScB.c_sp = out + 1;
    return(aout);
}
