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
xfillcolor(Memimage *m, Rectangle r, ulong v)
{
	Point p;
	Xmem *xm;
	XGC gc;
	Xwin *xw;
	
	xm = m->X;
	assert(xm != nil);
	xw = xm->w->x;

	/*
	 * Set up fill context appropriately.
	 */
	if(m->chan == GREY1){
		gc = xconn.gcfill0;
		if(xconn.gcfill0color != v){
			XSetForeground(xconn.display, gc, v);
			xconn.gcfill0color = v;
		}
	}else{
		if(m->chan == CMAP8 && xconn.usetable)
			v = xconn.tox11[v];
		gc = xconn.gcfill;
		if(xconn.gcfillcolor != v){
			XSetForeground(xconn.display, gc, v);
			xconn.gcfillcolor = v;
		}
	}

	/*
	 * XFillRectangle takes coordinates relative to image rectangle.
	 */
	p = subpt(r.min, m->r.min);
	XFillRectangle(xconn.display, xm->pixmap, gc, p.x, p.y, Dx(r), Dy(r));
}

void
xmemfillcolor(Memimage *m, ulong val)
{
	_memfillcolor(m, val);
	if(m->X == nil)
		return;
	if((val & 0xFF) == 0xFF)	/* full alpha */
		xfillcolor(m, m->r, rgbatoimg(m, val));
	else
		xputxdata(m, m->r);
}
