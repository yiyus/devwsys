/*
 * Window system protocol server.
 * Use thread(3) and 9pclient(3) to connect
 * to a wsys service as described in rio(4).
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <9pclient.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>

#define STACK (1024)

void runmsg(Wsysmsg*);
void replymsg(Wsysmsg*);
void kbdthread(void*);
void mousethread(void*);

int chatty = 0;
int drawsleep;
int wsysfd;

Channel *kbdchan;
Channel *mousechan;

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	threadexitsall(0);
}

void
bell(void *v, char *msg)
{
	if(strcmp(msg, "alarm") == 0)
		drawsleep = drawsleep ? 0 : 1000;
	noted(NCONT);
}

void
threadmain(int argc, char **argv)
{
	char *defwsys, *wsysaddr, *ns;
	uchar buf[4], *mbuf;
	int nmbuf, n, nn;
	Ioproc *io;
	Wsysmsg m;

	/*
	 * Move the protocol off stdin/stdout so that
	 * any inadvertent prints don't screw things up.
	 */
	dup(0, 3);
	dup(1, 4);
	close(0);
	close(1);
	open("/dev/null", OREAD);
	open("/dev/null", OWRITE);

	fmtinstall('H', encodefmt);
	fmtinstall('W', drawfcallfmt);

	ARGBEGIN{
	case 'd':
		chatty9pclient++;
		break;
	case 'D':
		chatty++;
		break;
	default:
		usage();
	}ARGEND

	/*
	 * Ignore arguments.  They're only for good ps -a listings.
	 */
	
	notify(bell);

	/*
	 * Connect to 9P server defined by $WSYS
	 * or unix!$NAMESPACE/wsys.
	 */
	wsysaddr = getenv("WSYS");
	defwsys = nil;
	if(wsysaddr == 0){
		ns = getns();
		if(ns == nil)
			sysfatal("no name space");
		defwsys = smprint("unix!%s/wsys", ns);
		wsysaddr = defwsys;
	}
	wsysfd = dial(wsysaddr, 0, 0, 0);
	if(wsysfd < 0)
		sysfatal("dial %s: %r", wsysaddr);
	if(wsysaddr == defwsys)
		free(wsysaddr);

	mbuf = nil;
	nmbuf = 0;
	io = ioproc();
	while((n = ioread(io, 3, buf, 4)) == 4){
		GET(buf, n);
		if(n > nmbuf){
			free(mbuf);
			mbuf = malloc(4+n);
			if(mbuf == nil)
				sysfatal("malloc: %r");
			nmbuf = n;
		}
		memmove(mbuf, buf, 4);
		nn = readn(3, mbuf+4, n-4);
		if(nn != n-4)
			sysfatal("eof during message");

		/* pick off messages one by one */
		if(convM2W(mbuf, nn+4, &m) <= 0)
			sysfatal("cannot convert message");
		if(chatty) fprint(2, "<- %W\n", &m);
		runmsg(&m);
	}
	threadexitsall(0);
}

void
replyerror(Wsysmsg *m)
{
	char err[256];
	
	rerrstr(err, sizeof err);
	m->type = Rerror;
	m->error = err;
	replymsg(m);
}


#define INC(a, i) BPLONG(a+i, BGLONG(a+i)+1)
/*
 * drawmsg increments the id of images in
 * devdraw messages so that id 1 can be used
 * for the main screen, instead of 0 as p9p's
 * libdraw expects.
 * The message size is returned.
 */
int
drawmsg(uchar *a)
{
	int i;

	switch(*a){
	case 'b':
		INC(a, 1);
		return 1+4+4+1+4+1+4*4+4*4+4;
	case 'A':
		INC(a, 5);
		INC(a, 9);
		return 1+4+4+4+1;
	case 'c':
		INC(a, 1);
		return 1+4+1+4*4;
	case 'd':
		INC(a, 1);
		INC(a, 5);
		INC(a, 9);
		return 1+4+4+4+4*4+2*4+2*4;
	case 'D':
		return 1+1;
	case 'e':
	case 'E':
		INC(a, 1);
		INC(a, 5);
		return 1+4+4+2*4+4+4+4+2*4+2*4;
	case 'f':
		INC(a, 1);
		return 1+4;
	case 'F':
		return 1+4;
	case 'i':
		INC(a, 1);
		return 1+4+4+1;
	case 'l':
		INC(a, 1);
		INC(a, 5);
		return 1+4+4+2+4*4+2*4+1+1;
	case 'L':
		INC(a, 1);
		return 1+4+2*4+2*4+4+4+4+4+2*4;
	case 'm':
		return 4+4;
	case 'n':
		INC(a, 1);
		return 1+4+1+a[5];
	case 'N':
		INC(a, 1);
		return 1+4+1+1+a[6];
	case 'o':
		INC(a, 1);
		return 1+4+2*4+2*4;
	case 'O':
		return 1+1;
	case 'p':
	case 'P':
		INC(a, 1);
		INC(a, 19);
		return 1+4+2+4+4+4+4+2*2*4+2*2*BGSHORT(a+5);;
	case 'r':
		INC(a, 1);
		return 1+4+4*4;
	case 's':
		INC(a, 1);
		INC(a, 5);
		INC(a, 9);
		return 1+4+4+4+2*4+4*4+2*4+2+BGSHORT(a+45)*2;
	case 'x':
		INC(a, 1);
		INC(a, 5);
		INC(a, 9);
		INC(a, 47);
		return 1+4+4+4+2*4+4*4+2*4+2+4+2*4+BGSHORT(a+45)*2;
	case 'S':
		return 1+4+4;
	case 't':
		for(i = 0; i < BGSHORT(a+2); i++)
			INC(a, 4+4*i);
		return 1+1+2+4*BGSHORT(a+2);
	case 'v':
		return 1;
	case 'y':
	case 'Y':
		INC(a, 1);
		return -1;
	}
	return -1;
}

