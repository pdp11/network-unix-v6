/*
 * C library - atolv: get a long
 * from a string and return it where the second argument
 * points (with pointer to next char returned directly).
 * Atolong is like atolv except third argument
 * is base if non-zero (0 defaults to octal on leading zero, else decimal).
 */

char *atolv (astring, apointer)
char *astring;
long *apointer;
{
  extern char *atolong ();
  return (atolong (astring, apointer, 0));
}

char *atolong (astring, apointer, abase)
char *astring;
long *apointer;
int   abase;
{
  register char *pointer;
  long	number;
  register int	base;
  int	negative;

  for (pointer = astring; *pointer == ' ' || *pointer == '\t'; pointer++)
    ;
  for (negative = 0; *pointer == '-'; pointer++)
    negative++;
  if (abase != 0)
    base = abase;
  else
  {
    base = 10;
    if (*pointer == '0')
    {
      base = 8;
      pointer++;
      if ((*pointer == 'x') || (*pointer == 'X'))
      {
	base = 16;
	pointer++;
      }
    }
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
