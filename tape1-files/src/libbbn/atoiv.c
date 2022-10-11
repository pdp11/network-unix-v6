/*
 * C library - atoiv: get an int
 * from a string and return it where the second argument
 * points (with pointer to next char returned directly).
 * Atoint is like atoiv except third argument
 * is base if non-zero (0 defaults to octal on leading zero, else decimal).
 */

char *atoiv (astring, apointer)
char *astring;
int  *apointer;
{
  extern char *atoint ();
  return (atoint (astring, apointer, 0));
}

char *atoint (astring, apointer, abase)
char *astring;
int  *apointer,
      abase;
{
  register char *pointer;
  register int	number,
		base;
  int	negative;

  for (pointer = astring; *pointer == ' ' || *pointer == '\t'; pointer++)
    ;
  for (negative = 0; *pointer == '-'; pointer++)
    negative++;
  if (abase != 0)
    base = abase;
  else
  {
    if ((pointer[0] == '0') && ((pointer[1] == 'x') || (pointer[1] == 'X')))
    {
      base = 16;
      pointer = &pointer[2];
    }
    else
      base = (*pointer == '0') ? 8 : 10;
  }
  for (number = 0; *pointer; pointer++)
  {
    if ((*pointer >= '0') && (*pointer <= '9'))
    {
      number = (number * base) + (*pointer - '0');
      continue;
    }
    if (base <= 10)
      break;
    if ((*pointer >= 'A') && (*pointer <= 'Z'))
    {
      number = (number * base) + ((*pointer - 'A') + 10);
      continue;
    }
    if ((*pointer >= 'a') && (*pointer <= 'z'))
    {
      number = (number * base) + ((*pointer - 'a') + 10);
      continue;
    }
    break;
  }
  if (negative)
    number = -number;
  *apointer = number;
  return (pointer);
}
