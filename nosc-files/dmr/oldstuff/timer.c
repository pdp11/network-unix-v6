#
/*
	DR-11a driver for timer
	version 1:	Copies data from register to user space.
			A verified 16 bit number is returned as 2 bytes.
 */


#include "../../h/param.h"
#include "../../h/conf.h"
#include "../../h/user.h"

#define DRADDR	0167770


struct {
	char	lobyte;
	char hibyte;
};

struct {
	int	drcsr;
	int	drwritereg;
	int	drreadreg;
};


timerread ()
{
	int	i;
	while ((i = DRADDR -> drreadreg) != (DRADDR -> drreadreg));
	if (passc (i.lobyte) >= 0) passc (i.hibyte);
}
