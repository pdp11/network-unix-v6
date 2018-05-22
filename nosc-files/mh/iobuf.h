/* file used for getc/putc */
struct iobuf {
	int	b_fildes;
	int	b_nleft;
	char	*b_nextp;
	char	b_buff[512];
};
