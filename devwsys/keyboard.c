#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include "dat.h"
#include "fns.h"

void
addkbd(Window *w, Rune r)
{
	Kbdbuf *kbd;

	kbd = &w->kbd;
	if(kbd->stall)
		return;

	kbd->r[kbd->wi++] = r;
	if(kbd->wi == nelem(kbd->r))
		kbd->wi = 0;
	if(kbd->ri == kbd->wi)
		kbd->stall = 1;
}

/*
 * Match queued keyboard reads with queued keyboard events.
 */
void
matchkbd(Window *w)
{
	Kbdbuf *kbd;
	Tagbuf *kbdtags;
	Wsysmsg m;

	kbd = &w->kbd;
	kbdtags = &w->kbdtags;

	while(kbd->ri != kbd->wi && kbdtags->ri != kbdtags->wi){
		m.type = Rrdkbd;
		m.tag = kbdtags->t[kbdtags->ri];
		m.v = kbdtags->r[kbdtags->ri];
		kbdtags->ri++;
		if(kbdtags->ri == nelem(kbdtags->t))
			kbdtags->ri = 0;
		m.rune = kbd->r[kbd->ri];
		kbd->ri++;
		if(kbd->ri == nelem(kbd->r))
			kbd->ri = 0;
		replymsg(w, &m);
	}
}

int
kbdputc(Kbdbuf *kbd, int ch)
{
	int n;
	Rune r;

	if(ch < 0) {
		kbd->alting = 0;
		return -1;
	}
	r = ch;
	if(kbd->alting){
		/*
		 * Kludge for Mac's X11 3-button emulation.
		 * It treats Command+Button as button 3, but also
		 * ends up sending XK_Meta_L twice.
		 */
		if(ch == Kalt){
			kbd->alting = 0;
			return -1;
		}
		kbd->nk += runetochar((char*)&kbd->k[kbd->nk], &r);
		n = latin1(kbd->k, kbd->nk);
		if(n > 0){
			kbd->alting = 0;
			r = (Rune)n;
			return r;
		}
		if(n == -1){
			kbd->alting = 0;
			kbd->k[kbd->nk] = 0;
			r = (Rune)0;
			return r;
		}
		/* n < -1, need more input */
		return -1;
	}else if(ch == Kalt){
		kbd->alting = 1;
		kbd->nk = 0;
		return -1;
	}
	return r;
}

