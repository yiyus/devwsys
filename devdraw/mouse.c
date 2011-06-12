#include <lib9.h>
#include <draw.h>
#include <memdraw.h>

#include <ixp.h>

#include "mouse.h"
#include "devdraw.h"
#include "drawfcall.h"

void
addmouse(int i, Mouse m)
{
	Mousebuf *mouse;

	mouse = &window[i]->mouse;
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
}

/*
 * Match queued mouse reads with queued mouse events.
 */
void
matchmouse(int i)
{
	Mousebuf *mouse;
	Tagbuf *mousetags;
	Wsysmsg m;

	mouse = &window[i]->mouse;
	mousetags = &window[i]->mousetags;
	
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
		replymsg(window[i], &m);
	}
}
