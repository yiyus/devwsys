#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

/* for copy/paste, lifted from plan9ports via drawterm */
static int putsnarf, assertsnarf;
static Atom clipboard; 
static Atom utf8string;
static Atom targets;
static Atom text;
static Atom compoundtext;

long keysym2ucs(KeySym);
int
xtoplan9kbd(XEvent *e)
{
	KeySym k;

	if(e->xany.type != KeyPress)
		return -1;
	// needstack(64*1024);	/* X has some *huge* buffers in openobject */
		/* and they're even bigger on SuSE */
	XLookupString((XKeyEvent*)e,NULL,0,&k,NULL);
	if(k == k == NoSymbol)
		return -1;

	if(k&0xFF00){
		switch(k){
		case XK_BackSpace:
		case XK_Tab:
		case XK_Escape:
		case XK_Delete:
		case XK_KP_0:
		case XK_KP_1:
		case XK_KP_2:
		case XK_KP_3:
		case XK_KP_4:
		case XK_KP_5:
		case XK_KP_6:
		case XK_KP_7:
		case XK_KP_8:
		case XK_KP_9:
		case XK_KP_Divide:
		case XK_KP_Multiply:
		case XK_KP_Subtract:
		case XK_KP_Add:
		case XK_KP_Decimal:
			k &= 0x7F;
			break;
		case XK_Linefeed:
			k = '\r';
			break;
		case XK_KP_Space:
			k = ' ';
			break;
		case XK_Home:
		case XK_KP_Home:
			k = Khome;
			break;
		case XK_Left:
		case XK_KP_Left:
			k = Kleft;
			break;
		case XK_Up:
		case XK_KP_Up:
			k = Kup;
			break;
		case XK_Down:
		case XK_KP_Down:
			k = Kdown;
			break;
		case XK_Right:
		case XK_KP_Right:
			k = Kright;
			break;
		case XK_Page_Down:
		case XK_KP_Page_Down:
			k = Kpgdown;
			break;
		case XK_End:
		case XK_KP_End:
			k = Kend;
			break;
		case XK_Page_Up:	
		case XK_KP_Page_Up:
			k = Kpgup;
			break;
		case XK_Insert:
		case XK_KP_Insert:
			k = Kins;
			break;
		case XK_KP_Enter:
		case XK_Return:
			k = '\n';
			break;
		case XK_Alt_L:
		case XK_Meta_L:	/* Shift Alt on PCs */
		case XK_Alt_R:
		case XK_Meta_R:	/* Shift Alt on PCs */
		case XK_Multi_key:
			k = Kalt;
			break;
		default:		/* not ISO-1 or tty control */
			if(k>0xff) {
				k = keysym2ucs(k);
				if(k==-1) return -1;
			}
		}
	}

	/* Compensate for servers that call a minus a hyphen */
	if(k == XK_hyphen)
		k = XK_minus;
	/* Do control mapping ourselves if translator doesn't */
	if(e->xkey.state&ControlMask && k != Kalt)
		k &= 0x9f;
	if(k == NoSymbol) {
		return -1;
	}

	return k+0;
}

int
xtoplan9mouse(XEvent *e, Mouse *m)
{
	int s;
	XButtonEvent *be;
	XMotionEvent *me;

	switch(e->type){
	case ButtonPress:
		be = (XButtonEvent*)e;
		/* 
		 * Fake message, just sent to make us announce snarf.
		 * Apparently state and button are 16 and 8 bits on
		 * the wire, since they are truncated by the time they
		 * get to us.
		 */
		if(be->send_event
		&& (~be->state&0xFFFF)==0
		&& (~be->button&0xFF)==0)
			return -1;
		/* BUG? on mac need to inherit these from elsewhere? */
		m->xy.x = be->x;
		m->xy.y = be->y;
		s = be->state;
		m->msec = be->time;
		switch(be->button){
		case 1:
			s |= Button1Mask;
			break;
		case 2:
			s |= Button2Mask;
			break;
		case 3:
			s |= Button3Mask;
			break;
		case 4:
			s |= Button4Mask;
			break;
		case 5:
			s |= Button5Mask;
			break;
		}
		break;
	case ButtonRelease:
		be = (XButtonEvent*)e;
		m->xy.x = be->x;
		m->xy.y = be->y;
		s = be->state;
		m->msec = be->time;
		switch(be->button){
		case 1:
			s &= ~Button1Mask;
			break;
		case 2:
			s &= ~Button2Mask;
			break;
		case 3:
			s &= ~Button3Mask;
			break;
		case 4:
			s &= ~Button4Mask;
			break;
		case 5:
			s &= ~Button5Mask;
			break;
		}
		break;

	case MotionNotify:
		me = (XMotionEvent*)e;
		s = me->state;
		m->xy.x = me->x;
		m->xy.y = me->y;
		m->msec = me->time;
		break;

	default:
		return -1;
	}

	m->buttons = 0;
	if(s & Button1Mask)
		m->buttons |= 1;
	if(s & Button2Mask)
		m->buttons |= 2;
	if(s & Button3Mask)
		m->buttons |= 4;
	if(s & Button4Mask)
		m->buttons |= 8;
	if(s & Button5Mask)
		m->buttons |= 16;
	return 0;
}

/*
 * Cut and paste.  Just couldn't stand to make this simple...
 */

