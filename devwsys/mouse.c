#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

void replymouse(Mousebuf*, void*);

void
addmouse(Window *w, Mouse m, int resized)
{
	Mousebuf *mouse;

	if(!w->mouseopen)
		return;
	mouse = &w->mouse;
	if(mouse->stall)
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

/*
 * Match queued mouse reads with queued mouse events.
 */
void
matchmouse(Window *w)
{
	Mousebuf *mouse;
	Reqbuf *mousereqs;
	void *r;

	if(!w->mouseopen)
		return;
	mouse = &w->mouse;
	mousereqs = &w->mousereqs;

	while(mouse->ri != mouse->wi && mousereqs->ri != mousereqs->wi){
		r = mousereqs->r[mousereqs->ri];
		mousereqs->ri++;
		if(mousereqs->ri == nelem(mousereqs->r))
			mousereqs->ri = 0;
		replymouse(mouse, r);
		mouse->ri++;
		if(mouse->ri == nelem(mouse->m))
			mouse->ri = 0;
	}
}

void
readmouse(Window *w, void* r)
{
	Mousebuf *mouse;
	Reqbuf *mousereqs;

	mouse = &w->mouse;
	mousereqs = &w->mousereqs;

	mousereqs->r[mousereqs->wi] = r;
	mousereqs->wi++;
	if(mousereqs->wi == nelem(mousereqs->r))
		mousereqs->wi = 0;
	if(mousereqs->wi == mousereqs->ri)
		sysfatal("too many queued mouse reads");
	// fprint(2, "mouse unstall\n");
	mouse->stall = 0;
	matchmouse(w);
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
	ixpread(r, buf);
	mouse->resized = 0;
}
