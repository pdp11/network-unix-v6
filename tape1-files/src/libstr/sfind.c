/* -------------------------- S F I N D ----------------------------- */
/*
 * sfind(string, cclass) searches through string until it finds one of the
 * characters in cclass. Returns a pointer to the first occurrence in string,
 * or to the null at the end.
 * drb(4-Feb-80): added correct typing to function header.
 */
char *sfind(str, ccl)
   char *str;
   char *ccl;
   {
   register char *p, *q;

   for (p = str; *p; p++)
      for (q = ccl; *q; q++)
         if (*p == *q)
            return(p);
   return(p); /* Didn't match */
   }
