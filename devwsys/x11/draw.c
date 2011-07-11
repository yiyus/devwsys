#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

enum {
	Fullsrc=1<<5,
};

static int xdraw(Memdrawparam*);

Memdrawparam par;

Memdrawparam*
memimagedrawsetup(Memimage *dst, Rectangle r, Memimage *src, Point p0, Memimage *mask, Point p1, int op)
{
	if(mask == nil)
		mask = memopaque;

	if(drawclip(dst, &r, src, &p0, mask, &p1, &par.sr, &par.mr) == 0)
		return nil;

	if(op < Clear || op > SoverD)
		return nil;

	par.op = op;
	par.dst = dst;
	par.r = r;
	par.src = src;
	/* par.sr set by drawclip */
	par.mask = mask;
	/* par.mr set by drawclip */

	par.state = 0;
	if(src->flags&Frepl){
		par.state |= Replsrc;
		if(Dx(src->r)==1 && Dy(src->r)==1){
			par.sval = pixelbits(src, src->r.min);
			par.state |= Simplesrc;
			par.srgba = imgtorgba(src, par.sval);
			par.sdval = rgbatoimg(dst, par.srgba);
			if((par.srgba&0xFF) == 0 && (op&DoutS))
				return nil;	/* no-op successfully handled */
			if((par.srgba&0xFF) == 0xFF)
				par.state |= Fullsrc;
		}
	}

	if(mask->flags & Frepl){
		par.state |= Replmask;
		if(Dx(mask->r)==1 && Dy(mask->r)==1){
			par.mval = pixelbits(mask, mask->r.min);
			if(par.mval == 0 && (op&DoutS))
				return nil;	/* no-op successfully handled */
			par.state |= Simplemask;
			if(par.mval == ~0)
				par.state |= Fullmask;
			par.mrgba = imgtorgba(mask, par.mval);
		}
	}
	return &par;
}

/*
 * The X acceleration doesn't fit into the standard hwaccel
 * model because we have the extra steps of pulling the image
 * data off the server and putting it back when we're done.
 */
void
xmemimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
	Memimage *mask, Point mp, int op)
{
	Memdrawparam *par;

	if((par = memimagedrawsetup(dst, r, src, sp, mask, mp, op)) == nil)
		return;

	/* only fetch dst data if we need it */
	if((par->state&(Simplemask|Fullmask)) != (Simplemask|Fullmask))
		xgetxdata(par->dst, par->r);

	/* always fetch source and mask */
	xgetxdata(par->src, par->sr);
	xgetxdata(par->mask, par->mr);

	/* now can run memimagedraw on the in-memory bits */
	_memimagedraw(dst, r, src, sp, mask, mp, op);

	if(xdraw(par))
		return;

	/* put bits back on x server */
	xputxdata(par->dst, par->r);
}

static int
xdraw(Memdrawparam *par)
{
	ulong sdval;
	uint m, state;
	Memimage *src, *dst, *mask;
	Point dp, mp, sp;
	Rectangle r;
	Xmem *xdst, *xmask, *xsrc;
	XGC gc;
	Xwin *xw;

	if(par->dst->X == nil)
		return 0;

	dst   = par->dst;
	mask  = par->mask;
	r     = par->r;
	src   = par->src;
	state = par->state;

	/*
	 * If we have an opaque mask and source is one opaque pixel,
	 * we can convert to the destination format and just XFillRectangle.
	 */
	m = Simplesrc|Fullsrc|Simplemask|Fullmask;
	if((state&m) == m){
		xfillcolor(dst, r, par->sdval);
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * If no source alpha and an opaque mask, we can just copy
	 * the source onto the destination.  If the channels are the
	 * same and the source is not replicated, XCopyArea works.
	 */
	m = Simplemask|Fullmask;
	if((state&(m|Replsrc))==m && src->chan==dst->chan && src->X){
		xdst = dst->X;
		xw =xdst->w->x;
		xsrc = src->X;
		dp = subpt(r.min,       dst->r.min);
		sp = subpt(par->sr.min, src->r.min);
		gc = dst->chan==GREY1 ?  xw->gccopy0 : xw->gccopy;

		XCopyArea(xconn.display, xsrc->pixmap, xdst->pixmap, gc,
			sp.x, sp.y, Dx(r), Dy(r), dp.x, dp.y);
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * If no source alpha, a 1-bit mask, and a simple source,
	 * we can copy through the mask onto the destination.
	 */
	if(dst->X && mask->X && !(mask->flags&Frepl)
	&& mask->chan==GREY1 && (state&Simplesrc)){
		xdst = dst->X;
		xw =xdst->w->x;
		xmask = mask->X;
		sdval = par->sdval;

		dp = subpt(r.min, dst->r.min);
		mp = subpt(r.min, subpt(par->mr.min, mask->r.min));

		if(dst->chan == GREY1){
			gc = xw->gcsimplesrc0;
			if(xw->gcsimplesrc0color != sdval){
				XSetForeground(xconn.display, gc, sdval);
				xw->gcsimplesrc0color = sdval;
			}
			if(xw->gcsimplesrc0pixmap != xmask->pixmap){
				XSetStipple(xconn.display, gc, xmask->pixmap);
				xw->gcsimplesrc0pixmap = xmask->pixmap;
			}
		}else{
			/* this doesn't work on rob's mac?  */
			return 0;
			/* gc = xw->gcsimplesrc;
			if(dst->chan == CMAP8 && xconn.usetable)
				sdval = xw->tox11[sdval];

			if(xw->gcsimplesrccolor != sdval){
				XSetForeground(xconn.display, gc, sdval);
				xw->gcsimplesrccolor = sdval;
			}
			if(xw->gcsimplesrcpixmap != xmask->pixmap){
				XSetStipple(xconn.display, gc, xmask->pixmap);
				xw->gcsimplesrcpixmap = xmask->pixmap;
			}
			*/
		}
		XSetTSOrigin(xconn.display, gc, mp.x, mp.y);
		XFillRectangle(xconn.display, xdst->pixmap, gc, dp.x, dp.y,
			Dx(r), Dy(r));
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * Can't accelerate.
	 */
	return 0;
}

