#include <ctype.h>
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

/* >= Top are disallowed if mouse button is pressed */
enum
{
	New,
	Resize,
	Move,
	Scroll,
	Noscroll,
	Set,
	Top,
	Bottom,
	Current,
	Hide,
	Unhide,
	Delete,
};

static char *cmds[] = {
	[New]	= "new",
	[Resize]	= "resize",
	[Move]	= "move",
	[Scroll]	= "scroll",
	[Noscroll]	= "noscroll",
	[Set]		= "set",
	[Top]	= "top",
	[Bottom]	= "bottom",
	[Current]	= "current",
	[Hide]	= "hide",
	[Unhide]	= "unhide",
	[Delete]	= "delete",
	nil
};

enum
{
	Cd,
	Deltax,
	Deltay,
	Hidden,
	Id,
	Maxx,
	Maxy,
	Minx,
	Miny,
	PID,
	R,
	Scrolling,
	Noscrolling,
};

static char *params[] = {
	[Cd]	 			= "-cd",
	[Deltax]			= "-dx",
	[Deltay]			= "-dy",
	[Hidden]			= "-hide",
	[Id]				= "-id",
	[Maxx]			= "-maxx",
	[Maxy]			= "-maxy",
	[Minx]			= "-minx",
	[Miny]			= "-miny",
	[PID]				= "-pid",
	[R]				= "-r",
	[Scrolling]			= "-scroll",
	[Noscrolling]		= "-noscroll",
	nil
};

static
int
word(char **sp, char *tab[])
{
	char *s, *t;
	int i;

	s = *sp;
	while(isspace(*s))
		s++;
	t = s;
	while(*s!='\0' && !isspace(*s))
		s++;
	for(i=0; tab[i]!=nil; i++)
		if(strncmp(tab[i], t, strlen(tab[i])) == 0){
			*sp = s;
			return i;
	}
	return -1;
}

static
int
set(int sign, int neg, int abs, int pos)
{
	if(sign < 0)
		return neg;
	if(sign > 0)
		return pos;
	return abs;
}

/* permit square brackets, in the manner of %R */
static
int
riostrtol(char *s, char **t)
{
	int n;

	while(*s!='\0' && (*s==' ' || *s=='\t' || *s=='['))
		s++;
	if(*s == '[')
		s++;
	n = strtol(s, t, 10);
	if(*t != s)
		while((*t)[0] == ']')
			(*t)++;
	return n;
}

static
int
parsewctl(char **argp, Rectangle r, Rectangle *rp, int *pidp, int *idp, int *hiddenp, int *scrollingp, char **cdp, char *s, char *err)
{
	int cmd, param, xy, sign;
	char *t;

	/*
	 * Scrolling defaults to the option given in rio,
	 * we ignore it, so it is set to 0.
	 * New commands are ignored too.
	 */
	*pidp = 0;
	*hiddenp = 0;
	*scrollingp = 0;
	*cdp = nil;
	cmd = word(&s, cmds);
	if(cmd < 0){
		strcpy(err, "unrecognized wctl command");
		return -1;
	}
	if(cmd == New)
		return 0;

	strcpy(err, "missing or bad wctl parameter");
	while((param = word(&s, params)) >= 0){
		switch(param){	/* special cases */
		case Hidden:
			*hiddenp = 1;
			continue;
		case Scrolling:
			*scrollingp = 1;
			continue;
		case Noscrolling:
			*scrollingp = 0;
			continue;
		case R:
			r.min.x = riostrtol(s, &t);
			if(t == s)
				return -1;
			s = t;
			r.min.y = riostrtol(s, &t);
			if(t == s)
				return -1;
			s = t;
			r.max.x = riostrtol(s, &t);
			if(t == s)
				return -1;
			s = t;
			r.max.y = riostrtol(s, &t);
			if(t == s)
				return -1;
			s = t;
			continue;
		}
		while(isspace(*s))
			s++;
		if(param == Cd){
			*cdp = s;
			while(*s && !isspace(*s))
				s++;
			if(*s != '\0')
				*s++ = '\0';
			continue;
		}
		sign = 0;
		if(*s == '-'){
			sign = -1;
			s++;
		}else if(*s == '+'){
			sign = +1;
			s++;
		}
		if(!isdigit(*s))
			return -1;
		xy = riostrtol(s, &s);
		switch(param){
		case -1:
			strcpy(err, "unrecognized wctl parameter");
			return -1;
		case Minx:
			r.min.x = set(sign, r.min.x-xy, xy, r.min.x+xy);
			break;
		case Miny:
			r.min.y = set(sign, r.min.y-xy, xy, r.min.y+xy);
			break;
		case Maxx:
			r.max.x = set(sign, r.max.x-xy, xy, r.max.x+xy);
			break;
		case Maxy:
			r.max.y = set(sign, r.max.y-xy, xy, r.max.y+xy);
			break;
		case Deltax:
			r.max.x = set(sign, r.max.x-xy, r.min.x+xy, r.max.x+xy);
			break;
		case Deltay:
			r.max.y = set(sign, r.max.y-xy, r.min.y+xy, r.max.y+xy);
			break;
		case Id:
			if(idp != nil)
				*idp = xy;
			break;
		case PID:
			if(pidp != nil)
				*pidp = xy;
			break;
		}
	}

	*rp = r;

	while(isspace(*s))
		s++;
	if(cmd!=New && *s!='\0'){
		strcpy(err, "extraneous text in wctl message");
		return -1;
	}

	if(argp)
		*argp = s;

	return cmd;
}

