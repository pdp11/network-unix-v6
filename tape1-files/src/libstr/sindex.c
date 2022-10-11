/* -------------------------- S I N D E X --------------------------- */
/*
 * index = sindex(str1, str2)
 * Looks for first occurrence of str2 in str1.
 * Returns index of it
 * The first char in a str has an index of 0.
 * If str2 does not occur in str1, returns -1.
 */
int
sindex(s1, s2)
   char *s1;
   char *s2;
   {
   register char *p1, *p2;
   char *q1;
   char *q2;

   p1 = s1;
   p2 = s2;

   for(;;)
      {
      while(*p1 != *p2)             /* Get match on first char of str2 */
         {
         if (*p1 == 0)              /* Reached end of str1, give up */
            return(-1);
         p1++;
         }

/* Now match the whole thing */

      for (q1 = p1, q2 = p2; *q2 && *q1 == *q2; q1++, q2++)
         ;
      if (*q2 == 0)              /* End of str2, success */
         return(p1 - s1);
      p1++;         /* Go past initial matching char */
      }
   }
