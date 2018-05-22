#
#define DEBUG TRUE

#define TRUE 1
#define FALSE 0
#define EVER (;;)

#define T_OK 2
#define DONE 1
#define OK 0
#define NOTOK -1

#define NODEL -2
#define TRYAGN -3
#define TIMEOUT -4
#define BADNET -5
#define HOSTDEAD -6
#define HOSTERR -7
#define TRYMAIL -8
#define SKIP -9
#define MINSTAT -9
#define MAXSTAT 2

#define NAMELEN 100
#define LINESIZE 256
#define ERRLINSIZ 300

#define DELT_OK '\200'
#define DELMAIL '/'
#define DELTTY '\\'
#define DELBOTH '+'
#define DELTorM '_'
#define DELNONE '|'

#define QUOTE '\\'
#define LCMNTDLIM '('
#define RCMNTDLIM ')'

#define RETCITE 1

#define CLOSEFD -1
#define PUREXEC 0
#define FRKEXEC 1
#define SPNEXEC 2
#define FRKWAIT 1
#define FRKPIGO 2
#define FRKCIGO 4
#define HIGHFD 16
#define NUMTRY 30

#define SIGCLK 14
#define SIGINR 15









#define RPLOK 200
#define RPLGOT 250
#define RPLXFER 426
#define RPLLOC 451
#define RPLSYN 500
#define RPLPARM 501
#define RPLCMD 502
#define RPLUSR 553

struct adr
{ char adr_dest;
char adr_hand;
char adr_delv;
char adr_name[NAMELEN+1];
};

#define ANYDLVR 1
#define BAKGRND 0
#define LOCDLVR ' '
#define POBOX 'w'
/*#define ARPANET 'a' */

struct netstruct
{ char net_code;
char net_access;
char *net_name;
char *net_ref;
char *net_spec;
char *net_tpath;
int net_tabfd;
};


struct adrstruct
{
struct adrstruct *nxtadr;
int shnum;
char dlv_code;
char *shostr;
char *smbox;
};

struct iobuf
{
int fildes;
int nleft;
char *nextp;
char buff[512];
};

struct inode
{
char minor;
char major;
int inumber;
int iflags;
char nlinks;
char uid;
char gid;
char fsize0;
int fsize1;
int iaddr[8];
long int acctime;
long int modtime;

};

struct

{
char minor;
char major;
int inumber;
int iflags;
char nlinks;
char uid;
long int fsize;
};

struct pipstruct
{ int prd;
int pwrt;
};
