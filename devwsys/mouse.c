#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

void
writemouse(Window *w, Mouse m, int resized)
{
	char buf[50], c;

	/*
	 * TODO:
	 * If the last event in the queue is a resize,
	 * overwrite it instead of adding a new event.
	 */
	c = 'm';
	if(resized)
		c = 'r';
	sprint(buf, "%c%11d %11d %11d %11ld ", c, m.xy.x, m.xy.y, m.buttons, m.msec);
	w->mousebuttons = m.buttons;
	ixppwrite(w->mousep, buf);
}

