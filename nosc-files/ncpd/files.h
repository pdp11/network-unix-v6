#
 
/*	files.h	*/

#define	nfiles	16		/* number of file structures */

struct	file			/* ncp file structure */
{
int	f_id;			/* kernel's id for it */
int	f_sbase;		/* base of socket group for file */
char	f_state;		/* current file state */
char	f_skt_x[3];		/* indices of sockets currently attached to
				  this file */
char	f_nsatt;		/* number of attached sockets */
char	f_nsrsrv;		/* number of reserved sockets */
long    f_timeo;                /* for saving con opn times */
#ifdef SFHBYTE
int	f_bysz;			/* for saving connection bytesize */
#endif
}
files[nfiles];			/* reserve space for nfiles of them */

int	n_f_left;		/* counter for allocating files */

/*	file states	*/
#define	fs_null		0
#define	fs_uiopw	1	/* user icp open wait */
#define	fs_uisnw	2	/* user icp socket number wait */
#define	fs_dopnw	3	/* data open wait */
#define fs_open		4
#define	fs_gonew	5	/* gone wait */
#define	fs_siopw	6	/* server icp open wait */
#define	fs_sialw	7	/* server icp ALL wait */
#define	fs_sirfw	8	/* server icp rfnm wait */

/* nominal allocation values */
#define	nom_mall	10	/* messages */
#define	nom_ball	512	/* bits */

/* null socket index */
#define	fsx_null	0177777	/* will use this value to indicate that
				   no socket for this type index exists */

/* null kernel file id */
#define	fid_null	0	/* will use to indicate that kernel is no longer
				   interested in file */

/* number of seconds allowed to complete a server connection */
#define FTIMEOUT        60      /* if a connection is complete by now forget
					it.
				*/
