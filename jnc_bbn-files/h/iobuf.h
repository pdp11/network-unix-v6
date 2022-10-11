/*
   This is the structure used by the buffered IO routines putc(III) and getc(III).
   It is not used anywhere in the kernel.
*/

struct iobuf
	{
	int io_fildes;
          int io_nleft;
          char *io_nextp;
	char io_buff[512];
	};
