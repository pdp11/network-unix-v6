#include "stdio.h"
/* -------------------------- _ C M D E R R ------------------------- */
/*
 * _cmderr(func, errno, progname, printargs) writes an error message
 * of the form
 *	progname: user_info errmsg.
 * on file descriptor 2. "errmsg" is obtained from the "errno"
 * argument; if "errno" is zero, it is null. "user_info" is printed by passing
 * the vector "printargs" on to printf. Whatever terminates the
 * user_info string (\r\n or \n) is moved to the end of the entire message.
 */
_cmderr(progname, eno, printargs)
    int eno;
    char * printargs[];
{
    register char * term;
    char * format;
    register char * fmt_end;
    char *errmsg();

    if (progname != 0)
	fprintf(stderr, "%s: ", progname);

    if (eno != 0)
    {
	format = printargs[0];
	fmt_end = &format[slength(format)-1];
	if (fmt_end[0] == '\n' && fmt_end[-1] == '\r')
	{
	    fmt_end[-1] = '\0';
	    term = "\r\n";
	}
	else if (fmt_end[0]  == '\n')
	{
	    fmt_end[0] = '\0';
	    term = "\n";
	}
	else
	    term = "";
    }
    fprintf(stderr, "%r", printargs);
    if (eno != 0)
    {
	fprintf(stderr, " %s.%s", errmsg(eno), term);
	if (seq(term, "\n"))
	    fmt_end[0] = '\n';
	else if (seq(term, "\r\n"))
	{
	    fmt_end[-1] = '\r';
	    fmt_end[0] = '\n';
	}
    }
}

/* -------------------------- C M D E R R --------------------------- */
/*
 * Call _cmderr.
 */

/* VARARGS2 */
cmderr(eno, pfargs)
    int eno;
    char * pfargs;
{
    extern char * progname;

    _cmderr(progname, eno, &pfargs);
}

/* -------------------------- E C M D E R R ------------------------- */
/*
 * Like cmderr, but exit afterwards.
 */

/* VARARGS2 */
ecmderr(eno, pfargs)
    int eno;
    char *pfargs;
{
    extern char * progname;

    _cmderr(progname, eno, &pfargs);
    if (eno == 0)
	exit(-1);
    else
	exit(eno);
}

/* -------------------------- R C M D E R R ------------------------- */
/*
 * Like cmderr, but reset afterwards.
 */

/* VARARGS2 */
rcmderr(eno, pfargs)
    int eno;
    char * pfargs;
{
    extern char * progname;

    _cmderr(progname, eno, &pfargs);
    reset(1);	/* The argument is required by VAX version of reset (arrgh!) */
}

/* -------------------------- A C M D E R R ------------------------- */
/*
 * Like cmderr, but aborts afterwards.  This routine calls
 * '_cleanup()' to flush buffers before the abort.
 */

/* VARARGS2 */
acmderr(eno, pfargs)
int   eno;
char *pfargs;
{
    extern char * progname;

    _cmderr(progname, eno, &pfargs);
    _cleanup();
    abort();
}


/* ------------------------ _ A C M D E R R ------------------------- */
/*
 * Like cmderr, but aborts afterwards.  This routine has a companion
 * function 'acmderr()' in a file of that name.  That function is 
 * is similar, but does a '_cleanup()' to flush buffers before the
 * abort.  It is not included here because some people are not using
 * stdio.
 */

/* VARARGS2 */
_acmderr(eno, pfargs)
int   eno;
char *pfargs;
{
    extern char * progname;

    _cmderr(progname, eno, &pfargs);
    fflush(stderr);                     /* Make sure the error gets printed */
    abort();
}


char * progname;    /* Simplifies its use in programs */
