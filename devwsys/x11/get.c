#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

XImage*
xgetxdata(Memimage *m, Rectangle r)
{
	int x, y;
	uchar *p;
	Point tp, xdelta, delta;
	Xmem *xm;
	
	xm = m->X;
	if(xm == nil)
		return nil;

	if(xm->dirty == 0)
		return xm->xi;

	abort();	/* should never call this now */

	r = xm->dirtyr;
	if(Dx(r)==0 || Dy(r)==0)
		return xm->xi;

	delta = subpt(r.min, m->r.min);

	tp = xm->r.min;	/* need temp for Digital UNIX */
	xdelta = subpt(r.min, tp);

	XGetSubImage(xconn.display, xm->pixmap, delta.x, delta.y, Dx(r), Dy(r),
		AllPlanes, ZPixmap, xm->xi, xdelta.x, delta.y);

	if(xconn.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = xconn.toplan9[*p];
	}
	xm->dirty = 0;
	xm->dirtyr = Rect(0,0,0,0);
	return xm->xi;
}

void
xputxdata(Memimage *m, Rectangle r)
{
	int offset, x, y;
	uchar *p;
	Point tp, xdelta, delta;
	Xmem *xm;
	XGC gc;
	XImage *xi;
	Xwin *xw;

	xm = m->X;
	if(xm == nil)
		return;

	xw = xm->w->x;
	xi = xm->xi;
	gc = m->chan==GREY1 ? xw->gccopy0 : xw->gccopy;
	if(m->depth == 24)
		offset = r.min.x & 3;
	else
		offset = r.min.x & (31/m->depth);

	delta = subpt(r.min, m->r.min);

	tp = xm->r.min;	/* need temporary on Digital UNIX */
	xdelta = subpt(r.min, tp);

	if(xconn.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = xconn.tox11[*p];
	}

	XPutImage(xconn.display, xm->pixmap, gc, xi, xdelta.x, xdelta.y, delta.x, delta.y,
		Dx(r), Dy(r));
	
	if(xconn.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = xconn.toplan9[*p];
	}
}

/*
static void
addrect(Rectangle *rp, Rectangle r)
{
	if(rp->min.x >= rp->max.x)
		*rp = r;
	else
		combinerect(rp, r);
}

void
_xdirtyxdata(Memimage *m, Rectangle r)
{
	Xmem *xm;

	xm = m->X;
	if(xm == nil)
		return;

	xm->dirty = 1;
	addrect(&xm->dirtyr, r);
}
*/


