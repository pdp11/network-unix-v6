#define TNSE    240                 /*      TELNET    */
#define TNNOP   241                 /*      special   */
#define TNDM    242                 /*     function   */
#define TNBRK   243                 /*    characters  */
#define TNIP    244
#define TNAO    245
#define TNAYT   246
#define TNEC    247
#define TNEL    248
#define TNGA    249
#define TNSB    250
#define TNWILL  251
#define TNWONT  252
#define TNDO    253
#define TNDONT  254
#define TNIAC   255


/* User Telnet Keyboard Mappings */

#define UTIP    0177            /* del */
#define UTEC    043             /* # */
#define UTEL    0100            /* @ */
#define UTAO    01              /* ctrl A */
#define UTBRK   0               /* nul or "arg" */
#define UTAYT   05              /* ctrl E */

#define UTQUOTE 021             /* ctrl Q */
#define CEOT    04              /* ctrl D */
#define UTRETYP 022             /* ctrl R */
