/*	param.c	4.2	81/04/02	*/

#include "../h/param.h"
#include "../h/systm.h"
#ifdef BBNNET
#define KERNEL
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/imp.h"
#include "../bbnnet/ucb.h"
#undef KERNEL
#include "../bbnnet/ifcb.h"
#endif BBNNET
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/text.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/callout.h"
#include "../h/clist.h"
#include "../h/cmap.h"
/*
 * System parameter formulae.
 *
 * This file is copied into each directory where we compile
 * the kernel; it should be modified there to suit local taste
 * if necessary.
 *
 * Compiled with -DHZ=xx -DTIMEZONE=x -DDST=x -DMAXUSERS=xx
 */

int	hz = HZ;
int	timezone = TIMEZONE;
int	dstflag = DST;
#define	NPROC (20 + 8 * MAXUSERS)
int	nproc = NPROC;
int	ntext = 24 + MAXUSERS;
int	ninode = (NPROC + 16 + MAXUSERS) + 32;
int	nfile = 8 * (NPROC + 16 + MAXUSERS) / 10 + 32;
int	ncallout = 16 + MAXUSERS;
int	nclist = 100 + 16 * MAXUSERS;
#ifdef BBNNET
int     nnetpages = NNETPAGES;
int     nwork = NWORK;
int     nnetcon = NCON;
int     nhost = NHOST;
int	ngate = NGATE;
#endif BBNNET

/*
 * These are initialized at bootstrap time
 * to values dependent on memory size
 */
int	nbuf, nswbuf;

/*
 * These have to be allocated somewhere; allocating
 * them here forces loader errors if this file is omitted.
 */
struct	proc *proc, *procNPROC;
struct	text *text, *textNTEXT;
struct	inode *inode, *inodeNINODE;
struct	file *file, *fileNFILE;
struct 	callout *callout;
struct	cblock *cfree;

struct	buf *buf, *swbuf;
short	*swsize;
int	*swpf;
char	*buffers;
struct	cmap *cmap, *ecmap;
#ifdef BBNNET
struct  pfree *freetab;
struct  work *workfree, *workNWORK;
struct  ucb *contab, *conNCON;
struct  host *host, *hostNHOST;
struct  gway *gateway, *gatewayNGATEWAY;
struct	if_local *locnet, *locnetNLOCNET;
struct	ifcb *ifcb, *ifcbNIFCB;
struct  net netcb;
struct  net_stat netstat;
#endif BBNNET
