#

/*	kread.h		*/

struct	kr	/* just to define kr_op field */
{
char	kr_op;			/* op field for kernel reads */
};

struct	kr_open			/* open instruction structure */
{
char	kr_op;			/* op field */
char	kro_type;		/* type of open */
int	kro_id;			/* kernel's id for file */
char	*kro_lskt;		/* local socket */
char	*kro_fskt[2];		/* foreign socket */
char	kro_host;		/* host */
char	kro_bysz;		/* byte size */
int	kro_nmal;		/* nominal (initial) allocation */
int	kro_timo;		/* timeout */
int	kro_relid;		/* id of relative file for relative open */
};

/* open type bits--should be taken from netopen.h */
#define	otb_drct	01	/* icp | direct */
#define	otb_serv	02	/* user | server */
#define	otb_init	04	/* listen | init */
#define	otb_spcf	010	/* general | specific (for listen) */
#define	otb_dux		020	/* simplex | duplex */
#define	otb_rltv	040	/* absolute | relative (socket #'s) */

/*	open error codes--should be taken from user.h	*/
#define EDDWN		48	/* ncp daemon was killed */
#define EDAEIO		49	/* ncpdaemon I/O error */
#define EDINV		50	/* invalid argument to daemon */
#define EDNORES		51	/* no resource in daemon */

struct	kr_rcv			/* rcv instruction structure */
{
char	kr_op;			/* op field */
char	krr_type;		/* type field from imp leader */
char	krr_host;		/* source field from imp leader */
char	krr_link;		/* link filed from imp leader */
char	krr_sbty;		/* subtype field from imp leader */
char	krr_data;		/* data from message, if any, starts here */
char	krr_dts[1008];		/* space for data */
}
kr_buf;				/* this is the kernel read buffer */

#define	krr_ovhd	5	/* length of leader and op fields in rcv */

struct	kr_close		/* close instruction structure */
{
char	kr_op;			/* op field */
char	krc_sinx;		/* socket index in file */
int	krc_id;			/* kernel's file id */
};

struct	krr_reset
{
char	kr_op;			/* op field */
char	krst_host;		/* host to reset */
};
/*	kernel read instruction opcodes	*/
#define	kri_open	0
#define	kri_rcv		1
#define	kri_close	2
#define	kri_timo	3
#define kri_reset	4

