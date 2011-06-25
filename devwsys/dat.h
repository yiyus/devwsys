/* Pixmap undefined */
enum
{
	PMundef = ~0
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
typedef struct Draw Draw;
typedef struct Client Client;
typedef struct FChar FChar;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct Kbdbuf Kbdbuf;
typedef struct Mousebuf Mousebuf;
typedef struct Reqbuf Reqbuf;
typedef struct Window Window;
typedef struct Wsysmsg Wsysmsg;

/* Drawing device */
#define	NHASH		(1<<5)
#define	HASHMASK	(NHASH-1)

struct Draw
{
	Rectangle	flushrect;
};

struct Client
{
	/*Ref		r;*/
	DImage*		dimage[NHASH];
//	CScreen*	cscreen;
//	Refresh*	refresh;
//	Rendez		refrend;
	Window	*window;
//	uchar*		readdata;
//	int		nreaddata;
	int		busy;
	int		clientid;
	int		slot;
//	int		refreshme;
//	int		infoid;
	int		op;
};

struct FChar
{
	int		minx;	/* left edge of bits */
	int		maxx;	/* right edge of bits */
	uchar		miny;	/* first non-zero scan-line */
	uchar		maxy;	/* last non-zero scan-line + 1 */
	schar		left;	/* offset of baseline */
	uchar		width;	/* width of baseline */
};

struct DImage
{
	int		id;
	int		ref;
	char		*name;
	int		vers;
	Memimage*	image;
	int		ascent;
	int		nfchar;
	FChar*		fchar;
	DScreen*	dscreen;	/* 0 if not a window */
	DImage*	fromname;	/* image this one is derived from, by name */
	DImage*		next;
};

struct DScreen
{
	int		id;
	int		public;
	int		ref;
	DImage	*dimage;
	DImage	*dfill;
	Memscreen*	screen;
	Client*		owner;
	DScreen*	next;
};

/* Events */
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

struct Reqbuf
{
	void *r[32];
	int ri;
	int wi;
};

/* Window system */
struct Window
{
	int id;
	int deleted;
	int mouseopen;
	char *label;
	Draw draw;
	Kbdbuf kbd;
	Memimage *screenimage;
	Mousebuf mouse;
	Reqbuf kbdreqs;
	Reqbuf mousereqs;
	Rectangle r;
	void *x;
};

/* Global Vars */
int debuglevel;
int nwindow;
Window **window;
int nclient;
Client** client;

