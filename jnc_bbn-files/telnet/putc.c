#
/*
 * Buffered character and word output.
 * as similar as possible to the standard Unix version,
 * but is much more careful about errors and probably
 * a bit slower.
 *
 * Imports: creat (II), write (II), errno
 * Exports: FCreat(), PutC(), PutW(), FFlush(), FPutBlock()
 * Author: Jon Dreyer, October, 1978
 * Fixed up 12/13/78 so it doesn't output an extra byte each
 *	time buffer is flushed automatically.
 */
struct iobuf {
	int fildes;	/* file desc. of output file */
	int nunused;	/* # free bytes in buff */
	char *xfree;	/* ptr to next free slot */
	char buff[512];	/* dis is it */
};

#define MODE 0664	/* mode of created file */

extern int errno;

/*
 * FCreat -- creates file (mode MODE) and readies it for
 * buffered output via PutC and PutW
 */
FCreat(FileName, Buf) char *FileName; struct iobuf *Buf;{
	register struct iobuf *B;	/* just a quick Buf */
	B = Buf;
	if ((B->fildes = creat(FileName, MODE)) < 0)
		return (-1);
	else {
		B->nunused = 512;
		B->xfree = B->buff;
		return (0);
	}
}

/*
 * PutC -- Put one char (buffered)
 * Returns the char if ok, else returns -1.
 * Note that a -1 test won't work if there's a chance you'll
 * be putting a -1.
 */
PutC(Ch, Buf) char Ch; struct iobuf *Buf; {
	*(Buf->xfree)++ = Ch;
	if (--(Buf->nunused) == 0)
		if (FFlush(Buf) < 0)
			return (-1);
	return (Ch);
}

/*
 * PutW -- Like PutC, but for words
 */
PutW(Wd, Buf) int Wd; struct iobuf *Buf; {
	struct {char LowByte, HighByte;};
	/* Quick and vutt! */
	PutC((&Wd)->LowByte, Buf);
	PutC((&Wd)->HighByte, Buf);
	return (errno ? -1 : Wd);
}


/*
 * FFlush -- flush buffer
 * Used by PutC and PutW and also by the user.
 * Returns -1 if there's a problem.
 */
FFlush(Buf) struct iobuf *Buf; {
	register struct iobuf *B;	/* for short */
	register int NBytes;
	B = Buf;
	NBytes = 512 - B->nunused;
	if (write(B->fildes, B->buff, NBytes) < NBytes)
		return (-1);
	B->nunused = 512;
	B->xfree = B->buff;
	return (0);
}

/*
 * FEven -- ensure word boundary
 */
FEven(Buf) struct iobuf *Buf; {
	if (Buf->xfree % 2)
		PutC(0, Buf);
}

/*
 * FPutBlock -- output a block of data
 */
FPutBlock(Block, NBytes, Buf) char *Block; struct iobuf *Buf; {
	while (NBytes--)
		PutC(*Block++, Buf);
	return (errno ? -1 : 0);
}
