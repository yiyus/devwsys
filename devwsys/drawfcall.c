/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

void
runmsg(Window *w, Wsysmsg *m)
{
	Kbdbuf *kbd;
	Mousebuf *mouse;
	Reqbuf *kbdreqs, *mousereqs;
	int havemin;

	switch(m->type){
	case Tinit:
		if((w = newwin()) != nil) {
			w->r = xwinrectangle(m->label, m->winsize, &havemin);
			w->x = xcreatewin(m->label, m->winsize, w->r);
			w->r = xmapwin(w->x, havemin, w->r);
			w->screenimage = xallocmemimage(w, w->r, xchan(), xscreenpm(w));
			initscreenimage(w->screenimage);
		}
		replymsg(w, m);
		break;

	case Trdmouse:
		mouse = &w->mouse;
		mousereqs = &w->mousereqs;
		mousereqs->r[mousereqs->wi] = m->v;
		mousereqs->wi++;
		if(mousereqs->wi == nelem(mousereqs->r))
			mousereqs->wi = 0;
		if(mousereqs->wi == mousereqs->ri)
			sysfatal("too many queued mouse reads");
		// fprint(2, "mouse unstall\n");
		mouse->stall = 0;
		matchmouse(w);
		break;

	case Trdkbd:
		kbd = &w->kbd;
		kbdreqs = &w->kbdreqs;
		kbdreqs->r[kbdreqs->wi] = m->v;
		kbdreqs->wi++;
		if(kbdreqs->wi == nelem(kbdreqs->r))
			kbdreqs->wi = 0;
		if(kbdreqs->wi == kbdreqs->ri)
			sysfatal("too many queued kbd reads");
		// fprint(2, "kbd unstall\n");
		kbd->stall = 0;
		matchkbd(w);
		break;

	case Tlabel:
		w->label = strdup(m->label);
		if(!w->label)
			return;
		xsetlabel(w);
		replymsg(w, m);
		break;

/***
	case Tmoveto:
		_xmoveto(m->mouse.xy);
		replymsg(m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			_xsetcursor(nil);
		else
			_xsetcursor(&m->cursor);
		replymsg(m);
		break;
			
	case Tbouncemouse:
		_xbouncemouse(&m->mouse);
		replymsg(m);
		break;

	case Trdsnarf:
		m->snarf = _xgetsnarf();
		replymsg(m);
		free(m->snarf);
		break;

	case Twrsnarf:
		_xputsnarf(m->snarf);
		replymsg(m);
		break;

	case Trddraw:
		n = m->count;
		if(n > sizeof buf)
			n = sizeof buf;
		n = _drawmsgread(buf, n);
		if(n < 0)
			replyerror(m);
		else{
			m->count = n;
			m->data = buf;
			replymsg(m);
		}
		break;

	case Twrdraw:
		if(_drawmsgwrite(m->data, m->count) < 0)
			replyerror(m);
		else
			replymsg(m);
		break;
	
	case Ttop:
		_xtopwindow();
		replymsg(m);
		break;
	
	case Tresize:
		_xresizewindow(m->rect);
		replymsg(m);
		break;
***/
	}
}

void
replymsg(Window *w, Wsysmsg *m)
{
	/* T -> R msg */
	if(m->type%2 == 0)
		m->type++;
		
	ixpreply(w, m);
}
