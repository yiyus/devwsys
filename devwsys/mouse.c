#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include "dat.h"
#include "fns.h"

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
	Tagbuf *mousetags;
	Wsysmsg m;

	if(!w->mouseopen)
		return;
	mouse = &w->mouse;
	mousetags = &w->mousetags;

	while(mouse->ri != mouse->wi && mousetags->ri != mousetags->wi){
		m.type = Rrdmouse;
		m.tag = mousetags->t[mousetags->ri];
		m.v = mousetags->r[mousetags->ri];
		mousetags->ri++;
		if(mousetags->ri == nelem(mousetags->t))
			mousetags->ri = 0;
		m.mouse = mouse->m[mouse->ri];
		m.resized = mouse->resized;
		/*
		if(m.resized)
			fprint(2, "sending resize\n");
		*/
		mouse->resized = 0;
		mouse->ri++;
		if(mouse->ri == nelem(mouse->m))
			mouse->ri = 0;
		replymsg(w, &m);
	}
}
