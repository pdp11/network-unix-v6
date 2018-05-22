#define DC11ADDR	0174100
struct dcaddr {
	int dcrcsr;
	int dcrbuf;
	int dctscr;
	int dctbuf;
};

struct clist {
	int c_cc;
	int c_cf;
	int c_cl;
};

struct {
	struct clist t_inq;
	int t_state;
	int t_flags;
	int t_speeds;
	struct dcaddr *t_addr;
	int *outbuf;
	char *nxtc;
	char *lastc;
	int time_left;
} dc11h;