void
xinitclipboard(void){
	clipboard = XInternAtom(xconn.display, "CLIPBOARD", False);
	utf8string = XInternAtom(xconn.display, "UTF8_STRING", False);
	targets = XInternAtom(xconn.display, "TARGETS", False);
	text = XInternAtom(xconn.display, "TEXT", False);
	compoundtext = XInternAtom(xconn.display, "COMPOUND_TEXT", False);
}

static
void
xsetsnarfowner(void)
{
	if(putsnarf != assertsnarf){
		assertsnarf = putsnarf;
		XSetSelectionOwner(xconn.display, XA_PRIMARY, xconn.w, CurrentTime);
		if(clipboard != None)
			XSetSelectionOwner(xconn.display, clipboard, xconn.w, CurrentTime);
		XFlush(xconn.display);
	}
}

char*
xgetsnarf(void)
{
	uchar *data, *xdata;
	Atom cb, type, prop;
	unsigned long len, lastlen, dummy;
	int fmt, i;
	XWindow w;

	/*
	 * Have we snarfed recently and the X server hasn't caught up?
	 */
	if(putsnarf != assertsnarf)
		goto mine;

	/*
	 * Is there a primary selection (highlighted text in an xterm)?
	 */
	cb = XA_PRIMARY;
	w = XGetSelectionOwner(xconn.display, XA_PRIMARY);
	if(w == xconn.w){
	mine:
		data = (uchar*)strdup(clip.buf);
		goto out;
	}

	/*
	 * If not, is there a clipboard selection?
	 */
	if(w == None && clipboard != None){
		cb = clipboard;
		w = XGetSelectionOwner(xconn.display, clipboard);
		if(w == xconn.w)
			goto mine;
	}

	/*
	 * If not, give up.
	 */
	if(w == None){
		data = nil;
		goto out;
	}
		
	/*
	 * We should be waiting for SelectionNotify here, but it might never
	 * come, and we have no way to time out.  Instead, we will clear
	 * local property #1, request our buddy to fill it in for us, and poll
	 * until he's done or we get tired of waiting.
	 *
	 * We should try to go for utf8string instead of XA_STRING,
	 * but that would add to the polling.
	 */
	prop = 1;
	XChangeProperty(xconn.display, xconn.w, prop, XA_STRING, 8, PropModeReplace, (uchar*)"", 0);
	XConvertSelection(xconn.display, cb, XA_STRING, prop, xconn.w, CurrentTime);
	XFlush(xconn.display);
	lastlen = 0;
	for(i=0; i<10 || (lastlen!=0 && i<30); i++){
		usleep(10*1000);
		XGetWindowProperty(xconn.display, xconn.w, prop, 0, 0, 0, AnyPropertyType,
			&type, &fmt, &dummy, &len, &data);
		if(lastlen == len && len > 0)
			break;
		lastlen = len;
	}
	if(i == 10){
		data = nil;
		goto out;
	}
	/* get the property */
	data = nil;
	XGetWindowProperty(xconn.display, xconn.w, prop, 0, SnarfSize/sizeof(unsigned long), 0, 
		AnyPropertyType, &type, &fmt, &len, &dummy, &xdata);
	if((type != XA_STRING && type != utf8string) || len == 0){
		if(xdata)
			XFree(xdata);
		data = nil;
	}else{
		if(xdata){
			data = (uchar*)strdup((char*)xdata);
			XFree(xdata);
		}else
			data = nil;
	}
out:
	return (char*)data;
}

void
xputsnarf(Window *window, char *data)
{
	if(strlen(data) >= SnarfSize)
		return;
	strcpy(clip.buf, data);

	/* leave note for mouse proc to assert selection ownership */
	putsnarf++;

	xsetsnarfowner();
}

void
xselect(XEvent *e)
{
	char *name;
	XEvent r;
	XSelectionRequestEvent *xe;
	Atom a[4];
	XDisplay *xd;

	if(e->xany.type != SelectionRequest)
		return;

	xd  = xconn.display;

	memset(&r, 0, sizeof r);
	xe = (XSelectionRequestEvent*)e;
if(0) iprint("xselect target=%d requestor=%d property=%d selection=%d\n",
	xe->target, xe->requestor, xe->property, xe->selection);
	r.xselection.property = xe->property;
	if(xe->target == targets){
		a[0] = XA_STRING;
		a[1] = utf8string;
		a[2] = text;
		a[3] = compoundtext;

		XChangeProperty(xd, xe->requestor, xe->property, xe->target,
			8, PropModeReplace, (uchar*)a, sizeof a);
	}else if(xe->target == XA_STRING || xe->target == utf8string || xe->target == text || xe->target == compoundtext){
		/* if the target is STRING we're supposed to reply with Latin1 XXX */
		XChangeProperty(xd, xe->requestor, xe->property, xe->target,
			8, PropModeReplace, (uchar*)clip.buf, strlen(clip.buf));
	}else{
		iprint("get %d\n", xe->target);
		name = XGetAtomName(xd, xe->target);
		if(name == nil)
			iprint("XGetAtomName failed\n");
		else if(strcmp(name, "TIMESTAMP") != 0)
			iprint("%s: cannot handle selection request for '%s' (%d)\n", argv0, name, (int)xe->target);
		r.xselection.property = None;
	}

	r.xselection.display = xe->display;
	/* r.xselection.property filled above */
	r.xselection.target = xe->target;
	r.xselection.type = SelectionNotify;
	r.xselection.requestor = xe->requestor;
	r.xselection.time = xe->time;
	r.xselection.send_event = True;
	r.xselection.selection = xe->selection;
	XSendEvent(xd, xe->requestor, False, 0, &r);
	XFlush(xd);
}

