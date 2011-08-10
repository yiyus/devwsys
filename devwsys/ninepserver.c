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
			fprint(2, err);
		if(ninepready(&s, xfd))
			xnextevent();
		ninepdefault(&s);
	}
	ninepend(&s);
	return;
}

