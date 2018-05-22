int	impi_sockt;		/* pts to sktp involved with imp read */
struct netbuf *impi_msg;	/* holds msg being built by imp read */
int	impi_mlen;		/* len of msg being built */
int	impistart;		/* phys addr of imp struct */
int	impiend;			/* phys addr of end of imp struct */
int	impostart;		/* phys addr of start of output leader */
int	impoend	;		/* phys addr of end of output hh leader */
char impflushing;	/* if on indicates imp is being flushed */
char impleader;		/* signals leader read */



struct			/* struct for holding statistics on imp */
{
	char *i_flushes;	/* number of times imp flushed */
	char *i_leaderr;	/* number of imp leader errs */
} imp_stat;

struct			/* struct read into on leader reads */
{
	char ndum;	/* dummy byte to keep on word bound */
	char nrcv;	/* permanently contains an ncp daemon rcv command */
	char type;	/* imp msg type */
	char host;	/* indicates from what host */
	char link;	/* indicates on what link */
	char subtype;	/* subtype field */
	char pad1;	/* pad char for host-host leader - must be zero */
	char bsize;	/* byte size of data coming in */
	int	bcnt;	/* num of bsize bytes coming in */
} imp;

/* possible values for imp type field */
#define ih_stdmsg	00	/* standard data message */
#define ih_elead	01	/* err in leader */
#define ih_igodwn	02	/* imp going down */
#define ih_sideband	03	/* new side band data path */
#define ih_nop		04	/* nop */
#define ih_rfnm		05	/* request for next message */
#define ih_drea		06	/* reason host is dead */
#define ih_hstd		07	/* host or imp is dead */
#define ih_edata	8	/* err in data */
#define ih_ictrans	9	/* incomplete transmission */
#define ih_impup	10	/* imp is up */
#define NUMHHOPS	13	/* largest legal host to host op code */



struct				/* struct for writing leaders to imp */
{
	char o_type;		/* msg type */
	char o_host;		/* for what host */
	char o_link;		/* over what link */
	char o_subtype;		/* with what subtype */
	char o_pad1;		/* first pad character for hh leader */
	char o_bsize;		/* byte size of data to follow */
	int	o_bcnt;		/* number of bytes in above byte size */
	char o_pad2;		/* second pad char for hh leader */
}	oimp;

#ifndef UCBUFMOD
struct devtab	impotab;	/* imp output device */
struct buf	impobuf;	/* imp output buffer */
#endif UCBUFMOD
#ifdef UCBUFMOD
struct netbuf *impoq;		/* imp output queue */
#endif UCBUFMOD

struct salloc		/* structure to lay over an allocate command */
{
	char a_op;
	char a_link;
	int	a_msgs;
	int	a_bitshi;
	int	a_bitslo;
};
