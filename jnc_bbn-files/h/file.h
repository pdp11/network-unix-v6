/*
 * One file structure is allocated
 * for each open/creat/pipe call.
 * Main use is to hold the read/write
 * pointer associated with each open
 * file.
 *
 * Flag fields expanded 3-14-79 jsq BBN.
 */
struct	file
{
	int     f_flag;
	char    f_flag2;
	char	f_count;	/* reference count */
	int	f_inode;	/* pointer to inode structure */
	char	*f_offset[2];	/* read/write character pointer */
} file[NFILE];

#ifdef NETWORK
struct netfile
{
	int  f_flag;    /* file type - should be FNET to use this */
	char f_flag2;
	char f_count;	/* processes with this open */
	int  f_netnode[3];	/* inode ptrs for read, write, and icp resp. */
};
#endif
/* flags */
#define	FREAD	01
#define	FWRITE	02
#define FEXCLU  04    /* rand:greep - file open for exclusive access */
#define FPIPE   010   /* rand:greep - number changed */
#define FPORT   020   /* rand addition by jsz 3/76 */
#ifdef NETWORK
#define FNET    040
#define FOPEN   0100
#define FERR    0200  /* there was an error in an net open */
#define FSHORT  00400    /* ncp open was old, short style jsq BBN 3-13-79 */
#define FRAW    01000    /* this is an RMI file jsq BBN 3-13-79 */

#define f_rdnode	0	/* index into f_netnode for read inode */
#define f_wrtnode 	1 	/*                          write      */
#define f_icpnode	2 	/*      		    icp        */

int open_err;           /* used in place of the f_rdnode field of the
			 * net file structure to pass open error code.
			 *                      8/31/78 S.Y. Chiu
			 */
#endif
