struct proto {
	struct proto *pr_next;		/* next proto entry on chain */
	short pr_num;			/* proto number */
	short pr_flag;			/* flags */
};

#define	PRUSED	1			/* entry in use */

#define NPRMB	(MLEN/sizeof(struct proto))	/* # proto entries per mbuf */
#define NPROTO 67			/* should be prime */

#define PRHASH(x) ((short)x % NPROTO)	/* proto hdr table hash function */

#ifdef KERNEL
struct proto ***protab;			/* ->start of proto hdr table */
int nproto;	

struct proto *profind();

#endif KERNEL
