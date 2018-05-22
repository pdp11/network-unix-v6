#include "mailer.h"

struct iobuf    namebuf,
		resbuf;
struct adr adr;
int     infd;                     /* handle on source text        */
char   *txtname;
char   *sender;
char *eosstr;

initcom (argc, argv)
int     argc;
char   *argv[];

/*      [name,] infd, namebuf.fildes, resbuf.fildes, logbuf.files,]
		txtfile , sender */
{
    infd = atoi (argv[1]);
    namebuf.fildes = atoi (argv[2]);
    resbuf.fildes = atoi (argv[3]);
    initlog (atoi (argv[4]));
    txtname = strdup (argv[5]);         /* Name of text file */
    sender = strdup (argv[6]);
    if (infd == 0 && (infd = open (txtname, 0)) == -1)
    {                                   /* probably in debug mode       */
	perror ("Unable to open first message file \"%s\"", txtname);
	putresp (NOTOK, "Unable to open first message file.\n");
	exit (-1);
    }
}

getname()
{
    int c;
    char *ptr;

    zero (&adr, sizeof adr);
    c = gcread (&namebuf, &adr, sizeof adr - 1, "\000\n\377");
    switch (c)
    {
     case NOTOK:
	perror ("error reading name from pipe\n");
	putresp (NOTOK, "error reading name from pipe\n");
	return (NOTOK);
     case OK:                       /* closed the pipe / all done       */
	dbglog ("getname: returning OK -> NOTOK");
	return (NOTOK);
     case 1:                        /* only sent a newline/null */
	dbglog ("getname: returning DONE");
	return (DONE);              /* This list is done        */
    }
    ptr = &adr;
    ptr[c] = '\0';
    dbglog ("getname got: \"%s\"", ptr);
    return (strequ (eosstr, ptr) ? DONE : OK);
}

putresp (resp, str)
    int resp;
    char *str;
{
    dbglog ("putresp: '%d' \"%s\"", resp, (str == 0) ? "" : str);
    putc (resp, &resbuf);
    if (str != 0)
	puts (str, &resbuf);
    putc ('\0', &resbuf);
    fflush (&resbuf);
}

newfile ()
{
    int c;

    if (txtname != 0)
    {
	free (txtname);
	txtname = 0;
    }
    if (sender != 0)
    {
	free (sender);
	sender = 0;
    }
    dbglog ("newfile:");
    switch (getname ())         /* name of next message file            */
    {
     case NOTOK:                /* normal end or i/o error; never sure  */
     case DONE:
/*      putresp (OK, 0);        ** normal end                   */
	return (DONE);
     case OK:
	break;
     default:
	return (NOTOK);
    }
    txtname = strdup (&adr);
    c = strlen (txtname) - 1;
    if (txtname[c] == '\n')
	txtname[c] = '\0';
    close (infd);
    if ((infd = open (txtname, 0)) == -1)
    {
	putresp (NOTOK, "Unable to open next message file.");
	perror ("Unable to open message file.\n");
	return (NOTOK);
    }
    putresp (OK, 0);

    switch (getname ())         /* name of return mailbox       */
    {
     case OK:
	break;
     case DONE:
     case NOTOK:
     default:
	putresp (NOTOK, "eof/pipe error reading return address.");
	perror ("eof/pipe error reading return address.");
	return (NOTOK);
    }
    sender = strdup (&adr);
    c = strlen (sender) - 1;
    if (sender[c] == '\n')
	sender[c] = '\0';

    putresp (OK, 0);
    return (OK);
}

infdsize ()
{
    struct inode    inode;

    return (fstat (infd, &inode) < OK ? NOTOK : (inode.fsize >> 9));
}

