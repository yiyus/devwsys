#include <lib9.h>
#include <kern.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

#define Mask MouseMask|ExposureMask|StructureNotifyMask|KeyPressMask|EnterWindowMask|LeaveWindowMask|FocusChangeMask

void
xattach(Window *w, char *winsize)
{
	int havemin;

	w->r = xwinrectangle(w->label, winsize, &havemin);
	w->x = xcreatewin(w->label, winsize, w->r);
	w->r = xmapwin(w->x, havemin, w->r);
	w->screenimage = xallocmemimage(w, w->r, xchan(), xscreenpm(w));
	initscreenimage(w->screenimage);
}

Rectangle
xwinrectangle(char *label, char *winsize, int *havemin)
{
	int height, mask, width, x, y;
	Rectangle r;

	/*
	 * We get to choose the initial rectangle size.
	 * This is arbitrary.  In theory we should read the
	 * command line and allow the traditional X options.
	 */
	mask = 0;
	x = 0;
	y = 0;
	if(winsize && winsize[0]){
		if(parsewinsize(winsize, &r, havemin) < 0)
			sysfatal("%r");
	}else{
		/*
		 * Parse the various X resources.  Thanks to Peter Canning.
		 */
		char *screen_resources, *display_resources, *geom, 
			*geomrestype, *home, *file;
		XrmDatabase database;
		XrmValue geomres;

		database = XrmGetDatabase(xconn.display);
		screen_resources = XScreenResourceString(xconn.screen);
		if(screen_resources != nil){
			XrmCombineDatabase(XrmGetStringDatabase(screen_resources), &database, False);
			XFree(screen_resources);
		}

		display_resources = XResourceManagerString(xconn.display);
		if(display_resources == nil){
			home = getenv("HOME");
			if(home!=nil && (file=smprint("%s/.Xdefaults", home)) != nil){
				XrmCombineFileDatabase(file, &database, False);
				free(file);
			}
		}else
			XrmCombineDatabase(XrmGetStringDatabase(display_resources), &database, False);
		geom = smprint("%s.geometry", label);
		if(geom && XrmGetResource(database, geom, nil, &geomrestype, &geomres))
			mask = XParseGeometry(geomres.addr, &x, &y, (uint*)&width, (uint*)&height);
		free(geom);

		if((mask & WidthValue) && (mask & HeightValue)){
			r = Rect(0, 0, width, height);
		}else{
			r = Rect(0, 0, WidthOfScreen(xconn.screen)*3/4,
					HeightOfScreen(xconn.screen)*3/4);
			if(Dx(r) > Dy(r)*3/2)
				r.max.x = r.min.x + Dy(r)*3/2;
			if(Dy(r) > Dx(r)*3/2)
				r.max.y = r.min.y + Dx(r)*3/2;
		}
		if(mask & XNegative){
			x += WidthOfScreen(xconn.screen);
		}
		if(mask & YNegative){
			y += HeightOfScreen(xconn.screen);
		}
		*havemin = 0;
	}

	return r;
}

