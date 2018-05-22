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
};

/*
 * the following defines may be plugged into the type field to indicate
 * what type of open is desired -- on means take the value on the right
 */
#define	o_direct	01	/* icp | direct */
#define o_server	02	/* user | server */
#define o_init		04	/* listen | init */
#define o_specific	010	/* general | specific ( for listen ) */
#define o_duplex	020	/* simplex | duplex */
#define o_relative	040	/* absolute | relative */

/* the following for (partial) compatibility with the original open.h */

struct {long LONG;};
#define o_lfskt	o_fskt[0].LONG

#define o_host	o_frnhost
#define o_bysze	o_bsize
#define o_nmal	o_nomall
#define o_timo	o_timeo

#define	DIRECT	 01	/* icp | direct */
#define SERVER	 02	/* user | server */
#define INIT	 04	/* listen | init */
#define SPECIFIC 010	/* general | specific (for listen) */
#define DUPLEX	 020	/* simplex | duplex */
#define RELATIVE 040	/* absolute | relative */
