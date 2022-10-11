/*
 * str = scopyn (src1, src2, src3, ..., dest, &dest[lastbyte], 0);
 * Copies strings (src1, src2, etc.) into destination string (dest),
 * truncating if necessary. Returns a pointer to the null.
 * If the source strings do not fit, they will be truncated.
 * The last byte of dest will still contain a null.
 *
 * Modified to not accept -1 as well as 0 for end-of-list indicator,
 * since -1 is a valid address (and its acceptance caused a
 * bizarre bug). BBN:dan Dec 8 1980
 */
char *
scopyn (args)
    char *args; /* first argument (lowest in stack) */
{
    register char **argp; /* pointer to argments in stack */
    char *dest; 	  /* pointer to destination string */
    char *end;		  /* pointer to end of destination */
    char **firstsrc;	  /* pointer to first source string pointer */
    char **lastsrc;	  /* pointer to last source string pointer */
    register char *cp;	  /* pointer into destination buffer */
    extern char *scopy();

/* find destination and end marker arguments */

    firstsrc = &args; /* pointer to first source pointer */

    for (argp = firstsrc; *argp != 0; argp++)
	continue;

    end = *(--argp);  /* back up to end pointer */
    dest = *(--argp); /* back up to destination pointer */
    lastsrc = --argp; /* save pointer to last source pointer */

    cp = dest; /* transfer strings to destination */
    for (argp = firstsrc; argp <= lastsrc; argp++)
	cp = scopy(*argp, cp, end);

    return(cp);
}
