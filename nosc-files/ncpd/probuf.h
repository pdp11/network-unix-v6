#
 
/*	probuf.h	*/


struct	probuf			/* protocol output buffer structure */
{
	struct	probuf	*pb_link;	/* output q link */
	char	pb_count;		/* number of bytes in buffer */
	char	pb_text[12];		/* the bytes themselves */
};

#define	pb_size	16		/* size of probuf in bytes */

struct	probuf	*pb_fr_q;	/* probuf free list head */

struct	probuf	*h_pb_q[256];		/* the output q heads for the hosts */

char	h_pb_sent[256];			/* count of buffers in the current
					  outstanding message (if any) to
					  each host (0 if none outstanding) */

char	h_pb_rtry[256];			/* retry count on the current
					  outstanding message to each host
					  (0 if none) */

char	h_up_bm[256/8];		/* bit map of live hosts */

char	rfnm_bm[256/8];		/* bit map for rfnms outstanding */

int	n_pb;			/* counter for total number of existing
				   probufs. used in dumps as measurement
				   tool */

int	pro2send;		/* !=0 if there is protocol queued to send
				  generated on this trip thru the main loop */

int	x_retries;		/* number of retries on protocol xmission */

