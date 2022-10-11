
/* -------------------------- C O M _ E R R -------------------- */
/*
 * com_err(errno, format[, args...]) writes an error message of the form
 *      progname: errmsg. user_info
 * on file descriptor 2. "progname" is obtained from the string of the same
 * name, which should be set to argv[0]. "errmsg" is obtained from the "errno"
 * argument; if "errno" is zero, it is null. "user_info" is printed by passing
 * the arguments from "format" on to printf. A CRLF is added at the end.
 */
com_err(eno, pfargs)
    int eno;
    char *pfargs;
{
    char *errmsg();
    extern char *progname;

    if (eno)
        fdprintf(2, "%s: %s. %r", progname, errmsg(eno), &pfargs);
    else
        fdprintf(2, "%s: %r", progname, &pfargs);
    fdprintf(2, "\r\n");
}

/* -------------------------- F A T A L ------------------------------- */
/*
 * fatal() acts like com_err(), then closes the net and exits.
 */
fatal(eno, pfargs)
    int eno;
    char *pfargs;
{
    char *errmsg();
    extern char *progname;

    if (eno)
        fdprintf(2, "%s: %s. %r", progname, errmsg(eno), &pfargs);
    else
        fdprintf(2, "%s: %r", progname, &pfargs);
    fdprintf(2, "\r\n");

    quit(1, 1, 0);
    exit(-1);
}
