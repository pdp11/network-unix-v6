#define	LIBN
#include "netlib.h"
#include "netmap.h"
/*
 * stash away dial digits and return index into buffer
 * dial digits in the map are stored in the name buffer
 * _dialext is used to save digits encountered in a numeric address
 */
unsigned
_dialstr(s)
char *s;
{
	unsigned x = _dialcnt;

	if (strlen(s) >= NETNAMSIZ)
		return 0;
	do
		if (_dialcnt >= _DIALSIZ) {
			_dialcnt = x;
			return 0;
		}
	while (_dialext[_dialcnt++] = *s++);
	return _netmap->map_ndial + x;
}
