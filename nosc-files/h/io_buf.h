/* This structure can be used as a template for a structure
 * used with putc/putw/getc/getw.  This .h file is referenced
 * by getch/putch in ../util .
 */
struct io_buf
{
    int fid;
    int unused;
    char *addr;
    char bf[512];
};
