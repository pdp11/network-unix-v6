/* -------------------------- S L E N G T H ------------------------- */
/*
 * curlen = slength(str)
 * Returns the current length of str.
 */
int
slength(s)
   char *s;
   {
   register char *p;

   p = s;
   while(*p++);
   return(p - s - 1);
   }
