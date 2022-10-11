struct imp {            /* 1822 long leader */
	unchar i_nff;		/* new format flag */
	unchar i_dnet;		/* dest network (unused) */
	unchar i_lflgs;		/* leader flags */
	unchar i_type;		/* message type */
	unchar i_htype;		/* handling type */
	unchar i_host;		/* host number */
	union {			/* (to define the imp field) */
		n_short i_iw;
		struct {
			unchar i_i1;
			unchar i_i2;
		} i_ib;
#define i_imp	I_un.i_iw	/* imp field */
#define i_impno	I_un.i_ib.i_i2	/* imp number */
#define i_lh	I_un.i_ib.i_i1	/* logical host */
	} I_un;
	unchar i_link;		/* link number */
	unchar i_stype;		/* message subtype */
	n_short i_mlen;		/* message length */
};

				/* ifcb flag definitions */
#define IMPTRYING 3                     /* # of noops from imp to ignore */
#define IMPENDMSG 4			/* end of message flag */
#define IMPSETTING 8			/* i/f in process of initializing */

#define MTU 1019                        /* maximum imp message size (bytes) */
#define IMPWHYDOWN 3			/* host going down subtype mask */
