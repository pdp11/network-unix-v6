
/*	globvar.h	*/

int	host;			/* current host number */

int	init_host;		/* current host being reset during initialization
				*/
int     stimeo;                 /* number of sockets waiting for timeout */
int     ftimeo;                 /* number of files waiting for timeout */
int     timeo;                  /* flag indicating whether a timeout request
				   is currently pending */

struct	file	*fp;		/* current file pointer */

char	*kr_p;			/* pointer to current byte in kr_buf */

int	kr_bytes;		/* count of unprocessed bytes from 
				   last kernel read */

int	k_file;			/* file descriptor of kernel file */

char	host_dead[];		/* host dead status code */
char	host_alive[];		/* host alive status code */

int	k_wdebug;		/* kernel write debug toggle */

int	k_rdebug;		/* kernel read debug toggle */

struct		/* just to define these byte fields */
{
	char	lo_byte;
	char	hi_byte;
};


struct		/* just to define the word field */
{
	int	word;
};

struct		/* defines a char array field */
{
	char	bytes[];
};

struct		/* defines an int array field */
{
	int	words[];
};

#define TIMEINT 20      /* number of seconds to wait */
			/* if this value is too small, the daemon will
			   wake up needlessly; if it is too large, the
			   minimum timeout delay will be affected since
			   all delays are in multiples of this value */
