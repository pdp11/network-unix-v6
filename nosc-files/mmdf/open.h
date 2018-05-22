#

 /* 
  *         MS PERSONAL MESSAGE SYSTEM
  *
  *
  *      Written for PDP-11s running Unix.
  *      Interfaces to Arpanet Mail.
  *
  *
  *               3 February 1978
  *
  *
  *      Information Sciences Department
  *      The Rand Corporation
  *      1700 Main Street
  *      Santa Monica, California  90406
  *
  *        phone:      (213) 393-0411
  *        Arpanet:    ms at Rand-Unix
  *
  */



/*                                                                      */
/*      Network Control Program Definition File                         */
/*                                                                      */


struct netopen			       /* network open parameters      */
{
    char    o_op;		       /* operation (used by NCP)      */
    char    o_type;		       /* type of open                 */
    int     o_id;		       /* file id                      */
    int     o_lskt;		       /* local socket                 */
    int     o_fskt[2];		       /* foreign socket               */
    char    o_host;		       /* host                         */
    char    o_bysize;		       /* byte size                    */
    int     o_nmal;		       /* nominal allocation           */
    int     o_timo;		       /* timeout                      */
    int     o_relid;		       /* relative id                  */
}
                openparam;

 /* open type bit definitions    */
#define DIRECT   01		       /* icp | direct                 */
#define SERVER   02		       /* user | server                */
#define INIT     04		       /* listen | init                */
#define SPECIFIC 010		       /* general | specific (for      */
				       /*   listen)                    */
#define DUPLEX   020		       /* simplex | duplex             */
#define RELATIVE 040		       /* absolute | relative          */
