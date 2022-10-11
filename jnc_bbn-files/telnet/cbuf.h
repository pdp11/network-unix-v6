#define CBUFSIZE 500
#define Cdebug x

/*
 * Circular buffer manager
 *
 * These routines manage circular buffers declared in the user's program.
 * To use the routines, put
      #include "/usr/dan/net/cbuf.h"
 * in your source, and compile with the library cbuflib.a.
 * The routines are implemented partially as macros for
 * convenience (and someday for efficiency). Thus, all of them take
 * the structure itself, rather than a pointer, as an argument. If all you
 * have is a pointer, indirect through it. E.g., Cinit(buf) or Cinit(*bufp)
 * will work. Declare the buffers as
 *    struct CircBuf buf1;
 * The routines are:
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
 *
 * The size of a circular buffer is 256 bytes.
 *
 * To cause a message to be printed when an attempt is made to getc an empty
 * buffer or putc a full one, recompile the routines with
      #define Cdebug x
 */

struct CircBuf
   {
   char *c_get;
   char *c_put;
   int c_full;
   char c_buf[CBUFSIZE];
   };
#define Cinit(cb)    {(cb).c_get = (cb).c_put = (cb).c_buf; (cb).c_full = 0;}
#define Cempty(cb)   ((cb).c_get == (cb).c_put && !(cb).c_full)
#define Cgetc(cb)    _Cgetc(&(cb))
#define Cungetc(ch,cb) _Cungetc(ch, &(cb))
#define Cputc(ch,cb) _Cputc(ch, &(cb))
#define Cfull(cb)    (cb).c_full
#define Clen(cb)     _Clen(&(cb))
#define Cfree(cb)   (CBUFSIZE - _Clen(&(cb)))
#define Cwrite(fd,cb,n) _Cwrite(fd, &(cb), n)
#define Cread(fd,cb,n) _Cread(fd, &(cb), n)
