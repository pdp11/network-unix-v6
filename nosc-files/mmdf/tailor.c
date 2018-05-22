#include "mailer.h"
/*                                                                      */
/*      This file contains all of the host dependent information        */
/*   that the message system needs to have.  The data which is          */
/*   considered to be the most host dependent is flagged in the         */
/*   left margin.                                                       */
/*                                                                    */

int

 /* */  lochostnum 2,               /* local host number            */
	sentprotect 0600,	       /* protection on files in mail  */
				       /*   directories                */
	termplen 24,                   /* default screen size          */
				       /*   zero => no pagination      */
	timout1  72,                   /* notify of slow delivery       */
	timout2  144,                   /* return undelivered mail       */
	mailsleep 600;                 /* for background mailer         */

char

	dbgmode { TRUE }, /*DBG*/
 /* */  locname[] "SRI-TSC",         /* local host name              */
	lhostr[] "2",
/* */	stndmail[] ".mail",	/* file to receive new mail */
 /* */  namsmail[] "nmail",             /* name of simple mail process  */
 /* */  /*prgsmail[] "/u/dcrocker/misc.d/nmail",*/
 /* */  prgsmail[] "/m/mmdf/nmail",
	delim1[] "\001\001\001\001\n", /* line to add to begin of msgs */
	delim2[] "\001\001\001\001\n", /* line to add to end of msgs   */
				       /* location of file w/mailer    */
 /* */  prgsubmit[] "/m/mmdf/submit",
 /* */  namsubmit[] "submit",          /* name to show for mailer      */
 /* */  prgmailer[] "/m/mmdf/deliver",
 /* */  nammailer[] "deliver",         /* name to show for mailer      */
	userloc[] "rcvmail",
	userpo[] "poboxrcv",           /* pgm in user's bin to receive */
	userrem[] "remrcv",           /* for own remote processing      */
	eosstr[] "!\n",                  /* indicates end of a stream     */
	ttympre[] "\n\r\007***** NOTICE *****\n\r",
	ttymsuf[] "\n\r***** ****** *****\n\r",
	dlvnote[]
{
    0
}      ,			       /* text of delivery notice to   */
				       /*   be written to local users' */
				       /*   terminal (if ok).          */
	homeque[] "/m/homeque",
	logfile[] "/m/log/mailer.log",
	remslalog[] "/m/log/remsla.log",
	remmaslog[] "/m/log/remmas.log",
	script[] "/m/mmdf/script",
	remsndport[] "/dev/ttyn",
/*	dialdev[] "/dev/acu13",*/
	dialdev[] "/dev/ttyd",
	dlslalog[] "/m/log/dlsla.log",
	dlslatrn[] "/m/log/dlsla.trn",
	dlmaslog[] "/m/log/dlmas.log",
	dlmastrn[] "/m/log/dlmas.trn",
 /* */  aquedir[] "aquedir/",
 /* */  mquedir[] "mquedir/",
				       /* name of directory where      */
				       /*   queued mail is put         */
 /* */  tquedir[] "tquedir/",
				       /* name of temporary directory  */
				       /*   where queued mail is put   */
 /* */  snoopbox[] {0},                /* address of where to send     */
				       /*   externally transmitted     */
				       /*   mail for correspondence    */
				       /* if no need to snoop set to 0 */
	snoopfile[] {0},
 /* */  supportaddr[] "DEAFNET at SRI-TSC",
				       /* mail address of support group */
 /* */  timzone 'P',                   /* first letter of local time   */
				       /*   zone                       */
	 whofile[] "/etc/utmp";        /* who is using the system now  */

struct netstruct
/*      net_pgm, net_access, net_path, net_ref, net_spec, net_path, net_fd */
#ifdef LOCDLVR
    net_loc
	{LOCDLVR, ANYDLVR, "/m/mmdf/locsnd", "Local", "Local",
	    "/m/names/aliases", 0},
#endif
#ifdef ARPANET
    net_arpa
	{ARPANET, ANYDLVR,  "/m/mmdf/netsnd", "ArpaNet", "ArpaNet",
 	  "/usr/net/hnames", 0},	/* name of host names file	*/
#endif
#ifdef  POBOX
    net_pobox
	{POBOX,  ANYDLVR,   "/m/mmdf/poboxsnd", "P.O. Box", "POBox",
	    /* "/mnt/dcrocker/poboxnames", 0}, */
	   "/m/names/ecnames", 0},
				/* user needs special pgm       */
#endif
#ifdef  DIALSND
    net_dialsnd
	{DIALSND,  ANYDLVR,   "/m/mmdf/remsnd", "P.O. Box", "POBox",
	    /* "/mnt/dcrocker/poboxnames", 0}, */
	   "/m/names/wcnames", 0},
#endif

    *nets[] {            /* need this array for searches         */
#ifdef LOCDLVR
	&net_loc,
#endif
#ifdef  ARPANET
	&net_arpa,
#endif
#ifdef  POBOX
	&net_pobox,
#endif
#ifdef  DIALSND
	&net_dialsnd,
#endif
	0};

char   *respon[]
{
    "SKIP", "TRYMAIL", "HOSTERR", "HOSTDEAD", "BADNET", "TIMEOUT",
    "TRYAGN", "NODEL", "NOTOK", "OK", "DONE", "T_OK"
};                                     /* For log */
