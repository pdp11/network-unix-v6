/* -------------------------- S S K I P ----------------------------- */
/*
 * sskip(string, cclass) returns a pointer to the first character of
 * string which is not in cclass, or to the null at the end of string
 * if all of them are.
 */
char *
sskip(str, ccl)
   char *str;
   char *ccl;
   {
   register char *p, *q;

   for (p = str; *p; p++)
      for (q = ccl; *p != *q; q++)
         if (*q == 0)  /* Ran out of cclass chars */
            return(p);
   return(p); /* All matched */
   }