int
wctlmesg(Window *w, char *a, int n, char *err)
{
	int buttons, cnt, cmd, j, id, hideit, scrollit, pid;
	char *arg, *dir;
	Rectangle rect;

	cnt = n;
	a[cnt] = '\0';
	id = 0;
	buttons = w->mouse.m[w->mouse.ri].buttons;

// print(" XXX wctlmesg: a = %s\n", a);
	rect = rectaddpt(rect, w->orig);
// print("XXX original rect %d %d %d %d\n", rect.min.x, rect.min.y, rect.max.x, rect.max.y);
	cmd = parsewctl(&arg, rect, &rect, &pid, &id, &hideit, &scrollit, &dir, a, err);
	if(cmd < 0)
		return -1;

	if(buttons!=0 && cmd>=Top){
		strcpy(err, "action disallowed when mouse active");
		return -1;
	}

	if(id != 0){
		for(j=0; j<nwindow; j++)
			if(window[j]->id == id)
				break;
		if(j == nwindow){
			strcpy(err, "no such window id");
			return -1;
		}
		w = window[j];
		if(w->deleted){
			strcpy(err, "window deleted");
			return -1;
		}
	}

	switch(cmd){
	case New:
		// return wctlnew(rect, arg, pid, hideit, scrollit, dir, err);
		return 1;
	case Set:
		// if(pid > 0)
		//	wsetpid(w, pid, 0);
		return 1;
	case Move:
		rect = Rect(rect.min.x, rect.min.y, rect.min.x+Dx(w->screenr), rect.min.y+Dy(w->screenr));
		/* fall through */
	case Resize:
		/*** TODO
		if(!goodrect(rect)){
			strcpy(err, Ebadwr);
			return -1;
		}
		***/
		if(eqrect(rect, rectaddpt(w->screenr, w->orig)))
			return 1;
// print("XXX resizing to %d %d %d %d\n", rect.min.x, rect.min.y, rect.max.x, rect.max.y);
		xresizewindow(w, rect);
		return 1;
	case Scroll:
		// w->scrolling = 1;
		// wshow(w, w->nr);
		// wsendctlmesg(w, Wakeup, ZR, nil);
		return 1;
	case Noscroll:
		// w->scrolling = 0;
		// wsendctlmesg(w, Wakeup, ZR, nil);
		return 1;
	case Top:
		xtopwindow(w);
		return 1;
	case Bottom:
		// wbottomme(w);
		return 1;
	case Current:
		// wcurrent(w);
		xtopwindow(w);
		return 1;
	case Hide:
		/***
		switch(whide(w)){
		case -1:
			strcpy(err, "window already hidden");
			return -1;
		case 0:
			strcpy(err, "hide failed");
			return -1;
		default:
			break;
		}
		***/
		return 1;
	case Unhide:
		/***
		for(j=0; j<nhidden; j++)
			if(hidden[j] == w)
				break;
		if(j == nhidden){
			strcpy(err, "window not hidden");
			return -1;
		}
		if(wunhide(j) == 0){
			strcpy(err, "hide failed");
			return -1;
		}
		***/
		return 1;
	case Delete:
		deletewin(w);
		return 1;
	}
	strcpy(err, "invalid wctl message");
	return -1;
}
