/* Constants */
enum {
	/* Qroot is #defined 0 in ninepserver.h */

	/* Root */
	Qkill = 1,
	Qdraw,
	Qwsys,

	/* Window */
	Qlabel,
	Qwctl,
	Qwinid,
	Qwinname,
	/*	Keyboard */
	Qcons,
	Qconsctl,
	Qkeyboard,
	/*	Mouse */
	Qcursor,
	Qmouse,
	Qpointer,
	Qsnarf,
	/*	draw/ */
	Qnew,

	/* Draw clientt */
	/*	draw/n/ */
	Qdrawn,
	Qctl,
	Qdata,
	Qcolormap,
	Qrefresh,
	Qwindow,
};

/* Error messages */
extern char
	Edeleted[],
	Einterrupted[],
	Einuse[],
	Enofile[],
	Enodrawimage[],
	Enoperm[],
	Eshortread[];

/* Macros */
#define PATH(s, t)	((Path)((s)<<8)|((t)&0xFF))
#define QSLOT(p)	((p)>>8)
#define QTYPE(p)	((p)&0xFF)

void fsinit(Ninepserver*);
Window* qwindow(Qid*);

char *eve;
Ninepops ops;
Ninepserver *server;

/* Devdraw */
DClient* drawnewclient(Draw*);
char* drawopen(Qid *qid, int mode);
char* drawread(Qid qid, char *buf, ulong *n, vlong offset);
char* drawwrite(Qid qid, char *buf, ulong *n, vlong offset);
char* drawclose(Qid qid);
Window* drawwindow(int id);
int drawpath(DClient *cl);
