struct imp {            /* 1822 long leader */
	u_char i_nff;		/* new format flag */
	u_char i_dnet;		/* dest network (unused) */
	u_char i_lflgs;		/* leader flags */
	u_char i_type;		/* message type */
	u_char i_htype;		/* handling type */
	u_char i_host;		/* host number */
	union {			/* (to define the imp field) */
		n_short i_iw;
		struct {
			u_char i_i1;
			u_char i_i2;
		} i_ib;
#define i_imp	I_un.i_iw	/* imp field */
#define i_impno	I_un.i_ib.i_i2	/* imp number */
#define i_lh	I_un.i_ib.i_i1	/* logical host */
	} I_un;
	u_char i_link;		/* link number */
	u_char i_stype;		/* message subtype */
	n_short i_mlen;		/* message length */
};

				/* ifcb flag definitions */
#define IMPTRYING 3                     /* # of noops from imp to ignore */
#define IMPENDMSG 4			/* end of message flag */
#define IMPSETTING 8			/* i/f in process of initializing */

#define IMPIPROTO 155		/* IP link number for 1822 */

#define IMPMTU 1019			/* maximum imp message size (bytes) */

/* zero lh byte for RFNM counting */
#ifndef mbb
#define addr_1822(x) (((x).s_neta == 10 || (x).s_neta == 3) ? \
					((x).s_addr & 0xff00ffff) : (x).s_addr)	
#else 
#define addr_1822(x) (((x).s_neta == 10 || (x).s_neta == 3) ? \
					((x).s_addr & 0xfffff003ff) : (x).s_addr)	
#endif

#define IMP_NFF		15	/* new format flag
				/* 1822 message types */
#define IMP_DATA	0		/* regular message */
#define		IMP_DATA_STD		0	/* standard */
#define		IMP_DATA_UNCTL		3	/* uncontrolled */
#define IMP_LERROR	1		/* leader error */
#define 	IMP_LERROR_HDR		0	/* error in leader */
#define		IMP_LERROR_SHORT	1	/* short leader */
#define		IMP_LERROR_TYPE		2	/* illegal type */
#define		IMP_LERROR_STYLE	3	/* wrong style */
#define IMP_DOWN	2		/* imp going down */
#define IMP_NOP		4		/* no-op */
#define IMP_RFNM	5		/* ready for next message */
#define IMP_HSTAT	6		/* dead host status */
#define IMP_DEAD	7		/* destination dead */
#define 	IMP_DEAD_IMP		0	/* imp unreachable */
#define		IMP_DEAD_HOST		1	/* host dead */
#define		IMP_DEAD_LDR		2 	/* host can't handle new ldr */
#define		IMP_DEAD_ADM		3	/* administrative */
#define IMP_DERROR	8		/* data error */
#define IMP_INCOMP	9		/* incomplete transmission */
#define 	IMP_INCOMP_SLO		0	/* host tardy */
#define		IMP_INCOMP_LEN		1	/* msg too long */
#define 	IMP_INCOMP_TIMO		2	/* timeout */
#define		IMP_INCOMP_LOST		3	/* lost in network */
#define		IMP_INCOMP_BLOK		4	/* IMP blocking */
#define		IMP_INCOMP_IO		5	/* IMP i/o error */
#define	IMP_RESET	10		/* interface reset */
