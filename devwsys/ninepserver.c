/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <ninep.h>
#include <ninepserver.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

void
fsloop(char *address, int xfd)
{
	char *err;
	Ninepserver s;

	server = &s;
	ninepinit(&s, &ops, address, 0555, 0);
	if(debuglevel > 0)
		ninepdebug();
	fsinit(&s);
	for(;;) {
		nineplisten(&s, xfd);
		err = ninepwait(&s);
		if(err != nil)
			fprintf(2, err);
		if(ninepready(&s, xfd))
			xnextevent();
		ninepprocess(&s);
	}
	ninepend(&s);
	return;
}

