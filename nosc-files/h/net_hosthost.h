
/*	defines for dealing with host to host protocol		*/

#define hhnop	0	/* nop */
#define hhrts	1	/* receiver to sender */
#define hhstr	2	/* sender to receiver */
#define hhcls	3	/* close */
#define hhall	4	/* allocate */
#define hhgvb	5	/* give back */
#define hhret	6	/* return */
#define hhinr	7	/* interrupt by receiver */
#define hhins	8	/* interrupt by sender */
#define hheco	9	/* echo request */
#define hherp	10	/* echo reply */
#define hherr	11	/* error */
#define hhrst	12	/* reset */
#define hhrrp	13	/* reset reply */

/* this array is indexed by the above op codes to give the length of the command */

char hhsize[14];
/*	1,		/* nop */
/*	10,		/* rts */
/*	10,		/* str */
/*	9,		/* cls */
/*	8,		/* all */
/*	4,		/* gvb */
/*	8,		/* ret */
/*	2,		/* inr */
/*	2,		/* ins */
/*	2,		/* eco */
/*	2,		/* erp */
/*	12,		/* err */
/*	1,		/* rst */
/*	1		/* rrp */

/* bit map for waiting on rfnms over the host control link */
char host_map[256/8];
