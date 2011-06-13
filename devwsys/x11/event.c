#include <lib9.h>
#include <draw.h>
#include "inc.h"
#include "x.h"
#include "keyboard.h"
#include "mouse.h"

#define debug(...) if(debuglevel) fprint(2, __VA_ARGS__)

extern int nwindow;
extern int debuglevel;

int lookupxwin(XWindow);
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
		i = lookupxwin(xev.xclient.window);
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
	debug("Configure event at window %d: w=%d h=%d\n", i, m.xy.x, m.xy.y);
	addmouse(i, m, 1);
	matchmouse(i);
}

void
kbdevent(XEvent xev)
{
	int i, c;
	KeySym k;

	i = lookupxwin(xev.xany.window);
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
	if((c = xtoplan9kbd(&xev)) < 0)
		return;
	debug("Keyboard event at window %d. rune=%c\n", i, c);
	addkbd(i, c);
	matchkbd(i);
}

void
mouseevent(XEvent xev)
{
	int i;
	Mouse m;

	i = lookupxwin(xev.xany.window);
	if(i < 0)
		return;
	if(xtoplan9mouse(&xev, &m) < 0)
		return;
	debug("Mouse event at window %d: x=%d y=%d b=%d\n", i, m.xy.x, m.xy.y, m.buttons);
	addmouse(i, m, 0);
	matchmouse(i);
}
