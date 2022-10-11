#include "string.h"
extern struct sctrl _ScB;
/*
 * str = sbefore(s1, s2);
 * Searches in s1 for s2. Returns a new auto string which is the portion
 * of s1 before s2. If not found, returns a copy of all of s1.
 */
char *
sbefore(str1, str2)
    char *str1;
    char *str2;
{
    register int ix;
    extern char * substr();
    extern char * scatn();
    extern int sindex();

    if ((ix = sindex(str1, str2)) == -1)
	return(scatn(str1, NULL));
    return(substr(str1, ix));
}
