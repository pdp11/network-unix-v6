# include  "rm.h"




/*
 *     routine which reads a line from the indicated file and fills the array
 *     'fields' with pointers to the blank separated strings.  returns the
 *     number of such strings found if no errors.  error returns:
 *
 *     -1  --  eof or error on file read
 *     -2  --  line length exceeds maximum
 *     -3  --  number of fields exceeds maximum
 *     -4  --  end quote missing from quoted string
 */

parselin(fields, linebuff, filebuf, maxfields, maxline)
  char  *fields[], linebuff[];
  struct iobuf  *filebuf;
  int  maxfields, maxline;
    {
    register char  *cp;
    register int  nfields, count;
    int  c;

    count = 0;
    nfields = 0;
    cp = linebuff;

/*  first read in a line from the file.  it can't be longer than MAXLINE  */
/*  even if it is too long, read in the whole thing                       */

    while (1)
      {
      c = getc(filebuf);

      if (c == -1)
        return(-1);

      if (c == '\n')
        if (count >= MAXLINE)
          return(-2);
        else
          {
          *cp = '\n';
          count++;
          break;
          }

/*  'c' is a normal character.  if we haven't exceeded the buffer capacity  */
/*  keep it                                                                 */

      if (count < MAXLINE)
        {
        *cp++ = c;
        count++;
        }
      }

/*  now find the strings on the line that are separated by blanks  */

    cp = linebuff;

    while (1)
      {
      while ((*cp == ' ') || (*cp == '\t'))
        *cp++ = '\0';

      if (*cp == '\n')
        {
        *cp = '\0';
        return(nfields);
        }

/*  save the pointer to the start of the field  */

      if ((nfields + 1) > MAXFIELDS)
        return(-3);

      fields[nfields++] = cp;

/*  special case for quoted strings.  embedded blanks are ignored.  must  */
/*  find another '"' before the newline                                   */

      if (*cp == '"')
        {
        cp++;

        while ((*cp != '"') && (*cp != '\n'))
          cp++;

        if (*cp == '\n')
          return(-4);

        cp++;
        }

      else
        while ((*cp != ' ') && (*cp != '\t') && (*cp != '\n'))
          cp++;
      }
    }




/*
 *     the purpose of this routine is to fork, retrying 10 times if necessary
 *     -1 is returned if failure after all retries
 */

tryfork()
    {
    register int  try, pid;

    for (try = 0; try <= 9; try++)
      {
      pid = fork();

      if (pid != -1)
        return(pid);
      }

    return(-1);
    }




/*
 *     routine which returns the length of a string
 */

length(string)
  char  *string;
    {
    register char  *p;
    register int  count;

    count = 0;
    p = string;

    while (*p++)
      count++;

    return(count);
    }




/*
 *     this routine copies a string from 'in' to 'out'
 */

char  *cpstr(in, out)
  char  *in, *out;
    {
    register char  *a, *b;

    a = in;
    b = out;

    while (*a)
      *b++ = *a++;

    *b = '\0';
    return(b);
    }
