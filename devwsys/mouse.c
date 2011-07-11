#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"

void
writemouse(Window *w, Mouse m, int resized)
{
	char buf[50], c;

	/*
	 * TODO:
	 * If the last event in the queue is a resize,
	 * overwrite it instead of adding a new event.
	 */
	c = 'm';
	if(resized)
		c = 'r';
	sprint(buf, "%c%11d %11d %11d %11ld ", c, m.xy.x, m.xy.y, m.buttons, m.msec);
	w->mousebuttons = m.buttons;
	ixppwrite(w->mousep, buf);
}

static int
revbyte(int b)
{
	int r;

	r = 0;
	r |= (b&0x01) << 7;
	r |= (b&0x02) << 5;
	r |= (b&0x04) << 3;
	r |= (b&0x08) << 1;
	r |= (b&0x10) >> 1;
	r |= (b&0x20) >> 3;
	r |= (b&0x40) >> 5;
	r |= (b&0x80) >> 7;
	return r;
}

static
void
drawwincursor(Window *win, Drawcursor* c)
{
	uchar *bs, *bc, *ps, *pm;
	int i, j, w, h, bpl;

	if(c->data == nil){
		if(win->cursor.h != 0)
			win->cursor.h = 0;
		xsetcursor(win);
		return;
	}

	h = (c->maxy-c->miny)/2;	/* image, then mask */
	if(h > CursorSize)
		h = CursorSize;
	bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 1);
	w = bpl;
	if(w > CursorSize/8)
		w = CursorSize/8;

	ps = win->cursor.src;
	pm = win->cursor.mask;
	bc = c->data;
	bs = c->data + h*bpl;
	for(i = 0; i < h; i++){
		for(j = 0; j < bpl && j < w; j++) {
			*ps++ = revbyte(bs[j]);
			*pm++ = revbyte(bs[j] | bc[j]);
		}
		bs += bpl;
		bc += bpl;
	}
	win->cursor.h = h;
	win->cursor.w = w*8;
	win->cursor.hotx = c->hotx;
	win->cursor.hoty = c->hoty;
	xsetcursor(win);
}

int
cursorwrite(Window *w, char *buf, int n)
{
	uchar *p;
	Drawcursor c;

	p = buf;
	/* TO DO: perhaps interpret data as an Image */
	/*
	 *  hotx[4] hoty[4] dx[4] dy[4] clr[dx/8 * dy/2] set[dx/8 * dy/2]
	 *  dx must be a multiple of 8; dy must be a multiple of 2.
	 */
	if(n == 0){
		c.data = nil;
		drawwincursor(w, &c);
		return 0;
	}
	if(n < 8)
		return -1;
	c.hotx = BGLONG(p+0*4);
	c.hoty = BGLONG(p+1*4);
	c.minx = 0;
	c.miny = 0;
	c.maxx = BGLONG(p+2*4);
	c.maxy = BGLONG(p+3*4);
	if(c.maxx%8 != 0 || c.maxy%2 != 0 || n-4*4 != (c.maxx/8 * c.maxy))
		return -1;
	c.data = p + 4*4;
	drawwincursor(w, &c);
	return 0;
}
