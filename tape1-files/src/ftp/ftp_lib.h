/* declarations to go with ftp_lib.c 
*/

extern int type, mode, stru;

#define TYPEA   0 /* ASCII Non-print                                    */
#define TYPEAN  0 /* ASCII Non-print                                    */
#define TYPEAT  1 /* ASCII Telnet format effectors (not supported)      */
#define	TYPEAC  2 /* ASCII Carriage Control characters (not supported)  */
#define TYPEEN  3 /* EBCDIC Non-print (not supported)                   */
#define TYPEET  4 /* EBCDIC Telnet format effectors (not supported)     */
#define	TYPEEC  5 /* EBCDIC Carriage control characters (not supported) */
#define TYPEI   6 /* Image mode                                         */
#define TYPEL   7 /* Local mode (only 8 bit byte supported)             */
#define	STRUF   0 /* file structure                                     */
#define STRUR   1 /* record structure                                   */
#define STRUP   2 /* page structure (not supported)                     */
#define	MODES   0 /* stream mode                                        */
#define	MODEB   1 /* block mode (not supported)                         */
#define MODEC   2 /* compressed mode (not supported)                    */

extern int abrtxfer;
