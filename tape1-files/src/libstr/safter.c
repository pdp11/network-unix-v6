/* ------------------------ S A F T E R ----------------------------- */
/*
 * str = safter(s1, s2);
 * Searches in s1 for s2. If found, returns the string after s2.
 * This is not a copy but the actual rest of s1. If not found,
 * returns a zero-length string.
 */
char *
safter(str1, str2)
   char *str1;
   char *str2;
   {
   register int ix;

   if ((ix = sindex(str1, str2)) == -1)
      return("");
   ix =+ slength(str2);
   return(&str1[ix]);
   }
