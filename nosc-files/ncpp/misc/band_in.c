#
/* structure decs */

struct openparams		/* struct for making parameterized connections */
{
	char o_op;		/* opcode for kernel & daemon - unused here */
	char o_type;		/* type for connection see defines below */
	int  o_id;		/* id of file for kernel & daemon - unused here */
	int  o_lskt;		/* local socket number either abs or rel */
	int  o_fskt[2];		/* foreign skt either abs or rel */
	char o_frnhost;		/* for connection to specific host nums */
	char o_bsize;		/* bytesize of connection telnet demands 8 */
	int o_nomall;		/* initial allocation bits and msgs */
	int o_timeo;		/* num of secs to wait before timing out */
	int  o_relid;		/* fid of file to base a data connection on */
} openparams;

#define	opnparamsize	18

/* open type bits */
#define	o_direct	01	/* icp | direct */
#define o_server	02	/* user | server */
#define o_init		04	/* listen | init */
#define o_specific	010	/* general | specific ( for listen ) */
#define o_duplex	020	/* simplex | duplex */
#define o_relative	040	/* absolute | relative */
#define o_rcv		01	/* direct user listen general smplex absolute */
/**/

main (){

char	buf[1024];
int	fdi;
int	num_bytes;
double	total_bytes;		/* running total for this print file */
int	ivec[2];		/* time vector */
int	tvec[2];		/* termination time	*/


double	baud;
int	time();			/* time of day	*/
char	*ctime();		/* convert to ascii	*/
 

	openparams.o_type = o_rcv;
	openparams.o_nomall = 1024;
	openparams.o_lskt   = 10;
	total_bytes = 0;

	fdi = open ("/dev/net/anyhost",&openparams);

	if (fdi < 0) {
		printf ("*** unable to open net connection\n");
		exit();
	}


/**/
	time(ivec);
	while (1) {
		num_bytes = read(fdi,&buf,1024);
		if (num_bytes <= 0) break;
		total_bytes = total_bytes + num_bytes;
	}

	time(tvec);
	close (fdi);

	baud = (total_bytes * 8.0) / (tvec[1]-ivec[1]);
	printf (" band_in   %10.2f chars at %10.2f baud by %s",total_bytes,baud,ctime(tvec));
}
