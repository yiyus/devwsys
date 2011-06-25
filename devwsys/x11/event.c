#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "inc.h"
#include "x.h"
#include "dat.h"
#include "fns.h"

#define debugev(...) if(0) debug(__VA_ARGS__)

void configevent(Window *w, XEvent);
void kbdevent(Window *w, XEvent);
void mouseevent(Window *w, XEvent);

void
xnextevent(void) {
	int i;
	Window *w;
	Xwin *xw;
	XEvent xev;

	xw = nil;
	XNextEvent(xconn.display, &xev);
	for(i = 0; i < nwindow; i++){
		xw = window[i]->x;
		if(xw && xw->drawable == xev.xany.window)
			break;
	}
	if(i == nwindow)
		return;
	w = window[i];
	switch(xev.type){
	case ClientMessage:
		if(xev.xclient.data.l[0] == xw->wmdelmsg)
			deletewin(w);
		break;

	case ConfigureNotify:
		configevent(w, xev);
		break;

	case Expose:
		//xexpose(w, xev);
		break;
	
	case DestroyNotify:
		/* Nop */
		break;

	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
		mouseevent(w, xev);
		break;

	case KeyPress:
		kbdevent(w, xev);
		break;
	
	case FocusOut:
		/*
		 * Stop alting when
		 * window losts focus.
		 */
		writekbd(w, -1);;
		break;
	
	default:
		break;
	}
}

void
configevent(Window *w, XEvent xev)
{
	Mouse m;

	// TODO: plan9port/src/cmd/devdraw/x11-init.c:686
	m.xy.x = xev.xconfigure.width;
	m.xy.y = xev.xconfigure.height;
	// _xreplacescreenimage();
	debugev("Configure event at window %d: w=%d h=%d\n", w->id, m.xy.x, m.xy.y);
	writemouse(w, m, 1);
}

void
kbdevent(Window *w, XEvent xev)
{
	KeySym k;

	XLookupString((XKeyEvent*)&xev, NULL, 0, &k, NULL);
	if(k == XK_F11){
		/* TODO
		fullscreen = !fullscreen;
		_xmovewindow(fullscreen ? screenrect : windowrect);
		*/
		return;
	}

	k = xtoplan9kbd(&xev);
	debugev("Keyboard event at window %d. rune=%C (%d)\n", w->id, k, k);
	writekbd(w, k);
}

void
mouseevent(Window *w, XEvent xev)
{
	Mouse m;

	if(xtoplan9mouse(&xev, &m) < 0)
		return;
	debugev("Mouse event at window %d: x=%d y=%d b=%d\n", w->id, m.xy.x, m.xy.y, m.buttons);
	writemouse(w, m, 0);
}
