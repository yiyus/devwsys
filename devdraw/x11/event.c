#include <lib9.h>
#include <draw.h>
#include "inc.h"
#include "x.h"

#define debug(...) if(debuglevel) fprint(2, "X: " __VA_ARGS__)

extern int nwindow;
extern int debuglevel;

int xlookupwin(XWindow);

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
		i = xlookupwin(xev.xclient.window);
		if(xev.xclient.data.l[0] == xwindow[i]->wmdelmsg) {
			debug("Delete window msg\n");
			XDestroyWindow(xconn.display, xev.xclient.window);
			XSync(xconn.display, False);
			return i;
		}
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
		//debug("Mouse event\n");
		//xmouse(xev);
		break;

	default:
		debug("Other event (%d)\n", xev.type);
		break;
	}
	return -1;
}

int
xlookupwin(XWindow w)
{
	int i;

	for(i = 0; i < nwindow; i++){
		if(xwindow[i]->drawable == w)
			return i;
	}
	return -1;
}
