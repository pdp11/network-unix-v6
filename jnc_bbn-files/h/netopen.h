#ifdef NCP                      /* jsq bbn 10/19/78 */
#define SHORTDEV(-1)            /* major device numbers for old and new */
#define LONGDEV (-2)            /* user interfaces */
struct openparams		/* struct for making parameterized connections */
{
	char o_op;		/* opcode for kernel & daemon - unused here */
	char o_type;		/* type for connection see defines below */
	int  o_id;		/* id of file for kernel & daemon - unused here */
	int  o_lskt;		/* local socket number either abs or rel */
	long	o_fskt;		/* foreign skt either abs or rel */
	char o_frnhost;		/* for connection to specific host nums */
	char o_bsize;		/* bytesize of connection telnet demands 8 */
	int o_nomall;		/* initial allocation bits and msgs */
	int o_timeo;		/* num of secs to wait before timing out */
	int  o_relid;		/* fid of file to base a data connection on */
};

struct nopenparams              /* struct for making parameterized connections */
{
	char o_op;		/* opcode for kernel & daemon - unused here */
	char o_type;		/* type for connection see defines below */
	int  o_id;		/* id of file for kernel & daemon - unused here */
	int  o_lskt;		/* local socket number either abs or rel */
	long	o_fskt;		/* foreign skt either abs or rel */
	char o_frnhost;		/* for connection to specific host nums */
	char o_bsize;		/* bytesize of connection telnet demands 8 */
	int o_nomall;		/* initial allocation bits and msgs */
	int o_timeo;		/* num of secs to wait before timing out */
	int  o_relid;		/* fid of file to base a data connection on */
	long o_host;            /* 32 bit new format host number */
};

/*
 * the following defines may be plugged into the type field to indicate
 * what type of open is desired
 */

#define o_direct	001	/* icp |~ direct */
#define o_server	002	/* user | server */
#define o_init		004	/* listen | init */
#define o_specific	010	/* general | specific */
#define o_duplex	020	/* simplex | duplex */
#define o_relative	040	/* absolute | relative */

/*
 * The following are the channels openned by ear.c
 */
#define netchan 3
#define foochan 4		/* for the net close kludge */

#endif                          /* on NCP */
