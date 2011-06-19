#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include "inc.h"
#include "x.h"
#include "dat.h"
#include "fns.h"

void configevent(XEvent);
void kbdevent(XEvent);
void mouseevent(XEvent);

int
xnextevent(void) {
	int i;
	XEvent xev;

	debug("Next event: ");
	XNextEvent(xconn.display, &xev);
	debug("window: %d\t", xev.xany.window);
	debug("type: ");
	switch(xev.type){
	case ClientMessage:
		i = xlookupwin(xev.xclient.window);
		if(xev.xclient.data.l[0] == xwindow[i]->wmdelmsg) {
			debug("Delete window msg\n");
			XDestroyWindow(xconn.display, xev.xclient.window);
			XSync(xconn.display, False);
			return i;
		}
		break;

	case ConfigureNotify:
		debug("ConfigureNotify\n");
		configevent(xev);
		break;

	case Expose:
		debug("Expose\n");
		//xexpose(xev);
		break;
	
	case DestroyNotify:
		debug("DestroyNotify\n");
		/* Nop */
		break;

	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
		debug("Mouse\n");
		mouseevent(xev);
		break;

	case KeyPress:
		debug("KeyPress\n");
		kbdevent(xev);
		break;
	
	default:
		debug("Unknown (%d)\n", xev.type);
		break;
	}
	return -1;
}

void
configevent(XEvent xev)
{
	int i;
	Mouse m;

	i = xlookupwin(xev.xany.window);
	if(i < 0)
		return;
	debug("Configure event at window %d\n", i);
	// TODO: plan9port/src/cmd/devdraw/x11-init.c:686
	m.xy.x = xev.xconfigure.width;
	m.xy.y = xev.xconfigure.height;
	// _xreplacescreenimage();
	debug("Configure event at window %d: w=%d h=%d\n", i, m.xy.x, m.xy.y);
	addmouse(i, m, 1);
	matchmouse(i);
}

void
kbdevent(XEvent xev)
{
	int i;
	KeySym k;

	i = xlookupwin(xev.xany.window);
	if(i < 0)
		return;
	XLookupString((XKeyEvent*)&xev, NULL, 0, &k, NULL);
	if(k == XK_F11){
		/* TODO
		fullscreen = !fullscreen;
		_xmovewindow(fullscreen ? screenrect : windowrect);
		*/
		return;
	}

	k = kbdputc(xtoplan9kbd(&xev));
	if(k == -1)
		return;
	debug("Keyboard event at window %d. rune=%C (%d)\n", i, k, k);
	addkbd(i, k);
	matchkbd(i);
}

void
mouseevent(XEvent xev)
{
	int i;
	Mouse m;

	i = xlookupwin(xev.xany.window);
	if(i < 0)
		return;
	if(xtoplan9mouse(&xev, &m) < 0)
		return;
	debug("Mouse event at window %d: x=%d y=%d b=%d\n", i, m.xy.x, m.xy.y, m.buttons);
	addmouse(i, m, 0);
	matchmouse(i);
}
