
/*	impi.h	*/

/* imp-host protocol command codes */

/* host-to-imp codes */
#define	iwi_reglr	0
#define	iwi_ernid	1	/* err w/o msg id */
#define	iwi_hstgd	2	/* host going down */
#define iwi_nop		4
#define	iwi_erwid	8	/* err w/ id */

/* imp-to-host codes */
#define	iri_reglr	0
#define	iri_erldr	1	/* err in leader */
#define	iri_igd		2	/* imp going down */
#define	iri_nop		4
#define	iri_rfnm	5
#define	iri_hdeds	6	/* dead host status */
#define	iri_dhid	7	/* dest host or imp dead */
#define	iri_erdat	8	/* err in data */
#define	iri_inclx	9	/* incomplete xmission */
#define	iri_reset	10

