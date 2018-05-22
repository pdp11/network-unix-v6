/*
 * One file structure is allocated
 * for each open/creat/pipe call.
 * Main use is to hold the read/write
 * pointer associated with each open
 * file.
 */
struct	file
{
	char	f_flag;		/* see below for flag bit assignments */
	char	f_count;	/* reference count */
	int	f_inode;	/* pointer to inode structure */
	char	*f_offset[2];	/* read/write character pointer */
};
#ifdef NFILE
struct file file[NFILE];
#endif NFILE

struct netfile
{
	char f_flag;		/* file type- should be FNET for this struct */
	char f_count;		/* processes with this open */
	int  f_netnode[3];	/* inode ptrs for read, write, and icp resp. */
};
/* flags */
#define	FREAD	01
#define	FWRITE	02
#define	FPIPE	04
#define FNET	010
#define FTIMEO	020
#define FOPEN	040
#define FERR	0100		/* there was an error in an net open */

#define f_rdnode	0	/* index into f_netnode for read inode */
#define f_wrtnode 	1 	/*                          write      */
#define f_icpnode	2 	/*      		    icp        */
