/* -------------------------- S E Q --------------------------------- */
/*
 * int = seq(str1, str2)
 * Compares str1 and str2; returns 1 if equal, 0 if not
 */
int
seq(s1, s2)
   char *s1, *s2;
   {
   register char *p1, *p2;

   p1 = s1;
   p2 = s2;
   while (*p1 == *p2++)
      if (*p1++ == 0)
         return(1);
   return(0);
   }
