#include "mh.h"

char    current[];
struct msgs *mp;

m_setcur(num)

{
	char buf[6];
	register int i;
	register char *cp1;

	if(mp->msgflags&READONLY) {
		m_replace(cp1 = concat("cur-",mp->foldpath,0), m_name(num));
		free(cp1);
	}
	else {
		cp1 = copy(m_name(num), buf);
		*cp1++ = '\n';
		if(!equal(current, "cur"))
			error("\"current\" got Clobbered!! Tell B. Borden");
		if((i = creat(current, 0660)) >= 0) {
			write(i, buf, cp1-buf);
			close(i);
		}
	}
}
