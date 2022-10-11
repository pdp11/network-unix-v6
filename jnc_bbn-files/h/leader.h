#

/*	leader.h	*/

#ifndef SHORT
struct  ih
{
char    ih_nff;          /* new format flag */
char    ih_net;          /* destination network */
char    ih_flgs;         /* leader flags */
char    ih_type;         /* imp message type */
char    ih_htype;        /* handling type */
char    ih_hoi;          /* host on imp field */
char    ih_imp1;         /* imp field */
char    ih_imp0;         /* imp field */
char    ih_link;         /* imp link field */
char    ih_sbty;         /* imp subtype field */
int     ih_mlength;      /* message length */
};
#define ihllen 12
#endif

#ifdef SHORT
struct  ih
{
char	ih_type;		/* imp message type */
char	ih_host;		/* imp host field */
char	ih_link;		/* imp link field */
char	ih_sbty;		/* imp subtype field */
};
#define ihllen 4
#endif

struct hh {
char    hh_fil1;        /* must be zero */
char    hh_bysz;         /* host-host byte size */
int     hh_bycnt;       /* host_host byte count */
char    hh_fil2;       /* must be zero */
};
#define hhllen 5

struct leader {
	struct ih l_ih;
	struct hh l_hh;
	char data[];
};

#define lleader (ihllen + hhllen)
