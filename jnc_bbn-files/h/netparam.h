#
/*
 * netparam.h - used by ncpkernel, ken/awaitr.c, ken/table.c,
 * ncpdaemon, libn packages, ncpprogs
 * see also rawnet.h for rmi specific definitions
 * this file should be in the h subdirectory of the kernel sources,
 * copied to the current daemon source directory, and
 * copied to /usr/sys/h for the libn and progs sources.
 * jsq bbn 1-22-79
 */

	/* definitions for conditional compilation of pieces */

/* #define MSG     0    */           /* mods for NSW MSG */
/* #define SHORT 0      */    /* use long leaders */

	/* kernel buffering considerations */

#define kb_hiwat  4             /* max system bufs we can get for NCP
				 * Should not be greater than
				 * NSBUF (defined in param.h)
				 */
#define init_b_hyster 4 /* initial kernel bufs taken by ncp or rmi daemon */
#define rbuf_max 80             /* max netbufs usable for raw messages
				  (8 netbufs per system buffer) */


	/* definitions related to imp-host leaders */

#ifndef SHORT
#define NFF 15                  /* new format flag */
#define ihllen 12              /* length of long imp-host leader */
#endif
#ifdef SHORT
#define ihllen 4               /* length of short imp-host leader */
#endif


	/* flow control */

#define	NOMMSG	10		/* JSK Nominal message allocation */


	/* host number definitions */

	    /* definitions for new long host format */

#define LOCALNET  3     /* default (until localnet is reset by an sgnet) */
				/* local net address for long host numbers */

/* for taking a long host apart into appropriate pieces */
struct { char h_hoi;     char h_net;     char h_imp0;    char h_imp1; };
struct { char h_hoi;     char h_net;     char h_imp0;    char h_logh; };
struct { char h_hoi;     char h_net;     int h_imp;                   };

	    /* conversion of host forms */

long stolhost();        /* so we can call it correctly.  Note stolhost */
			/* takes a char in the kernel and a long elsewhere */

	    /* hashing definitions */

		/* these are used mostly in the kernel */

#ifndef SHORT
#define LIMPS 85        /* at least # of highest imp in use + 1 */
#define LHOI 16         /* maximum # of host on imp kernel can handle */
#endif
#ifdef SHORT
#define LIMPS 64        /* defaults */
#define LHOI 4
#endif

		/* these are used for the daemon hashing scheme */

				/* defines for the daemon hashing scheme */
#define host_len 257            /* must be prime (see util.c/host_hash) */
	/* also needs to be greater than number of active hosts */
#define host_ken (host_len + 2) /* kludge to deal with "host zero" */
 

	/* host and link structure (mostly used by daemon) */

struct	hostlink		/* host and link structure */
{
	long    hl_host;
	int     hl_link;
};


	/* general information */

struct netparam {
	long net_lhost;
	int net_flags;
	int net_open;
} netparam;

#define ncpopnstate netparam.net_open   /* 0 - no daemon running */
					/* 1 - ncpdaemon */
					/* -1 - rawdaemon */

	/* flags for netparam.net_flags */
#define net_network     001
#define net_short       002
#define net_ncp         004
#define net_raw         010
#define net_gate        020
#define net_msg         040
