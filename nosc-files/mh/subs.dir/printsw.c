#include "mh.h"

printsw(substr, swp)
char *substr;
struct swit *swp;
{
	char buf[128];
	register char *cp, *cp1;
	register int i;
	int len;

	len = length(substr);
	for(; swp->sw; swp++)
		if(!*substr ||                  /* null matches all strings */
		   (ssequal(substr, swp->sw) && len >= swp->minchars))
			if(swp->minchars > 0) {
				cp = buf;
				*cp++ = '(';
				for(cp1 = swp->sw, i = 0; i < swp->minchars; i++)
					*cp++ = *cp1++;
				*cp++ = ')';
				while(*cp++ = *cp1++);
				printf("  -%s\n", buf);
			} else if(swp->minchars == 0)
				printf("  -%s\n", swp->sw);
}
