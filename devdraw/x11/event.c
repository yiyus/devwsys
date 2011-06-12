#include <lib9.h>
#include <draw.h>
#include "inc.h"
#include "x.h"
#include "../mouse.h"

#define debug(...) if(0 && debuglevel) fprint(2, "X: " __VA_ARGS__)

extern int nwindow;
extern int debuglevel;

int lookupxwin(XWindow);
void configevent(XEvent);
void mouseevent(XEvent);

int
xnextevent(void) {
	int i;
	XEvent xev;

	debug("Next event\n");
	XNextEvent(xconn.display, &xev);
	debug("\t Event window: %d\n", xev.xany.window);
	debug("\t Event type: ");
	switch(xev.type){
	case ClientMessage:
		i = lookupxwin(xev.xclient.window);
		if(xev.xclient.data.l[0] == xwindow[i]->wmdelmsg) {
			debug("Delete window msg\n");
			XDestroyWindow(xconn.display, xev.xclient.window);
			XSync(xconn.display, False);
			return i;
		}
		break;

	case ConfigureNotify:
		debug("Configure event\n");
		configevent(xev);
		break;

	case Expose:
		debug("Window exposed\n");
		//xexpose(xev);
		break;
	
	case DestroyNotify:
		debug("Window destroyed\n");
		/* Nop */
		break;

	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
		debug("Mouse event\n");
		mouseevent(xev);
		break;

	default:
		debug("Other event (%d)\n", xev.type);
		break;
	}
	return -1;
}

int
lookupxwin(XWindow w)
{
	int i;

	for(i = 0; i < nwindow; i++){
		if(xwindow[i]->drawable == w)
			return i;
	}
	return -1;
}

void
configevent(XEvent xev)
{
	int i;
	Mouse m;

	i = lookupxwin(xev.xany.window);
	if(i < 0)
		return;
	debug("Configure event at window %d\n", i);
	// TODO: plan9port/src/cmd/devdraw/x11-init.c:686
	m.xy.x = xev.xconfigure.width;
	m.xy.y = xev.xconfigure.height;
	// _xreplacescreenimage();
	addmouse(i, m, 1);
	matchmouse(i);
}

void
mouseevent(XEvent xev)
{
	int i;
	Mouse m;

	i = lookupxwin(xev.xany.window);
	if(i < 0)
		return;
	debug("Mouse event at window %d\n", i);
	if(xtoplan9mouse(&xev, &m) < 0)
		return;
	addmouse(i, m, 0);
	matchmouse(i);
}
