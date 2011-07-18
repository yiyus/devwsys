#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

void xclose(void);

static void	plan9cmap(void);
static int	setupcmap(XWindow);

Xconn xconn;

static int
xerror(XDisplay *d, XErrorEvent *e)
{
	char buf[200];

	if(e->request_code == 42) /* XSetInputFocus */
		return 0;
	if(e->request_code == 18) /* XChangeProperty */
		return 0;
	/*
	 * BadDrawable happens in apps that get resized a LOT,
	 * e.g. when KDE is configured to resize continuously
	 * during a window drag.
	 */
	if(e->error_code == 9) /* BadDrawable */
		return 0;

	fprint(2, "X error: error_code=%d, request_code=%d, minor=%d disp=%p\n",
		e->error_code, e->request_code, e->minor_code, d);
	XGetErrorText(d, e->error_code, buf, sizeof buf);
	fprint(2, "%s\n", buf);
	return 0;
}

static int
xioerror(XDisplay *d)
{
	/*print("X I/O error\n"); */
	exit(0);
	/*sysfatal("X I/O error\n");*/
	abort();
	return -1;
}

int
xfd(void)
{
	return xconn.fd;
}

/*
 * Connect to the X server.
 */
int
xinit(void)
{
	char *disp;
	int i, n, xrootid;
	XPixmapFormatValues *pfmt;
	XVisualInfo xvi;

	if(xconn.display != nil) /* already connected */
		return 0;

	/*
	 * Connect to X server.
	 */
	xconn.display = XOpenDisplay(NULL);
	if(xconn.display == nil){
		disp = getenv("DISPLAY");
		werrstr("XOpenDisplay %s: %r", disp ? disp : ":0");
		return -1;
	}
	xconn.fd = ConnectionNumber(xconn.display);
	XSetErrorHandler(xerror);
	XSetIOErrorHandler(xioerror);
	xrootid = DefaultScreen(xconn.display);
	xconn.root = DefaultRootWindow(xconn.display);

	/* 
	 * Figure out underlying screen format.
	 */
	if(XMatchVisualInfo(xconn.display, xrootid, 16, TrueColor, &xvi)
	|| XMatchVisualInfo(xconn.display, xrootid, 16, DirectColor, &xvi)){
		xconn.vis = xvi.visual;
		xconn.depth = 16;
	}
	else
	if(XMatchVisualInfo(xconn.display, xrootid, 15, TrueColor, &xvi)
	|| XMatchVisualInfo(xconn.display, xrootid, 15, DirectColor, &xvi)){
		xconn.vis = xvi.visual;
		xconn.depth = 15;
	}
	else
	if(XMatchVisualInfo(xconn.display, xrootid, 24, TrueColor, &xvi)
	|| XMatchVisualInfo(xconn.display, xrootid, 24, DirectColor, &xvi)){
		xconn.vis = xvi.visual;
		xconn.depth = 24;
	}
	else
	if(XMatchVisualInfo(xconn.display, xrootid, 8, PseudoColor, &xvi)
	|| XMatchVisualInfo(xconn.display, xrootid, 8, StaticColor, &xvi)){
		if(xconn.depth > 8){
			werrstr("can't deal with colormapped depth %d screens",
				xconn.depth);
			xclose();
			return -1;
		}
		xconn.vis = xvi.visual;
		xconn.depth = 8;
	}
	else{
		xconn.depth = DefaultDepth(xconn.display, xrootid);
		if(xconn.depth != 8){
			werrstr("can't understand depth %d screen", xconn.depth);
			xclose();
			return -1;
		}
		xconn.vis = DefaultVisual(xconn.display, xrootid);
	}

	if(DefaultDepth(xconn.display, xrootid) == xconn.depth)
		xconn.usetable = 1;

	/*
	 * xconn.depth is only the number of significant pixel bits,
	 * not the total number of pixel bits.  We need to walk the
	 * display list to find how many actual bits are used
	 * per pixel.
	 */
	xconn.chan = 0;
	pfmt = XListPixmapFormats(xconn.display, &n);
	for(i=0; i<n; i++){
		if(pfmt[i].depth == xconn.depth){
			switch(pfmt[i].bits_per_pixel){
			case 1:	/* untested */
				xconn.chan = GREY1;
				break;
			case 2:	/* untested */
				xconn.chan = GREY2;
				break;
			case 4:	/* untested */
				xconn.chan = GREY4;
				break;
			case 8:
				xconn.chan = CMAP8;
				break;
			case 15:
				xconn.chan = RGB15;
				break;
			case 16: /* how to tell RGB15? */
				xconn.chan = RGB16;
				break;
			case 24: /* untested (impossible?) */
				xconn.chan = RGB24;
				break;
			case 32:
				xconn.chan = XRGB32;
				break;
			}
		}
	}
	if(xconn.chan == 0){
		werrstr("could not determine screen pixel format");
		xclose();
		return -1;
	}

	/*
	 * Set up color map if necessary.
	 */
	xconn.screen = DefaultScreenOfDisplay(xconn.display);
	xconn.cmap = DefaultColormapOfScreen(xconn.screen);
	if(xconn.vis->class != StaticColor){
		plan9cmap();
		setupcmap(xconn.root);
	}
	xconn.screenrect = Rect(0, 0, WidthOfScreen(xconn.screen), HeightOfScreen(xconn.screen));

	xinitclipboard();

	return 0;
}

/*
 * Initialize map with the Plan 9 rgbv color map.
 */