void*
xcreatewin(char *label, char *winsize, Rectangle r)
{
	char *argv[2];
	int height, mask, width, x, y;
	XClassHint classhint;
	XSetWindowAttributes attr;
	XSizeHints normalhint;
	XTextProperty name;
	Xwin *xw;
	XWMHints hint;

	xw = malloc(sizeof(Xwin));
	if(xw == nil)
		return nil;

	/* remove warnings for unitialized vars */
	height = x = y = mask = width = 0;

	memset(&attr, 0, sizeof attr);
	attr.colormap = xconn.cmap;
	attr.background_pixel = ~0;
	attr.border_pixel = 0;
	xw->drawable = XCreateWindow(
		xconn.display,	/* display */
		xconn.root,	/* parent */
		x,		/* x */
		y,		/* y */
		Dx(r),		/* width */
	 	Dy(r),		/* height */
		0,		/* border width */
		xconn.depth,	/* depth */
		InputOutput,	/* class */
		xconn.vis,		/* visual */
				/* valuemask */
		CWBackPixel|CWBorderPixel|CWColormap,
		&attr		/* attributes (the above aren't?!) */
	);
	/*
	 * Start getting events from the window asap
	 */
	XSelectInput(xconn.display, xw->drawable, Mask);
	/*
	 * WM_DELETE_WINDOW will be sent by the window
	 * manager when the window is trying to be closed.
	 */
	xw->wmdelmsg = XInternAtom(xconn.display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(xconn.display, xw->drawable, &xw->wmdelmsg, 1);

	/*
	 * Label and other properties required by ICCCCM.
	 */
	memset(&name, 0, sizeof name);
	if(label == nil)
		label = "pjw-face-here";
	name.value = (uchar*)label;
	name.encoding = XA_STRING;
	name.format = 8;
	name.nitems = strlen((char*)name.value);

	memset(&normalhint, 0, sizeof normalhint);
	normalhint.flags = PSize|PMaxSize;
	if(winsize && winsize[0]){
		normalhint.flags &= ~PSize;
		normalhint.flags |= USSize;
		normalhint.width = Dx(r);
		normalhint.height = Dy(r);
	}else{
		if((mask & WidthValue) && (mask & HeightValue)){
			normalhint.flags &= ~PSize;
			normalhint.flags |= USSize;
			normalhint.width = width;
			normalhint.height = height;
		}
		if((mask & WidthValue) && (mask & HeightValue)){
			normalhint.flags |= USPosition;
			normalhint.x = x;
			normalhint.y = y;
		}
	}

	normalhint.max_width = WidthOfScreen(xconn.screen);
	normalhint.max_height = HeightOfScreen(xconn.screen);

	memset(&hint, 0, sizeof hint);
	hint.flags = InputHint|StateHint;
	hint.input = 1;
	hint.initial_state = NormalState;

	memset(&classhint, 0, sizeof classhint);
	classhint.res_name = label;
	classhint.res_class = label;

	argv[0] = label;
	argv[1] = nil;

	XSetWMProperties(
		xconn.display,	/* display */
		xw->drawable,	/* window */
		&name,		/* XA_WM_NAME property */
		&name,		/* XA_WM_ICON_NAME property */
		argv,		/* XA_WM_COMMAND */
		1,		/* argc */
		&normalhint,	/* XA_WM_NORMAL_HINTS */
		&hint,		/* XA_WM_HINTS */
		&classhint	/* XA_WM_CLASSHINTS */
	);
	XFlush(xconn.display);
	return xw;
}

Rectangle
xmapwin(void *x, int havemin, Rectangle r)
{
	Xwin *xw;
	XWindowAttributes wattr;

	xw = x;

	if(havemin){
		XWindowChanges ch;

		memset(&ch, 0, sizeof ch);
		ch.x = r.min.x;
		ch.y = r.min.y;
		XConfigureWindow(xconn.display, xw->drawable, CWX|CWY, &ch);
		/*
		 * Must pretend origin is 0,0 for X.
		 */
		r = Rect(0,0,Dx(r),Dy(r));
	}
	/*
	 * TODO: Look up clipboard atom.
	 * plan9port/src/cmd/devdraw/x11-init.c:351
	 */

	/*
	 * Put the window on the screen, check to see what size we actually got.
	 */
	XMapWindow(xconn.display, xw->drawable);
	XSync(xconn.display, False);

	if(!XGetWindowAttributes(xconn.display, xw->drawable, &wattr))
		fprint(2, "XGetWindowAttributes failed\n");
	else if(wattr.width && wattr.height){
		if(wattr.width != Dx(r) || wattr.height != Dy(r)){
			r.max.x = wattr.width;
			r.max.y = wattr.height;
		}
	}else
		fprint(2, "XGetWindowAttributes: bad attrs\n");

	/*
	 * Allocate our local backing store.
	 */
	xw->screenpm = XCreatePixmap(xconn.display, xw->drawable, Dx(r), Dy(r), xconn.depth);
	xw->nextscreenpm = xw->screenpm;

	/*
	 * Allocate some useful graphics contexts for the future.
	 *TODO/
	_x.gcfill	= xgc(_x.screenpm, FillSolid, -1);
	_x.gccopy	= xgc(_x.screenpm, -1, -1);
	_x.gcsimplesrc 	= xgc(_x.screenpm, FillStippled, -1);
	_x.gczero	= xgc(_x.screenpm, -1, -1);
	_x.gcreplsrc	= xgc(_x.screenpm, FillTiled, -1);

	pmid = XCreatePixmap(_x.display, _x.drawable, 1, 1, 1);
	_x.gcfill0	= xgc(pmid, FillSolid, 0);
	_x.gccopy0	= xgc(pmid, -1, -1);
	_x.gcsimplesrc0	= xgc(pmid, FillStippled, -1);
	_x.gczero0	= xgc(pmid, -1, -1);
	_x.gcreplsrc0	= xgc(pmid, FillTiled, -1);
	XFreePixmap(_x.display, pmid);

	 */
	return r;
}

void
xdeletewin(Window *w)
{
	Xwin *xw;

	xw = w->x;
	// TODO anything else to cleanup?
	XDestroyWindow(xconn.display, xw->drawable);
	XSync(xconn.display, False);
}

int
xscreenpm(Window *w)
{
	return ((Xwin*)w->x)->screenpm;
}

int
xsetlabel(Window *w)
{
	char *label;
	XTextProperty name;
	Xwin *xw;

	xw = w->x;
	label = w->label;
	memset(&name, 0, sizeof name);
	if(label == nil)
		label = "pjw-face-here";
	name.value = (uchar*)label;
	name.encoding = XA_STRING;
	name.format = 8;
	name.nitems = strlen((char*)name.value);

	XSetWMProperties(
		xconn.display,			/* display */
		xw->drawable,	/* window */
		&name,	/* XA_WM_NAME property */
		&name,	/* XA_WM_ICON_NAME property */
		nil,		/* XA_WM_COMMAND */
		0,		/* argc */
		nil,		/* XA_WM_NORMAL_HINTS */
		nil,		/* XA_WM_HINTS */
		nil		/* XA_WM_CLASSHINTS */
	);
	XFlush(xconn.display);
	return 0;
}
