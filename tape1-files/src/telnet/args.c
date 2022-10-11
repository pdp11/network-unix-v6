#define NULL 0
#define MINUS_1 ((char *)-1)

/* -------------------------- A R G S T R --------------------------- */
/*
 * Given keyword or NULL, return pointer to next arg. Expunge from arg list.
 */
char *
ArgStr(kw, deflt, argv)
    char * kw;
    char * deflt;
    char * argv[];
{
    register char ** ap;
    char * arg;

    for (ap = &argv[1]; *ap != 0 && *ap != MINUS_1; ap++)
	if (kw == NULL)
	{
	    arg = *ap;
	    ArgExpunge(ap);
	    return(arg);
	}
	else if ( strcmp(kw, *ap) == 0 )
	{
	    ap++;
	    if (*ap == 0 || *ap == MINUS_1)
		rcmderr(0, "%s requires an argument.\n", kw);
	    arg = *ap;
	    ArgExpunge(ap);
	    ArgExpunge(--ap);
	    return(arg);
	}
    return(deflt);
}

/* -------------------------- A R G I N T --------------------------- */
/*
 * Given keyword or NULL, return following int. Expunge from arg list.
 */
ArgInt(kw, deflt, argv)
    char * kw;
    int deflt;
    char * argv[];
{
    register char ** ap;
    int out;
    extern char * atoiv();

    for (ap = &argv[1]; *ap != 0 && *ap != MINUS_1; ap++)
	if (kw == NULL)
	{
	    if (*atoiv(*ap, &out) != '\0')
		rcmderr(0, "\"%s\" is not a number.\n", *ap);
	    ArgExpunge(ap);
	    return(out);
	}
	else if (strcmp(kw, *ap) == 0)
	{
	    ap++;
	    if (*ap == 0 || *ap == MINUS_1)
		rcmderr(0, "%s requires a numeric argument.\n", kw);
	    if (*atoiv(*ap, &out) != '\0')
		rcmderr(0, "\"%s\" is not a number.\n", *ap);
	    ArgExpunge(ap);
	    ArgExpunge(--ap);
	    return(out);
	}
    return(deflt);
}

/* -------------------------- A R G L O N G ------------------------- */
/*
 * Given keyword or NULL, return following long. Expunge from arg list.
 */
long
ArgLong(kw, deflt, argv)
    char * kw;
    long deflt;
    char * argv[];
{
    register char ** ap;
    long out;
    extern char * atolv();

    for (ap = &argv[1]; *ap != 0 && *ap != MINUS_1; ap++)
	if (kw == NULL)
	{
	    if (*atolv(*ap, &out) != '\0')
		rcmderr(0, "\"%s\" is not a number.\n", *ap);
	    ArgExpunge(ap);
	    return(out);
	}
	else if (strcmp(kw, *ap) == 0)
	{
	    ap++;
	    if (*ap == 0 || *ap == MINUS_1)
		rcmderr(0, "%s requires a numeric argument.\n", kw);
	    if (*atolv(*ap, &out) != '\0')
		rcmderr(0, "\"%s\" is not a number.\n", *ap);
	    ArgExpunge(ap);
	    ArgExpunge(--ap);
	    return(out);
	}
    return(deflt);
}

/* -------------------------- A R G F L A G ------------------------- */
/*
 * Given keyword, return its index in the argument array, or 0 if not found.
 */
ArgFlag(kw, argv)
    char * kw;
    char * argv[];
{
    register char ** ap;

    for (ap = &argv[1]; *ap != 0 && *ap != MINUS_1; ap++)
	if (strcmp(kw, *ap) == 0)
	{
	    ArgExpunge(ap);
	    return(ap - &argv[0]);
	}
    return(0);
}

/* -------------------------- A R G C O U N T ----------------------- */
/*
 * Return new value for argc following a series of calls on the routines above.
 * This routine returns the total number of arguments in the array,
 * including argv[0]. In other words, given an argv straight from
 * main(argc, argv), its return value will be equal to argc.
 */
ArgCount(argv)
    char * argv[];
{
    register char ** ap;
    register int i = 0;

    for (ap = &argv[0]; *ap != 0 && *ap != MINUS_1; ap++)
	i++;
    return(i);
}

/* -------------------------- A R G E X P U N G E ------------------- */
/*
 * Expunge the argument pointed to.
 */
ArgExpunge(ap)
    register char ** ap;
{
    while (*ap != 0 && *ap != (char *)-1)
    {
	*ap = *(ap + 1);
	ap++;
    }
}

/* ------------------------------------------------------------------ */
