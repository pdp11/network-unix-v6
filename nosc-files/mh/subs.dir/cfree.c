/* Conditional free -- perform a free call if the address passed
 * is in free storage;  else NOP
 */


cfree(addr)
char *addr;
{
	extern end;

	if(addr >= &end) free(addr);
}
