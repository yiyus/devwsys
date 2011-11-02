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

#define STACK (32*1024)

void runmsg(Wsysmsg*);
void replymsg(Wsysmsg*);
void kbdthread(void*);
void mousethread(void*);

int chatty = 0;
int drawsleep;
int fd;

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
	char *addr, *defaddr, *ns;
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
	addr = getenv("WSYS");
	defaddr = nil;
	if(addr == 0){
		ns = getns();
		if(ns == nil)
			sysfatal("no name space");
		defaddr = smprint("unix!%s/wsys", ns);
		addr = defaddr;
	}
	fd = dial(addr, 0, 0, 0);
	if(fd < 0)
		sysfatal("dial %s: %r", addr);
	if(addr == defaddr)
		free(addr);

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


/* 
 * Handle a single wsysmsg. 
 * Might queue for later (kbd, mouse read)
 */
void
runmsg(Wsysmsg *m)
{
	uchar buf[65536];
	int n;
	CFid *f;
	Rectangle r;
	static int border;
	static int id = -1;
	static CFid *fcons, *fmouse, *fctl, *fdata, *frddraw;
	static CFsys *fsys;

	switch(m->type){
	case Tinit:
		sprint((char*)buf, "new %s", m->winsize);
		if((fsys = fsmount(fd, (char*)buf)) == nil)
			sysfatal("fsmount: %r");

		fcons = fsopen(fsys, "cons", OREAD);
		fmouse = fsopen(fsys, "mouse", ORDWR);

		f = fsopen(fsys, "label", OWRITE);
		fsprint(f, m->label);
		fsclose(f);

		/*
		 * Read keyboard and mouse events
		 * from different threads.
		 * Use a buffered chan as tag queue.
		 */
		kbdchan = chancreate(sizeof(uchar), 12);
		mousechan = chancreate(sizeof(uchar), 12);
		f= fsopen(fsys, "consctl", OWRITE);
		fsprint(f, "rawon");
		threadcreate(kbdthread, fcons, STACK);
		threadcreate(mousethread, fmouse, STACK);

		/*
		 * Open draw(3) files and register
		 * image named winname
		 */
		fctl = fsopen(fsys, "draw/new", ORDWR);
		fsread(fctl, buf, 12*12);
		n = atoi((char*)buf);
		sprint((char*)buf, "draw/%d/data", n);
		fdata = fsopen(fsys, (char*)buf, ORDWR);
		
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
		f = fsopen(fsys, "cursor", OWRITE);
		if(m->arrowcursor)
			fswrite(f, buf, 0);
		else{
			PUT(&buf[0], m->cursor.offset.x);
			PUT(&buf[4], m->cursor.offset.y);
			for(n = 0; n < 32; n++){
				buf[2*4+n] = m->cursor.clr[n];
				buf[2*4+32+n] = m->cursor.set[n];
			}
			fswrite(f, buf, 72);
		}
		fsclose(f);
		replymsg(m);
		break;

	case Tbouncemouse:
		/* Ignore */
		replymsg(m);
		break;

	case Tlabel:
		f = fsopen(fsys, "label", OWRITE);
		fsprint(f, m->label);
		fsclose(f);
		replymsg(m);
		break;

	case Trdsnarf:
		f = fsopen(fsys, "snarf", OREAD);
		fsread(f, buf, sizeof buf);
		fsclose(f);
		m->snarf = strdup((char*)buf);
		replymsg(m);
		break;

	case Twrsnarf:
		f = fsopen(fsys, "snarf", OWRITE);
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
		 * from winname and associate it to a devdraw
		 * image with the command 'n', which we
		 * will free after ctl is read.
		 */
		SET(n);
		if(m->count == 2 && m->data[0] == 'J' && m->data[1] == 'I'){
			id = 1;
			buf[0] = 'f';
			BPLONG(&buf[1], id);
			fswrite(fdata, buf, 1+4);
			f = fsopen(fsys, "winname", OREAD);
			buf[0] = 'n';
			n = fsread(f, &buf[1+4+1], 64);
			border = (strncmp((char*)&buf[1+4+1], "noborder", 8) != 0);
			buf[1+4] = n;
			fsclose(f);
			BPLONG(&buf[1], id);
			n = fswrite(fdata, buf, 1+4+1+n);
			frddraw = fctl;
		} else
			n = fswrite(fdata, m->data, m->count);
		if(n < 0)
			replyerror(m);
		else
			replymsg(m);
		break;
	
	case Ttop:
		f = fsopen(fsys, "wctl", OWRITE);
		fsprint(f, "top");
		fsclose(f);
		replymsg(m);
		break;
	
	case Tresize:
		f = fsopen(fsys, "wctl", OWRITE);
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

