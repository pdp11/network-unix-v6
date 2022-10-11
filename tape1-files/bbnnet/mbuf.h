#define NNETPAGES 256			/* maximum number of net pages */
#define PGSIZE 1024			/* size of a net page */
#define MSIZE 128                       /* size of an mbuf */
#define MHEAD (2+2*sizeof(struct mbuf *)) /* mbuf hdr length (not incl data) */
#define MLEN  (MSIZE-MHEAD)             /* mbuf max data length */
#define NMBPG (PGSIZE/MSIZE)            /* mbufs/page */

struct mbuf {                   /* message buffer */
	struct mbuf *m_next;		/* -> next msg buffer in chain */
	struct mbuf *m_act;		/* auxiliary mbuf pointer */
	unsigned char m_off;		/* offset in msg buf to curr. header */
	unsigned char m_len;		/* curr. data length in msg buf */
	unsigned char m_dat[MLEN];	/* data storage */
};

	/* vaddr to top of mbuf */
#define dtom(x)  ((struct mbuf *)((int)x & ~0x7f))      

struct pfree {                  /* free page table */
	unsigned char p_free;		/* # free bufs remaining in page */
	unsigned char p_off;		/* offset to next free buf in page */
	unsigned char p_next;		/* index of next free page entry */
	unsigned char p_prev;		/* index of prev free page entry */
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
struct host *h_find();
struct host *h_make();

#endif KERNEL
