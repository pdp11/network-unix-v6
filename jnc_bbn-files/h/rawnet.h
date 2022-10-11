#ifdef RMI
#define RAWDEV  ((253<<8)|0)    /* (-3, 0) device of /dev/net/rawmsg */
#define NCPLOMSG  (72*16)       /* one of these is assigned to */
#define RAWLOMSG  0             /* LOMSG by rawopen */
int     LOMSG;  /* LOMSG and HIMSG define the range of msgs useable */
#define HIMSG ((255*16)+15)   /* for raw messages, i.e., not used by ncp */
#define ELSEMSG ((255*16)+15)   /* if open, gets anything no others do */
#define RAWTSIZE 16         /* number of raw msgs open at once */
#define ROPENFLAGS (n_open|n_raw) /* flags for rawskt */
#define MAXMSG  (1008 + impllen) /* 1006 data + leader + 2 length word */
#define RAWPAD  4       /* MAXMSG + RAWPAD may be read when MAXMSG written */

struct rawparams {
	int     z_mode; /* 0, 1, or 2 as in mode on standard file opens */
	long    z_host; /* zero means anyhost */
	int     z_lomsg;      /* low end of range */
	int     z_himsg;     /* high end: if zero, set equal to lomsg */
};

#define UNSIGN  int
struct rawskt { /* goes over all inode completely if MSG is defined otherwise
		 * i_addr[0] */
#ifdef MSG
	int unused[5];                  /* these are not used by the net
					 * software. Should be tampered
					 * with only with care since they
					 * include fields that are
					 * referenced as inode fields. */
	int INS_cnt;                    /* unused */
	int *itab_p;                    /* pointer to itab, used by await */
#endif
	int     v_conent;   /* unused */
	int     v_bsize;    /* unused */
	UNSIGN  v_flags;        /* always ROPENFLAGS (see also net.h) */
	int     v_msgs;     /* unused */
	int     v_bytes;        /* left to transfer in current message */
	int     v_msgq;         /* message que */
	UNSIGN  v_qtotal;       /* bytes in msgq */
	UNSIGN  v_hiwat;    /* unused */
	char    *v_proc;        /* process currently (last) using socket */
};

struct rawentry {
	long    y_host;     /* host number */
	int     y_lomsg;    /* low end of msgrange */
	int     y_himsg;    /* high end */
	int     y_rawskt;   /* read or write rawskt of msg */
} r_rawtab[RAWTSIZE], w_rawtab[RAWTSIZE];  /* read and write raw tables */

		/* points to rawtab entry of read msg (ELSEMSG) which gets */
struct rawentry *elseentry;     /* all messages not claimed by other msgs */

long ZERO0L;    /* fake a long constant zero (initialized in rawmain.c) */
#endif
