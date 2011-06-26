#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

/*
 * Allocate a Memimage with an optional pixmap backing on the X server.
 */
Memimage*
xallocmemimage(Window *w, Rectangle r, ulong chan, int pixmap)
{
	int d, offset;
	Memimage *m;
	XImage *xi;
	Xmem *xm;
	Xwin *xw;

	m = allocmemimage(r, chan);
	if(chan != GREY1 && chan != xconn.chan)
		return m;
	if(xconn.display == 0)
		return m;

	/*
	 * For bootstrapping, don't bother storing 1x1 images
	 * on the X server.  Memimageinit needs to allocate these
	 * and we memimageinit before we do the rest of the X stuff.
	 * Of course, 1x1 images on the server are useless anyway.
	 */
	if(Dx(r)==1 && Dy(r)==1)
		return m;

	xm = mallocz(sizeof(Xmem), 1);
	if(xm == nil){
		xfreememimage(m);
		return nil;
	}

	/*
	 * Allocate backing store.
	 */
	if(chan == GREY1)
		d = 1;
	else
		d = xconn.depth;
	if(pixmap != PMundef)
		xm->pixmap = pixmap;
	else {
		xw = w->x;
		xm->pixmap = XCreatePixmap(xconn.display, xw->drawable, Dx(r), Dy(r), d);
	}

	/*
	 * We want to align pixels on word boundaries.
	 */
	if(m->depth == 24)
		offset = r.min.x&3;
	else
		offset = r.min.x&(31/m->depth);
	r.min.x -= offset;
	assert(wordsperline(r, m->depth) <= m->width);

	/*
	 * Wrap our data in an XImage structure.
	 */
	xi = XCreateImage(xconn.display, xconn.vis, d,
		ZPixmap, 0, (char*)m->data->bdata, Dx(r), Dy(r),
		32, m->width*sizeof(u32int));
	if(xi == nil){
		xfreememimage(m);
		if(xm->pixmap != pixmap)
			XFreePixmap(xconn.display, xm->pixmap);
		return nil;
	}

	xm->xi = xi;
	xm->r = r;

	/*
	 * Set the XImage parameters so that it looks exactly like
	 * a Memimage -- we're using the same data.
	 */
	if(m->depth < 8 || m->depth == 24)
		xi->bitmap_unit = 8;
	else
		xi->bitmap_unit = m->depth;
	xi->byte_order = LSBFirst;
	xi->bitmap_bit_order = MSBFirst;
	xi->bitmap_pad = 32;
	XInitImage(xi);
	XFlush(xconn.display);

	// XXX! TODO m->X = xm;
	return m;
}

void
xfreememimage(Memimage *m)
{
	Xmem *xm;

	if(m == nil)
		return;

	xm = nil; // m->X;
	if(xm && m->data->ref == 1){
		if(xm->xi){
			xm->xi->data = nil;
			XFree(xm->xi);
		}
		XFreePixmap(xconn.display, xm->pixmap);
		free(xm);
		// m->X = nil;
	}
	freememimage(m);
}
