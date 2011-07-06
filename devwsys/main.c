/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

#define GETARG() (cp-*argv == strlen(*argv)-1) ? *++argv : cp+1

int debuglevel = 0;
int drawdebug; /* used by libmemdraw */
int nwindow;
Window **window;

/* used by libmemdraw */
int
iprint(char* fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = fprint(2, "devwsys: ");
	n += vfprint(2, fmt, args);
	va_end(args);
	return n;
}

int
main(int argc, char **argv)
{
	char *address, *cp;
	char *argv0;

	address = nil;
	/* XXX TODO: use ARGBEGIN, ... from <ixp_local.h> */
	argv0 = argv[0];
	while(*++argv) {
		if(strcmp(*argv, "--") == 0 || !(**argv == '-')) {
			break;
		}
		for(cp=*argv+1; cp<*argv+strlen(*argv); ++cp) {
			if(*cp == 'd') {
				debuglevel++;
				break;
			} else if(*cp == 'a') {
				address = GETARG();
				break;
			} else {
				fprint(1, "usage: %s [-d] [ -a ADDRESS ]", argv0);
				exit(1);
			}
		}
	}
	drawdebug = debuglevel;

	/* Connect to X */
	if(xinit() != 0)
		fatal("unable to connect to X server");

	memimageinit();

	/* Start the 9p server */
	return ixpserve(address);
}

void
_assert(char *fmt)
{
	fatal("assert failed: %s", fmt);
}
