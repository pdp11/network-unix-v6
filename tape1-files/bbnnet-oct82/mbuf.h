#define NNETPAGES 256			/* maximum number of net pages */
#define PGSIZE 1024			/* size of a net page */
#define MSIZE 128                       /* size of an mbuf */
#define MHEAD (2+2*sizeof(struct mbuf *)) /* mbuf hdr length (not incl data) */
#define MLEN  (MSIZE-MHEAD)             /* mbuf max data length */
#define NMBPG (PGSIZE/MSIZE)            /* mbufs/page */

struct mbuf {                   /* message buffer */
	struct mbuf *m_next;		/* -> next msg buffer in chain */
	struct mbuf *m_act;		/* auxiliary mbuf pointer */
	u_char m_off;			/* offset in msg buf to curr. header */
	u_char m_len;			/* curr. data length in msg buf */
	u_char m_dat[MLEN];		/* data storage */
};

/* addr to head of mbuf */
#define dtom(x)  ((struct mbuf *)((int)x & ~(MSIZE-1)))      
/* mbuf head to typed data */
#define mtod(x,t) ((t)((int)(x) + (x)->m_off))

struct pfree {                  /* free page table */
	u_char p_free;		/* # free bufs remaining in page */
	u_char p_off;		/* offset to next free buf in page */
	u_char p_next;		/* index of next free page entry */
	u_char p_prev;		/* index of prev free page entry */
};

	/* pfree index to ->page data */
#define pftom(x) ((struct mbuf *)((x << 10) + (int)netutl))  
	/* mbuf to pfree index */
#define mtopf(x) ((((int)x & ~0x3ff) - (int)netutl) >> 10)   
	/* mbuf to page offset */
#define mtoff(x) (((int)x & 0x3ff) >> 7)                

#ifdef KERNEL

struct pfree *freetab;                  /* ->free page table */
int nnetpages;
extern struct mbuf netutl[];            /* virtual address of net free mem */
extern struct pte Netmap[];             /* page tables to map Netutl */

struct mbuf *m_get();
struct mbuf *m_free();
int m_freem();
struct mbuf *m_adj();
struct mbuf *m_copy();
struct host *h_find();
struct host *h_make();

#endif KERNEL
