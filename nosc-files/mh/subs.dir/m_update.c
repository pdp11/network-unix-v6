#include "mh.h"
#include "/rnd/borden/h/iobuf.h"
#include "/rnd/borden/h/signals.h"

char    defpath[];

m_update()
{
	struct iobuf out;
	register struct node *np;
	int save;

	if(def_flags & DEFMOD) {
		save = signal(SIGINT, 1);
		if(fcreat(defpath, &out) < 0) {
			printf("Can't create %s!!\n", defpath);
			flush();  exit(1);
		}
		for(np = m_defs; np; np = np->n_next) {
			puts(np->n_name, &out);
			puts(": ", &out);
			puts(np->n_field, &out);
			putc('\n', &out);
		}
		fflush(&out);
		signal(SIGINT, save);
		close(out.b_fildes);
		def_flags =& ~DEFMOD;
	}
}
