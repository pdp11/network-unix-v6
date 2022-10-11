/* -------------------------- S V E R I F Y ------------------------- */
/*
 * sverify(string, cclass) verifies that string consists entirely of the
 * characters in cclass. Returns the index of the first occurrence
 * in string of a non-cclass character, or -1.
 */
sverify(str, ccl)
   char *str;
   char *ccl;
   {
   register char *p, *q;

   for (p = str; *p; p++)
      for (q = ccl; *p != *q; q++)
         if (*q == 0)
            return(p - str);
   return(-1); /* Didn't match */
   }
