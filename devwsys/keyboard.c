#include <lib9.h>
#include <draw.h>
#include <memdraw.h>

#include "keyboard.h"
#include "mouse.h"
#include "devwsys.h"
#include "drawfcall.h"

void
addkbd(int i, Rune r)
{
	Kbdbuf *kbd;

	kbd = &window[i]->kbd;
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
matchkbd(int i)
{
	Kbdbuf *kbd;
	Tagbuf *kbdtags;
	Wsysmsg m;

	kbd = &window[i]->kbd;
	kbdtags = &window[i]->kbdtags;

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
		replymsg(window[i], &m);
	}
}
