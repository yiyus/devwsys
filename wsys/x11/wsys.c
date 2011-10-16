#include <lib9.h>
#include <kern.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

void
xbottomwindow(Window *w)
{
	Xwin *xw;

	xtogglefullscreen(xconn.fullscreen);
	xw = w->x;
	XLowerWindow(xconn.display, xw->drawable);
	XFlush(xconn.display);
}

void
xtopwindow(Window *w)
{
	Xwin *xw;

	xtogglefullscreen(xconn.fullscreen);
	xw = w->x;
	XMapRaised(xconn.display, xw->drawable);
	XFlush(xconn.display);
}

void
xcurrentwindow(Window *w)
{
	Xwin *xw;

	xtogglefullscreen(xconn.fullscreen);
	xw = w->x;
	XMapRaised(xconn.display, xw->drawable);
	XSetInputFocus(xconn.display, xw->drawable, RevertToPointerRoot,
		CurrentTime);
	XFlush(xconn.display);
}

void
xresizewindow(Window *w, Rectangle r)
{
	XWindowChanges e;
	int value_mask;
	Xwin *xw;

	if(w != xconn.fullscreen || ! eqrect(r, xconn.screenrect))
		xtogglefullscreen(xconn.fullscreen);
	xw = w->x;
	memset(&e, 0, sizeof e);
	value_mask = CWX|CWY|CWWidth|CWHeight;
	e.x = r.min.x;
	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(xconn.display, xw->drawable, value_mask, &e);
	if(w->visible)
		XMapWindow(xconn.display, xw->drawable);
	else
		XUnmapWindow(xconn.display, xw->drawable);
	XFlush(xconn.display);
}

void
xmovewindow(Window *w, Rectangle r)
{
	XWindowChanges e;
	int value_mask;
	Xwin *xw;

	xtogglefullscreen(xconn.fullscreen);
	xw = w->x;
	memset(&e, 0, sizeof e);
	value_mask = CWX|CWY|CWWidth|CWHeight;
	e.x = r.min.x;
	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(xconn.display, xw->drawable, value_mask, &e);
	XFlush(xconn.display);
}

void xtogglefullscreen(Window* w)
{
	Mouse m;
	XSetWindowAttributes attr;
	Xwin *xw;

	if(w == nil)
		return;
	xw = w->x;
	if(w == xconn.fullscreen){
		xconn.fullscreen = nil;
		xresizewindow(w, xconn.restore);
		XUngrabKeyboard(xconn.display, CurrentTime);
		attr.override_redirect = False;
		XChangeWindowAttributes(xconn.display, xw->drawable, CWOverrideRedirect, &attr);
		XUnmapWindow(xconn.display, xw->drawable);
		XMapRaised(xconn.display, xw->drawable);
	}else{
		xconn.restore = rectaddpt(w->screenr, w->orig);
		/* xconn.fullscreen must be nil when xresizewindow is called */
		xresizewindow(w, xconn.screenrect);
		xconn.fullscreen = w;
		attr.override_redirect = True;
		XChangeWindowAttributes(xconn.display, xw->drawable, CWOverrideRedirect, &attr);
		XUnmapWindow(xconn.display, xw->drawable);
		XMapRaised(xconn.display, xw->drawable);
		XGrabKeyboard(xconn.display, xw->drawable, False, GrabModeAsync, GrabModeAsync, CurrentTime);
	}
	writemouse(w, m, 1);
}

