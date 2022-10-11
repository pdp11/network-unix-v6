/* -------------------------- S C M P ------------------------------- */
/*
 * int = scmp(str1, str2)
 * Subtracts str1 from str2; returns 0 if equal, a negative value if s1 < s2,
 * a positive value if s1 > s2
 */
int
scmp(s1, s2)
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
   return(*p1 - *p2);
   }
