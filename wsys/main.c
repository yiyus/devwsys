/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"

#define GETARG() (cp-*argv == strlen(*argv)-1) ? *++argv : cp+1

int debug = 0;
int drawdebug; /* used by libmemdraw */
int nwindow;
Window **window;

int
main(int argc, char **argv)
{
	int xfd;
	char *address, *cp, *err;
	char *argv0;

	address = nil;
	argv0 = argv[0];
Nextarg:
	while(*++argv) {
		if(strcmp(*argv, "--") == 0 || **argv != '-') {
			break;
		}
		for(cp=*argv+1; cp<*argv+strlen(*argv); ++cp) {
			switch(*cp){
			case 'd':
				debug |= Debug9p;
				continue;
			case 'D':
				debug |= Debugdraw;
				continue;
			case 'E':
				debug |= Debugevent;
				continue;
			case 'a':
				address = GETARG();
				if(address != nil)
					goto Nextarg;
			default:
				goto Usage;
			}
		}
	}
	if(*argv){
Usage:
		fprint(1, "usage: %s [-d] [ -a address ]", argv0);
		exit(1);
	}

	/* Init memdraw */
	drawdebug = debug&Debugdraw;
	memimageinit();

	/* Connect to X */
	xfd = xinit();
	if(xfd < 0)
		fatal("unable to connect to X server");

	/* 9p server loop */
	err = fsloop(address, xfd);

	xclose();
	if(err != nil)
		fatal("%s", err);
	return 0;
}

void
_assert(char *fmt)
{
	fatal("assert failed: %s", fmt);
}
