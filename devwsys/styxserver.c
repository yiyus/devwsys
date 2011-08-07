/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <styxserver.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

/* From libstyx */
int styxnewmsg(Styxserver*, int);
void styxnewclient(Styxserver*, int);
void styxfreeclient(Styxserver*, int);

void
fsloop(char *address, int xfd)
{
	int xfd;
	Styxserver s;

	server = &s;
	styxinit(&s, &ops, address, 0555, 0);
	fsysinit(&s);
	for(;;) {
		styxnewclient(&s, xfd);
		styxwait(&s);
		if(styxnewmsg(&s, xfd)){
			xnextevent();
			styxfreeclient(&s, xfd);
		}
		styxprocess(&s);
	}
	return 0;
}