static void
plan9cmap(void)
{
	int r, g, b, cr, cg, cb, v, num, den, idx, v7, idx7;
	static int once;

	if(once)
		return;
	once = 1;

	for(r=0; r!=4; r++)
	for(g = 0; g != 4; g++)
	for(b = 0; b!=4; b++)
	for(v = 0; v!=4; v++){
		den=r;
		if(g > den)
			den=g;
		if(b > den)
			den=b;
		/* divide check -- pick grey shades */
		if(den==0)
			cr=cg=cb=v*17;
		else {
			num=17*(4*den+v);
			cr=r*num/den;
			cg=g*num/den;
			cb=b*num/den;
		}
		idx = r*64 + v*16 + ((g*4 + b + v - r) & 15);
		xconn.map[idx].red = cr*0x0101;
		xconn.map[idx].green = cg*0x0101;
		xconn.map[idx].blue = cb*0x0101;
		xconn.map[idx].pixel = idx;
		xconn.map[idx].flags = DoRed|DoGreen|DoBlue;

		v7 = v >> 1;
		idx7 = r*32 + v7*16 + g*4 + b;
		if((v & 1) == v7){
			xconn.map7to8[idx7][0] = idx;
			if(den == 0) { 		/* divide check -- pick grey shades */
				cr = ((255.0/7.0)*v7)+0.5;
				cg = cr;
				cb = cr;
			}
			else {
				num=17*15*(4*den+v7*2)/14;
				cr=r*num/den;
				cg=g*num/den;
				cb=b*num/den;
			}
			xconn.map7[idx7].red = cr*0x0101;
			xconn.map7[idx7].green = cg*0x0101;
			xconn.map7[idx7].blue = cb*0x0101;
			xconn.map7[idx7].pixel = idx7;
			xconn.map7[idx7].flags = DoRed|DoGreen|DoBlue;
		}
		else
			xconn.map7to8[idx7][1] = idx;
	}
}

/*
 * Initialize and install the rgbv color map as a private color map
 * for this application.  It gets the best colors when it has the
 * cursor focus.
 *
 * We always choose the best depth possible, but that might not
 * be the default depth.  On such "suboptimal" systems, we have to allocate an
 * empty color map anyway, according to Axel Belinfante.
 */
static int 
setupcmap(XWindow w)
{
	char buf[30];
	int i;
	u32int p, pp;
	XColor c;

	if(xconn.depth <= 1)
		return 0;

	if(xconn.depth >= 24) {
		if(xconn.usetable == 0)
			xconn.cmap = XCreateColormap(xconn.display, w, xconn.vis, AllocNone); 

		/*
		 * The pixel value returned from XGetPixel needs to
		 * be converted to RGB so we can call rgb2cmap()
		 * to translate between 24 bit X and our color. Unfortunately,
		 * the return value appears to be display server endian 
		 * dependant. Therefore, we run some heuristics to later
		 * determine how to mask the int value correctly.
		 * Yeah, I know we can look at xconn.vis->byte_order but 
		 * some displays say MSB even though they run on LSB.
		 * Besides, this is more anal.
		 */
		c = xconn.map[19];	/* known to have different R, G, B values */
		if(!XAllocColor(xconn.display, xconn.cmap, &c)){
			werrstr("XAllocColor: %r");
			return -1;
		}
		p  = c.pixel;
		pp = rgb2cmap((p>>16)&0xff,(p>>8)&0xff,p&0xff);
		if(pp != xconn.map[19].pixel) {
			/* check if endian is other way */
			pp = rgb2cmap(p&0xff,(p>>8)&0xff,(p>>16)&0xff);
			if(pp != xconn.map[19].pixel){
				werrstr("cannot detect X server byte order");
				return -1;
			}

			switch(xconn.chan){
			case RGB24:
				xconn.chan = BGR24;
				break;
			case XRGB32:
				xconn.chan = XBGR32;
				break;
			default:
				werrstr("cannot byteswap channel %s",
					chantostr(buf, xconn.chan));
				break;
			}
		}
	}else if(xconn.vis->class == TrueColor || xconn.vis->class == DirectColor){
		/*
		 * Do nothing.  We have no way to express a
		 * mixed-endian 16-bit screen, so pretend they don't exist.
		 */
		if(xconn.usetable == 0)
			xconn.cmap = XCreateColormap(xconn.display, w, xconn.vis, AllocNone);
	}else if(xconn.vis->class == PseudoColor){
		if(xconn.usetable == 0){
			xconn.cmap = XCreateColormap(xconn.display, w, xconn.vis, AllocAll); 
			XStoreColors(xconn.display, xconn.cmap, xconn.map, 256);
			for(i = 0; i < 256; i++){
				xconn.tox11[i] = i;
				xconn.toplan9[i] = i;
			}
		}else{
			for(i = 0; i < 128; i++){
				c = xconn.map7[i];
				if(!XAllocColor(xconn.display, xconn.cmap, &c)){
					werrstr("can't allocate colors in 7-bit map");
					return -1;
				}
				xconn.tox11[xconn.map7to8[i][0]] = c.pixel;
				xconn.tox11[xconn.map7to8[i][1]] = c.pixel;
				xconn.toplan9[c.pixel] = xconn.map7to8[i][0];
			}
		}
	}else{
		werrstr("unsupported visual class %d", xconn.vis->class);
		return -1;
	}
	return 0;
}

void
xclose(void)
{
	/*
	 * Should do a better job of cleaning up here.
	 */
	XCloseDisplay(xconn.display);
}
