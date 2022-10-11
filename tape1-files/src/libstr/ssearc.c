/* -------------------------- S S E A R C H ------------------------- */
/*
 * ssearch(string, cclass) searches string for any of the characters
 * in cclass. Returns the index of the first occurrence in string, or -1.
 */
ssearch(str, ccl)
   char *str;
   char *ccl;
   {
   register char *p, *q;

   for (p = str; *p; p++)
      for (q = ccl; *q; q++)
         if (*p == *q)
            return(p - str);
   return(-1); /* Didn't match */
   }
