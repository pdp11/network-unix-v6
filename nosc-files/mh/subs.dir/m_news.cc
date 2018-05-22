#include "mh.h"

m_news()
{
	struct inode stbf;
	struct iobuf in;
	register flag, c;

	m_getdefs();
	if(stat(mhnews, &stbf) != -1 &&
	   stbf.i_mtime > deftime) {
		fopen(mhnews, &in);
		flag = getc(&in);
		while((c = getc(&in)) != -1)
			if(c == NEWSPAUSE) {
				if(!gans("More? ", anoyes))
					break;
			} else
				putchar(c);
		flush();
		if(flag == NEWSHALT)
			exit(0);
		close(in.b_fildes);
	}
}
