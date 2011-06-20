/* Drawfcall message types */
enum {
	Rerror = 1,
	Trdmouse = 2,
	Rrdmouse,
	Tmoveto = 4,
	Rmoveto,
	Tcursor = 6,
	Rcursor,
	Tbouncemouse = 8,
	Rbouncemouse,
	Trdkbd = 10,
	Rrdkbd,
	Tlabel = 12,
	Rlabel,
	Tinit = 14,
	Rinit,
	Trdsnarf = 16,
	Rrdsnarf,
	Twrsnarf = 18,
	Rwrsnarf,
	Trddraw = 20,
	Rrddraw,
	Twrdraw = 22,
	Rwrdraw,
	Ttop = 24,
	Rtop,
	Tresize = 26,
	Rresize,
	Tmax,
};

/* Runes */
enum {
	KF=	0xF000,	/* Beginning of private space */
	/* KF|1, KF|2, ..., KF|0xC is F1, F2, ..., F12 */
	Khome=	KF|0x0D,
	Kup=	KF|0x0E,
	Kpgup=	KF|0x0F,
	Kprint=	KF|0x10,
	Kleft=	KF|0x11,
	Kright=	KF|0x12,
	Kdown=	0x80,
	Kview=	0x80,
	Kpgdown=KF|0x13,
	Kins=	KF|0x14,
	Kend=	KF|0x18,

	Kalt=	KF|0x15,
	Kshift=	KF|0x16,
	Kctl=		KF|0x17,
	
	Kcmd=	0xF100	/* Beginning of Cmd+'a', Cmd+'A', etc on Mac */
};

/* Datatypes: */
typedef struct Kbdbuf Kbdbuf;
typedef struct Mousebuf Mousebuf;
typedef struct Tagbuf Tagbuf;
typedef struct Window Window;
typedef struct Wsysmsg Wsysmsg;

struct Kbdbuf
{
	Rune r[32];
	int ri;
	int wi;
	int stall;
	/* Used by kbdputc */
	uchar k[5*UTFmax];
	int alting, nk;
};

struct  Mouse
{
	int		buttons;	/* bit array: LMR=124 */
	Point	xy;
	ulong	msec;
};

struct Mousebuf
{
	Mouse m[32];
	int ri;
	int wi;
	int stall;
	int resized;
};

struct Tagbuf
{
	int t[32];
	void *r[32];
	int ri;
	int wi;
};

struct Window
{
	int id;
	int deleted;
	int mouseopen;
	char *label;
	Kbdbuf kbd;
	Memimage *img;
	Mousebuf mouse;
	Tagbuf kbdtags;
	Tagbuf mousetags;
	Rectangle r;
	void *x;
};

struct Wsysmsg
{
	uchar type;
	uchar tag;
	Mouse mouse;
	int resized;
//	Cursor cursor;
//	int arrowcursor;
	Rune rune;
	char *winsize;
	char *label;
//	char *snarf;
//	char *error;
//	uchar *data;
//	uint count;
//	Rectangle rect;
	void *v;
};

/* Global Vars */
int debuglevel;
int nwindow;
Window **window;
