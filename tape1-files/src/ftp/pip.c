  {     "PIPEGET",         ARG1,           rmtfil,  f_pget,
	"retrieve file from foreign host onto standard output",     },
  {     "PIPESEND",        ARG1,           locfil,  f_psnd,
	"send standard input to foreign host file",     },

FILE *inpip = NULL, *outpip = NULL;

char ** argp;

	if (argc > 3)
	{
	    argp = argv + 2;
	    if (strcmp(*argp, "-i") == 0)
	    {
		inpip = fdup (stdin);
		freopen (*(argp++), "r", stdin);
		argp ++;
		argc -= 2;
	    }
	    if (argc > 3)
	    {
		if (strcmp(*argp, "-o") == 0)
		{
		    outpip = fdup (stdout);
		    freopen (*(++argp), "w", stdout);
		}
	    }
	}
/*  */
f_psnd()
{
    if (inpip != NULL)
    {
	fdfil = inpip;
	inpip = NULL;
	putdat(arg1, fclose);
    }
    else error ("-i command not used to redirect input");
}
f_pget()
{
    if (outpip != NULL)
    {
	fdfil = outpip;
	outpip = NULL;
	getdat(arg1, fclose);
    }
    else error ("-o command was not used to redirect output");
}
/*  */
getdat(arg, close)
char *arg;
int *close();
{
    errno = 0;
    putcmd("RETR");
    putstr (arg);
    sndcmd ();
    if (rwait (0) != 0)
    {
	if (chekds (0))
	{
	    (*close) (fdfil);
	    fdfil = NULL;
	    net_close (&dsfdr);
	    if (errno == EINTR)
		abterr ();
	}

	rcvdata (&dsfdr, fdfil, logfn);
	net_close (&dsfdr);
    }
    (*close) (fdfil);
    fdfil = NULL;
    return rwait (4);
}
/*  */
putdat(arg, close)
char *arg;
int *close();
{
    errno = 0;
    putcmd ("STOR");
    putstr (arg);
    sndcmd ();

    if (rwait (0) != 0)
    {
	if (chekds (1))
	{
	    (*close) (fdfil);
	    fdfil = NULL;
	    net_close (&dsfds);
	    if (errno == EINTR)
		abterr ();
	}

	if (senddata (fdfil, &dsfds, logfn))
	    net_pclose (&dsfds);
	else
	    net_close (&dsfds);
	rwait (4);
    }
    if (fdfil != NULL)
    {
	(*close) (fdfil);
	fdfil = NULL;
    }
    return 0;
}