/* 
 * Handle a single wsysmsg. 
 * Might queue for later (kbd, mouse read)
 */
void
runmsg(Wsysmsg *m)
{
	uchar buf[65536];
	uchar *a;
	int n;
	CFid *f;
	Rectangle r;
	static int border;
	static CFid *fcons, *fcursor, *fmouse, *fctl, *fdata, *frddraw;
	static CFsys *wsysfs;

	static int na;
	static uchar *aa;

	switch(m->type){
	case Tinit:
		sprint((char*)buf, "new %s", m->winsize);
		if((wsysfs = fsmount(wsysfd, (char*)buf)) == nil)
			sysfatal("fsmount: %r");

		fcons = fsopen(wsysfs, "cons", OREAD);
		fmouse = fsopen(wsysfs, "mouse", ORDWR);
		fcursor = fsopen(wsysfs, "cursor", OWRITE);

		f = fsopen(wsysfs, "label", OWRITE);
		fsprint(f, m->label);
		fsclose(f);

		/*
		 * Read keyboard and mouse events
		 * from different threads.
		 * Use a buffered chan as tag queue.
		 */
		kbdchan = chancreate(sizeof(uchar), 12);
		mousechan = chancreate(sizeof(uchar), 12);
		f= fsopen(wsysfs, "consctl", OWRITE);
		fsprint(f, "rawon");
		threadcreate(kbdthread, fcons, STACK);
		threadcreate(mousethread, fmouse, STACK);

		/*
		 * Open draw(3) files and register
		 * image named winname
		 */
		fctl = fsopen(wsysfs, "draw/new", ORDWR);
		fsread(fctl, buf, 12*12);
		n = atoi((char*)buf);
		sprint((char*)buf, "draw/%d/data", n);
		fdata = fsopen(wsysfs, (char*)buf, ORDWR);
		
		replymsg(m);
		break;

	case Trdmouse:
		if(nbsend(mousechan, &m->tag) == 0)
			sysfatal("too many queued mouse reads");
		break;

	case Trdkbd:
		if(nbsend(kbdchan, &m->tag) == 0)
			sysfatal("too many queued mouse reads");
		break;

	case Tmoveto:
		fsprint(fmouse, "m %11d %11d %11d %11d", m->mouse.xy.x, m->mouse.xy.y, m->mouse.buttons, m->mouse.msec);
		replymsg(m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			n = fswrite(fcursor, buf, 0);
		else{
			BPLONG(&buf[0], m->cursor.offset.x);
			BPLONG(&buf[4], m->cursor.offset.y);
			for(n = 0; n < 32; n++){
				buf[2*4+n] = m->cursor.clr[n];
				buf[2*4+32+n] = m->cursor.set[n];
			}
			n = fswrite(fcursor, buf, 72);
		}
		if(n < 0)
			replyerror(m);
		else
			replymsg(m);
		break;

	case Tbouncemouse:
		/* Ignore */
		replymsg(m);
		break;

	case Tlabel:
		f = fsopen(wsysfs, "label", OWRITE);
		fsprint(f, m->label);
		fsclose(f);
		replymsg(m);
		break;

	case Trdsnarf:
		f = fsopen(wsysfs, "snarf", OREAD);
		fsread(f, buf, sizeof buf);
		fsclose(f);
		m->snarf = strdup((char*)buf);
		replymsg(m);
		break;

	case Twrsnarf:
		f = fsopen(wsysfs, "snarf", OWRITE);
		fsprint(f, m->snarf);
		fsclose(f);
		replymsg(m);
		break;

	case Trddraw:
		if((n = fsread(frddraw, buf, sizeof buf)) < 0){
			replyerror(m);
			break;
		}
		/*
		 * If it is not a "noborder" image lie to p9p's libdraw saying
		 * that the image is smaller, to respect the window border.
		 */
		if(frddraw == fctl){
			if(border){
				r = Rect(
					atoi((char*)&buf[4*12]),
					atoi((char*)&buf[5*12]),
					atoi((char*)&buf[6*12]),
					atoi((char*)&buf[7*12])
				);
				r = insetrect(r, Borderwidth);
				sprint((char*)&buf[4*12], "%11d %11d %11d %11d",
					r.min.x, r.min.y, r.max.x, r.max.y);
			}
		}
		m->count = n;
		m->data = buf;
		frddraw = fdata;
		replymsg(m);
		break;

	case Twrdraw:
		/*
		 * In the drawfcall protocol:
		 * 	J: Install screen image as image 0
		 * 	I: get "screen" (image 0) information
		 *
		 * Instead, we read the screen image name
		 * from winname and associate it to the image
		 * with id 1 using the command 'n'.
		 */
		SET(n);
		if(m->count == 2 && m->data[0] == 'J' && m->data[1] == 'I'){
			f = fsopen(wsysfs, "winname", OREAD);
			buf[0] = 'n';
			n = fsread(f, &buf[1+4+1], 64);
			border = (strncmp((char*)&buf[1+4+1], "noborder", 8) != 0);
			buf[1+4] = n;
			fsclose(f);
			BPLONG(&buf[1], 1);
			n = fswrite(fdata, buf, 1+4+1+n);
			frddraw = fctl;
		} else {
			a = m->data;
			if(aa){
				n = drawmsg(aa);
				memmove(aa+na, m->data, n-na);
				a += n-na;
				if((n = fswrite(fdata, aa, n)) < 0)
					break;
				free(aa);
				aa = nil;
			}
			while((na = a-m->data) < m->count){
				if((n = drawmsg(a)) < 0)
					n = m->count - na;
				if(n > m->count - na){
					/* fprint(2, "Eshortdraw: %d < %d\n", a-m->data, n); */
					aa = malloc(n);
					memmove(aa, fdata, n);
					goto End;
				}
				if((n = fswrite(fdata, a, n)) < 0)
					break;
				a += n;
			}
		}
		if(n < 0)
			replyerror(m);
		else
			replymsg(m);
End:
		break;
	
	case Ttop:
		f = fsopen(wsysfs, "wctl", OWRITE);
		fsprint(f, "top");
		fsclose(f);
		replymsg(m);
		break;
	
	case Tresize:
		f = fsopen(wsysfs, "wctl", OWRITE);
		fsprint(f, "resize %d %d", Dx(m->rect), Dy(m->rect));
		fsclose(f);
		replymsg(m);
		break;
	}
}

/*
 * Reply to m.
 */
QLock replylock;
void
replymsg(Wsysmsg *m)
{
	int n;
	static uchar *mbuf;
	static int nmbuf;

	/* T -> R msg */
	if(m->type%2 == 0)
		m->type++;

	if(chatty) fprint(2, "-> %W\n", m);
	/* copy to output buffer */
	n = sizeW2M(m);

	qlock(&replylock);
	if(n > nmbuf){
		free(mbuf);
		mbuf = malloc(n);
		if(mbuf == nil)
			sysfatal("out of memory");
		nmbuf = n;
	}
	convW2M(m, mbuf, n);
	if(write(4, mbuf, n) != n)
		sysfatal("write: %r");
	qunlock(&replylock);
}

void
kbdthread(void *arg)
{
	CFid *f;
	Rune r;
	Wsysmsg m;

	f = arg;
	m.type = Rrdkbd;
	while(1){
		recv(kbdchan, &m.tag);
		r = 0;
		if(fsread(f, &r, 2) < 1)
			sysfatal("short keyboard read");
		m.rune = r;
		replymsg(&m);
	}
}

void
mousethread(void *arg)
{
	char buf[49];
	CFid *f;
	Wsysmsg m;

	f = arg;
	m.type = Rrdmouse;
	while(1){
		recv(mousechan, &m.tag);
		if(fsread(f, buf, 49) < 49)
			sysfatal("short mouse read");
		m.resized = 0;
		if(buf[0] == 'r')
			m.resized = 1;
		m.mouse.xy.x = atoi(buf+1+0*12);
		m.mouse.xy.y = atoi(buf+1+1*12);
		m.mouse.buttons = atoi(buf+1+2*12);
		m.mouse.msec = atoi(buf+1+3*12);
		replymsg(&m);
	}
}

