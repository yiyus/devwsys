#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

void addmouse(Mousebuf*, Mouse, int);
void matchmouse(Mousebuf*, Reqbuf*);
void replymouse(Mousebuf*, void*);

void
writemouse(Window *w, Mouse m, int resized)
{
	Mousebuf *mouse;
	Reqbuf *mousereqs;

	mouse = &w->mouse;
	mousereqs = &w->mousereqs;

	addmouse(mouse, m, resized);
	matchmouse(mouse, mousereqs);
}

void
readmouse(Window *w, void* r)
{
	Mousebuf *mouse;
	Reqbuf *mousereqs;

	mouse = &w->mouse;
	mousereqs = &w->mousereqs;

	addreq(mousereqs, r);
	// fprint(2, "mouse unstall\n");
	mouse->stall = 0;
	matchmouse(mouse, mousereqs);
}

void
addmouse(Mousebuf *mouse, Mouse m, int resized)
{
	if(!mouse->open || mouse->stall)
		return;

	mouse->m[mouse->wi] = m;
	mouse->wi++;
	if(mouse->wi == nelem(mouse->m))
		mouse->wi = 0;
	if(mouse->wi == mouse->ri){
		mouse->stall = 1;
		mouse->ri = 0;
		mouse->wi = 1;
		mouse->m[0] = m;
		/* fprint(2, "mouse stall\n"); */
	}
	mouse->resized = resized;
}

void
matchmouse(Mousebuf *mouse, Reqbuf *mousereqs)
{
	while(mouse->ri != mouse->wi && mousereqs->ri != mousereqs->wi){
		replymouse(mouse, nextreq(mousereqs));
		mouse->ri++;
		if(mouse->ri == nelem(mouse->m))
			mouse->ri = 0;
	}
}

void
replymouse(Mousebuf *mouse, void *r)
{
	int resized;
	char buf[48], c;
	Mouse m;

	m = mouse->m[mouse->ri];
	resized = mouse->resized;
	c = 'm';
	if(mouse->resized)
		c = 'r';
	sprint(buf, "%c%11d %11d %11d %11ld ", c, m.xy.x, m.xy.y, m.buttons, m.msec);
	ixprread(r, buf);
	mouse->resized = 0;
}
