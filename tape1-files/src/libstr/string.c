/*
 * string library function which converts a char to a string
 *
 * Algorithm: set aside an array for most common chars.
 * If any others appear, allocate enough for all possible
 * chars. The current configuration makes all ASCII chars fast.
 *
 * Written by Dan Franklin, BBN, March 5, 1981.
 */

#include <cpu.h>
#define NULL 0

/*
 * Characters in the range 0 to NCHARS-1 are handled quickly;
 * characters in the range NCHARS to BYTEMASK-1 are handled with
 * big alloc; BYTEMASK itself is also quick, since it is the most
 * likely non-ASCII char.
 */

#define NCHARS 128

static char strings[NCHARS][2]; /* For quick code */

static char (*more)[BYTEMASK-NCHARS][2]; /* For slow code */
/* "more" is a pointer to an array of the same format as "strings" */

static char biggest[2] = { BYTEMASK, 0 };

char *
string(c)
    char c;
{
    register int ch;
    register char *cp;
    extern char *sbrk();

    ch = c & BYTEMASK;
    if (ch >= 0 && ch < NCHARS)
	cp = &strings[ch][0];
    else if (ch == BYTEMASK)
	cp = &biggest[0];
    else
    {
	if (more == NULL)
	{
	    more = (char (*)[BYTEMASK-NCHARS][2]) sbrk(sizeof(*more));
	    if (more == NULL)
	    {
		_serror("string(): out of address space.");
		return(biggest);
	    }
	}
	cp = &( (*more)[ch - NCHARS][0] );
    }
    cp[0] = c;
    cp[1] = '\0';
    return(cp);
}
