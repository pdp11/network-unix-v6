/*
 * char *dotpath(relpath, low, top)
 * Take any legal pathname, relpath, and put the equivalent most
 * reduced pathname that can be obtained without accessing any
 * file systems in the area pointed to by low, but not exceeding top.
 * A pointer to the null of the resultant string is returned.
 *
 * The resultant string starts with one of "/", ".", or "...".
 * If it starts with "/", all occurences of "." and ".." have been
 * removed (there was no "...") and the pathname is absolute and
 * condensed as much as possible.  If it starts with "." or "...",
 * all other occurences of "." or "..." have been removed, as have
 * all occurences of ".." which would not involve backing past the
 * initial "." or "...".  In any case, all multiple adjacent slashes
 * have been reduced to one, and any trailing slash has been removed.
 *
 * There are no possible errors except lack of space to put the string.
 */

char   *dotpath (relpath, low, top)
char   *relpath;
char   *low;
char   *top;
{
    register char  *cp,
                   *path;
    char   *ep,
           *slash,
           *root,
           *dot,
           *dotdot,
           *dotdotdot;
    char   *start;
    int slength (), smatch (), stokeq ();
    char *sskip (), *sfind (), *scopy (), *safter ();
    char *sbskip (), *sbfind (), *sbcopy ();

    root = "/";
    slash = "/";
    dot = ".";
    dotdot = "..";
    dotdotdot = "...";
    cp = relpath;
    path = low;
    if (smatch (cp, root) == slength (root))
    {
	start = path;
	path = scopy (root, path, top);
	cp = sskip (safter (cp, root), slash);
    }
    else
    {
	path = scopy (dot, path, top);
	path = scopy (slash, path, top);
	start = path;
    }
    ep = path;
    while (*cp)
    {
	if (stokeq (cp, slash, dot))
	{		     /* just ignore dot */
	    cp = sskip (sfind (cp, slash), slash);
	}
	else
	    if (stokeq (cp, slash, dotdot)
		    && (path > start) && !stokeq (ep, slash, dotdot))
	    {		     /* back up */
		cp = sskip (sfind (cp, slash), slash);
		path = ep;
		if (sbskip (ep, low, slash) > start)
		{
		    ep = sbfind (sbskip (ep, low, slash), low, slash);
		}
	    }
	    else
		if (stokeq (cp, slash, dotdotdot))
		{	     /* start over */
		    cp = sskip (sfind (cp, slash), slash);
		    path = scopy (dotdotdot, low, top);
		    start = ep = path = scopy (slash, path, top);
		}
		else
		{
		    ep = path;
		    path = scopy (cp, path, &path[sfind (cp, slash) - cp]);
		    if (!seq (low, root))
			path = scopy (slash, path, top);
		    cp = sskip (sfind (cp, slash), slash);
		}
	*path = 0;
    }
    if (!seq (low, root))
    {			     /* wipe out trailing slash */
	path = sbskip (path, low, slash);
	*path = 0;
    }
    return (path);
}
