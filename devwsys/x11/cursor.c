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
xsetcursor(Window *w)
{
	Cursor c;
	XCursor xc;
	XColor fg, bg;
	Pixmap xsrc, xmask;
	Xwin *xw;

	xw = w->x;
	if(w->cursor.h == 0){
		/* set the default system cursor */
		if(xw->cursor != 0) {
			XFreeCursor(xconn.display, xw->cursor);
			xw->cursor = 0;
		}
		XUndefineCursor(xconn.display, xw->drawable);
		XFlush(xconn.display);
		return;
	}
	c = w->cursor;

	xsrc = XCreateBitmapFromData(xconn.display, xw->drawable, (char*)c.src, c.w, c.h);
	xmask = XCreateBitmapFromData(xconn.display, xw->drawable, (char*)c.mask, c.w, c.h);

	fg = xconn.map[0];
	bg = xconn.map[255];
	fg.pixel = xconn.tox11[0];
	bg.pixel = xconn.tox11[255];
	xc = XCreatePixmapCursor(xconn.display, xsrc, xmask, &fg, &bg, -c.hotx, -c.hoty);
	if(xc != 0) {
		XDefineCursor(xconn.display, xw->drawable, xc);
		if(xw->cursor != 0)
			XFreeCursor(xconn.display, xw->cursor);
		xw->cursor = xc;
	}
	XFreePixmap(xconn.display, xsrc);
	XFreePixmap(xconn.display, xmask);
	XFlush(xconn.display);
}
