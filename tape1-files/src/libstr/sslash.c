/*
 * sslash(in) returns a new string (allocated on the string stack)
 * which is like the input string, but with special
 * characters in the input converted to C-style escapes.
 * It recognizes and converts backspace, newline, CR, tab,
 * and other non-printing characters.
 */

#include "string.h"
extern struct sctrl _ScB;

char *
sslash(in)
register char *in;
{
    register char *out;
    char *aout;

    get(slength(in)*4 + 1, aout);   /* They might all become \xxx */

    for (out = aout; *in != '\0'; in++, out++)
    {
	if (*in >= ' ' && *in <= '~' && *in != '\\')
	    *out = *in;
	else
	{
	    *out++ = '\\';
	    switch (*in)
	    {
	    case '\b':
		*out = 'b';
		break;
	    case '\n':
		*out = 'n';
		break;
	    case '\r':
		*out = 'r';
		break;
	    case '\t':
		*out = 't';
		break;
	    case '\f':
		*out = 'f';
		break;
	    case '\\':
		*out = '\\';
		break;
	    default:
		*out++ = '0' + ((*in & 0300) >> 6);
		*out++ = '0' + ((*in & 0070) >> 3);
		*out   = '0' +	(*in & 0007);
		break;
	    }
	}
    }
    *out = '\0';
    _ScB.c_sp = out + 1;

    return(aout);
}
