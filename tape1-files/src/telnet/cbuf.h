#define CBUFSIZE 128

/*
 * Circular buffer manager
 *
 * These routines manage circular buffers declared in the user's program.
 * To use the routines, put
 *	#include "/u1/dan/lib/cbuf.h"
 * in your source, and compile with the library /u1/dan/lib/lib.a.
 * Declare the circular buffers as structures of type CircBuf; e.g.,
 *	struct CircBuf buf;
 * Before using a circular buffer, initialize it with
 *	Cinit(buf);
 * All of these functions take the structure itself, rather than a pointer,
 * as an argument. If all you have is a pointer, indirect through it.
 * E.g., Cinit(buf) or Cinit(*bufp) will work.
 *
 * The routines are:
 *
 * Cinit(cbuf) initializes cbuf.
 * Cgetc(cbuf) returns the next character from cbuf, or -1 if it is empty.
 * Cungetc(ch, cbuf) puts ch in cbuf where it will be the next char getc'ed.
 * Cputc(ch, cbuf) puts ch in cbuf, unless it is full.
 * Cfull(cbuf) returns true (nonzero value) iff cbuf is full.
 * Cempty(cbuf) returns true iff cbuf is empty.
 * Clen(cbuf) returns the number of bytes in cbuf.
 * Cfree(cbuf) returns the number of bytes free in cbuf.
 * Cwrite(fd, cbuf, nwrite) is an interface to the system write call which
 *    accepts a circular buffer as argument.  It requires only one write
 *    call to empty the buffer.
 * Cread(fd, cbuf, nread) is an interface to the system read call which
 *    accepts a circular buffer as argument.  It requires only one read
 *    call to fill the buffer (if there's that much to read).
 * Cgetb(ch,cb,n) copies n or less bytes from cb to where ch points
 * Cputb(ch,cb,n) copies n or less bytes into cb from where ch points
 *
 * The size of a circular buffer is the parameter CBUFSIZE above.
 * If you change it, you must recompile cbuf.c. Sorry about that.
 *
 * A message is printed when an attempt is made to getc an empty
 * buffer or putc a full one.
 */

struct CircBuf
   {
   char *c_get;
   char *c_put;
   int c_full;
   char c_buf[CBUFSIZE];
   };
#define Cinit(cb)    {(cb).c_get=(cb).c_put=(cb).c_buf; (cb).c_full=0;}
#define Cempty(cb)   ((cb).c_get == (cb).c_put && !(cb).c_full)
#define Cgetc(cb)    _Cgetc(&(cb))
#define Cungetc(ch,cb) _Cungetc(ch, &(cb))
#define Cputc(ch,cb) _Cputc(ch, &(cb))
#define Cfull(cb)    (cb).c_full
#define Clen(cb)     _Clen(&(cb))
#define Cfree(cb)   (CBUFSIZE - _Clen(&(cb)))
#define Cwrite(fd,cb,n) _Cwrite(fd, &(cb), n)
#define Cread(fd,cb,n) _Cread(fd, &(cb), n)
#define Cgetb(ch,cb,n) _Cgetb(ch, &(cb), n)
#define Cputb(ch,cb,n) _Cputb(ch, &(cb), n)
