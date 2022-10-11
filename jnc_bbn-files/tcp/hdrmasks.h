struct BSLMask
{  int BPrecSec;           /* precedence, security fields */
   int BTccCcc;            /* trans control code, command control code */
   int BSecTcc;            /* security, TCC fields */
   int BFnTcp;             /* foreign TCP ID */
   int BVerSegNo;          /* version, s-segment number */
   int BBSLSave[3];        /* unused fields */
 };

struct INLMask
{  char ITypeService;      /* type of service */
   char IVerHdr;           /* 4 bit version, 4 bit header length */
   int ISegLength;         /* length of segment in octets */
   int ISegID;             /* segment identification */
   int IFragOffset;        /* 3 bits flags, 13 bits fragment offset */
   char IProtocol;         /* next layer protocol identifier */
   char ITimeToLive;       /* time to live... */
   int IINChkSum;          /* internet header checksum */
   char ISHstH;            /* source host - high byte */
   char ISrcNet;           /* source network */
   int ISHstL;             /* source host - lower two bytes */
   char IDHstH;            /* destination host - high byte */
   char IDstNet;           /* destination network */
   int IDHstL;             /* destination host - lower two bytes */
   char IOptions[4];       /* option field */
 };

struct TCPMask
{  int TSrcePort;        /* source port */
   int TDestPort;          /* destination port */
   long TSeqNo;            /* sequence number */
   long TAckNo;            /* acknowledge number */
   int TTCPFlags;          /* 4 bits data offset, rest are flags */
   int TWindow;            /* flow control window */
   int TTCPChkSum;         /* checksum of TCP & parts of IN header */
   int TUrgentPtr;         /* urgent pointer */
 };

struct ANMask1     /* Arpanet two word leader format */
{  int MLngth1;
   char Type1;
   char Host1;
   char Link1;
   char SbType1;
 };

struct ANMask2     /* Arpanet six word leader format */
{  int MLngth2;
   char Fmt2;
   char SNet2;
   char LdrFlgs2;
   char Type2;
   char HandType2;
   char Host2;
   char LogHost2;
   char Imp2;
   char Link2;
   char SbType2;
   int ALngth2;
 };
