#
/*
 *		Log package for EDN
 *
 * Imports: "specfmt.h", FCreat(), PutC(), PutW(), FFlush(),
 *		FEven(), FPutBlock(), errno, qtime()
 * Exports: LogInit(), Log(), LogEnd()
 * Author: Jon Dreyer, October 1978
 */
#include "specfmt.h"
#define FCREAT(LogFName)	FCreat(LogFName, &lOGbUF)
#define PUTC(C)		PutC(C, &lOGbUF)
#define PUTW(W)		PutW(W, &lOGbUF)
#define FPUTBLOCK(B,N)	FPutBlock(B, N, &lOGbUF)
#define FEVEN		FEven(&lOGbUF)
#define FFLUSH		FFlush(&lOGbUF)
#define ENDARGS		-1	/* terminates an arg list to Log */
#define NOLENGTH	-1	/* placed in length field when unknown */
extern int errno;

struct qtimebuf {
	long QSeconds;
	int QSixtieths;
};

struct qtimebuf StartTime;

struct {	/* for PutC, etc. */
	int fildes;
	int nunused;
	char *xfree;	/* ptr to next free slot */
	char buff[512];
} lOGbUF;

LogInit(LogFName) char *LogFName; {
	if (FCREAT(LogFName) < 0) {
		perror("LogInit");
		return (-1);
	}
	else {
		qtime(&StartTime);
		LogHeader(0, &StartTime, "");
		return (0);
	}
}

Log(MeasId, Format, Args) char *Format; {
	struct qtimebuf TimeStamp;
	register int *ArgPt;	/* used to pick off args */
	int OldErrNo;		/* holds errno during call */
	char Type;		/* type of conversion for current arg */
	register char *BytePt;
	register int ByteCt;
	OldErrNo = errno; errno = 0;
		/* the above kludge is so I can decide if there have
		 * been log errors without checking the return value,
		 * yet leaving errno as it was on return in case the
		 * user wants errno as it was before the call.
		 */
	qtime(&TimeStamp);
	/* change timestamp to time since LogInit */
	TimeStamp.QSeconds =- StartTime.QSeconds;
	TimeStamp.QSixtieths =- StartTime.QSixtieths;
	if (TimeStamp.QSixtieths > 60) { /* qtime anomaly */
		TimeStamp.QSeconds++;
		TimeStamp.QSixtieths =- 60;
	}
	else if (TimeStamp.QSixtieths < 0) { /* must "borrow" */
		TimeStamp.QSeconds--;
		TimeStamp.QSixtieths =+ 60;
	}
	LogHeader(MeasId, &TimeStamp, Format);
	ArgPt = &Args;	/* printf trick for getting args */
	while (Type = *Format++) { /* for each entry in fmt string */
		switch (Type) { /* log corresp. arg properly */
		case 'i': case 'I':	/* integer */
			FEVEN;
			PUTW(*ArgPt++);
			break;
		case 'b': case 'B': case 'c':	/* byte */
			PUTC (*ArgPt++);
			break;
		case 'l': case 'L':	/* long int */
			FEVEN;
			PUTW(*ArgPt++);
			PUTW(*ArgPt++);
			break;
		case 's': case 'S':	/* ptr to text string (null-term.) */
			FEVEN;
			PUTW(NOLENGTH);
			BytePt = *ArgPt++;
			while (PUTC(*BytePt++)); /* output text */
			break;
		case 'r': case 'R':	/* ptr to record */
			FEVEN;
			BytePt = *ArgPt++;
			ByteCt = *ArgPt++;	/* next arg is byte ct */
			PUTW(ByteCt);
			FPUTBLOCK(BytePt, ByteCt);
			break;
		case 'n':	/* internet header */
			FEVEN;
			PUTW(INHSIZE);
			FPUTBLOCK(*ArgPt++, INHSIZE);
			break;
		case 't':	/* tcp header */
			FEVEN;
			PUTW(TCPHSIZE);
			FPUTBLOCK(*ArgPt++, TCPHSIZE);
			break;
		default:
			printf ("Format type '%c' not implemented.\n", Type);
			errno = OldErrNo;
			return(-1);
		}
	}
	if (*ArgPt != ENDARGS) {
		printf("Log(%d,...): wrong number of args\n", MeasId);
		errno = OldErrNo;
		return(-1);
	}
	if (errno) {
		perror("Log");
		return(-1);
	}
	errno = OldErrNo;
	return(0);
}

/* LogHeader -- make header of log record
 */
LogHeader(Meas, Time, Form)
	int Meas;		/* meas id */
	struct qtimebuf *Time;	/* timestamp */
	char *Form;		/* format string */
{
	FEVEN;
	PUTW(NOLENGTH);
	PUTW(Meas);
	FPUTBLOCK(Time, sizeof(*Time));
	PUTW(NOLENGTH);		/* dummy len of fmt string */
	while (PUTC(*Form++));	/* fut fmt string */
}

LogEnd() {
	if (FFLUSH < 0) {
		perror("LogEnd");
		return(-1);
	}
	else
		return(0);
}
