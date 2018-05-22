
/*	hhi.h	*/

/* host-host protocol command codes */
#define	hhi_nop	0
#define	hhi_rts	1
#define	hhi_str	2
#define	hhi_cls	3
#define	hhi_all	4
#define	hhi_gvb	5
#define	hhi_ret	6
#define	hhi_inr	7
#define	hhi_ins	8
#define	hhi_eco	9
#define	hhi_erp	10
#define	hhi_err	11
#define	hhi_rst	12
#define	hhi_rrp	13

/* error code values for err command */
#define	hhe_ilop	1	/* illegal opcode */
#define	hhe_sps		2	/* short parameter space */
#define hhe_bpar	3	/* bad parameters */
#define	hhe_nxs		4	/* request on non-existent socket */
#define	hhe_snc		5	/* socket not connected */

/* host write assembly buffer */
char	hw_buf[12];

