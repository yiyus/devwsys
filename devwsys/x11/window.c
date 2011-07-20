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

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

#define Mask MouseMask|ExposureMask|StructureNotifyMask|KeyPressMask|EnterWindowMask|LeaveWindowMask|FocusChangeMask

static void* xcreatewin(char*, char*, Rectangle);
static Rectangle xmapwin(void*, int, Rectangle);
static Rectangle xwinrectangle(char*, char*, int*);

int
xattach(Window *w, char *winsize)
{
	int havemin;
	Xwin *xw;

	w->screenr = xwinrectangle(w->label, winsize, &havemin);
	// print("XXX xattach: w->screenr %d %d %d %d\n", w->screenr.min.x, w->screenr.min.y, w->screenr.max.x, w->screenr.max.y);
	xw = xcreatewin(w->label, winsize, w->screenr);
	if(xw == nil)
		goto Error;
	w->screenr = xmapwin(xw, havemin, w->screenr);
	w->x = xw;
	w->draw.screenimage = xallocmemimage(w, w->screenr, xconn.chan, xw->screenpm);
	// print("XXX xattach screenimage %d to window %d (draw %d)\n", w->draw.screenimage, w, &w->draw);
	if(w->draw.screenimage == nil)
		goto Error;
	return 1;

Error:
	/* BUG: detach screen */
	// print("XXX Error in xattach\n");
	return 0;
}

void
xdeletewin(Window *w)
{
	Xwin *xw;

	// print("XXX xdeletewin %d\n", w->id);
	// TODO anything else to cleanup?
	xw = w->x;
	if(!xw)
		return;

	XDestroyWindow(xconn.display, xw->drawable);
	if(xw->screenpm != xw->nextscreenpm)
		XFreePixmap(xconn.display, xw->screenpm);
	XFreePixmap(xconn.display, xw->nextscreenpm);
	free(xw);
	xw = (void*)nil;
	XSync(xconn.display, False);
}

int
xreplacescreenimage(Window *w)
{
	Memimage *m;
	XDrawable pixmap;
	Rectangle r;
	Xwin *xw;

	xw = w->x;
	r = w->newscreenr;

	pixmap = XCreatePixmap(xconn.display, xw->drawable, Dx(r), Dy(r), xconn.depth);
	m = xallocmemimage(w, r, xconn.chan, pixmap);
	if(xw->nextscreenpm != xw->screenpm)
		XFreePixmap(xconn.display, xw->nextscreenpm);
	xw->nextscreenpm = pixmap;
	w->screenr = r;
	drawreplacescreenimage(&w->draw, m);
	return 1;
}

int
xupdatelabel(Window *w)
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

static
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
	/*
	 * XXX TODO:
	 * The error returned by parsewinsize is ignored,
	 * there could be other options to mount we are
	 * not (but should be) processing.
	 */
	if(winsize && winsize[0])
		if(parsewinsize(winsize, &r, havemin) == 0)
			return r;

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
	return r;
}

static
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

	xw = mallocz(sizeof(Xwin), 1);
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

static
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

	return r;
}

void
xflushmemscreen(Window *w, Rectangle r)
{
	Xwin *xw;

	// print("XXX xflushmemscreen\n");
	xw = w->x;
	if(xw->nextscreenpm != xw->screenpm){
		// qlock(&xw->screenlock);
		XSync(xconn.display, False);
		XFreePixmap(xconn.display, xw->screenpm);
		xw->screenpm = xw->nextscreenpm;
		// qunlock(&xw->screenlock);
	}

	if(r.min.x >= r.max.x || r.min.y >= r.max.y)
		return;
	XCopyArea(xconn.display, xw->screenpm, xw->drawable, xconn.gccopy, r.min.x, r.min.y,
		Dx(r), Dy(r), r.min.x, r.min.y);
	XFlush(xconn.display);
}

