/* -------------------------- S M A T C H --------------------------- */
/*
 * index = smatch(str1, str2)
 * Compares str1 with str2. Returns the index of the first
 * different char.
 */
int
smatch(s1, s2)
   char *s1;
   char *s2;
   {
   register char *p1, *p2;

   p1 = s1;
   p2 = s2;

   while(*p1 && *p1 == *p2)
      {
      p1++;
      p2++;
      }
   return(p1 - s1);
   }
