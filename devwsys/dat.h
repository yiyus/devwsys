enum
{
	Debug9p = 1,
	Debugdraw = 2,
	Debugevent = 4,
};

enum
{
	PMundef = ~0,			/* Pixmap undefined */
	CursorSize= 32,		/* Biggest cursor size */
	SnarfSize= 100*1024,	/* snarf buffer size */
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
	Kdown=	KF|0x800,
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
typedef struct Draw Draw;	/* Defined in devdraw.c: */
typedef struct DClient DClient;	/* Defined in devdraw.c: */
typedef struct Kbdbuf Kbdbuf;
typedef struct Cursor Cursor;
typedef struct Clip Clip;
typedef struct Mousebuf Mousebuf;
typedef struct Reqbuf Reqbuf;
typedef struct Window Window;

/* Keyboard */
struct Kbdbuf
{
	Rune	r[32];
	int	ri;
	int	wi;
	int	stall;
	/* Used by kbdputc */
	uchar k[5*UTFmax];
	int	alting, nk;
};

/* Pointer */
struct  Mouse
{
	int		buttons;	/* bit array: LMR=124 */
	Point	xy;
	ulong	msec;
};

struct Mousebuf
{
	int open;
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

struct Cursor {
	int	hotx;
	int	hoty;
	int	w;
	int	h;
	uchar	src[(CursorSize/8)*CursorSize];	/* image and mask bitmaps */
	uchar	mask[(CursorSize/8)*CursorSize];
};

/* snarf buffer */
struct Clip
{
	char buf[SnarfSize];
};

/* Window system */
struct Window
{
	int			id;
	int			fullscreen;
	int			deleted;
	int			pid;
	int			resized;
	int			current;
	int			visible;
	int			ref;
	char			*label;
	char			*name;
	Draw		*draw;
	void			*screenimage;
	Kbdbuf		kbd;
	Mousebuf		mouse;
	Reqbuf		kbdreqs;
	Reqbuf		mousereqs;
	Cursor		cursor;
	Point		orig;
	Rectangle		screenr;
	Rectangle		newscreenr;
	void			*x;
};

/* Global Vars */
int		debug;
int		nwindow;
Window	**window;
int		nclient;
DClient	**client;
Clip clip;

