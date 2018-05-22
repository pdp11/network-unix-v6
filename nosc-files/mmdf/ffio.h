#
/*********************************************************
      This program was written by Bruce S. Borden

Permission to copy, either in whole or in part, is  hereby
granted  to  anyone  wishing to do so with the restriction
that this notice must accompany any copies.

Copyright (c) 1978 by Bruce S. Borden
		      The Rand Corporation
		      1700 Main Street
		      Santa Monica, California 90406
*********************************************************/
#
#define FF_BUF  (512+16)

long    ff_size(), ff_pos();

struct  {

	int     fs_seek,                /* Total seek sys calls */
		fs_read,                /*       read  "    "   */
		fs_write;               /*       write "    "   */
} ff_stats;
