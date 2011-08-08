/* Constants */
enum {
	/* Dirs */
	/* Qroot is defined in ninepserver.h */
	Qdraw = 1,
	Qwsys,

	/* Files */
	/*	Keyboard */
	Qcons,
	Qconsctl,
	/*	Mouse */
	Qcursor,
	Qmouse,
	Qsnarf,
	/*	Window */
	Qkill,
	Qlabel,
	Qwctl,
	Qwinid,
	Qwinname,
	/*	draw/ */
	Qnew,
	/*	draw/n/ */
	Qctl,
	Qdata,
	Qcolormap,
	Qrefresh,
};

/* Read response */
typedef struct IOResponse IOResponse;
struct IOResponse
{
	char * data;
	int count;
	const char *err;
};

/* Error messages */
const char
	*Edeleted,
	*Einterrupted,
	*Einuse,
	*Enofile,
	*Enodrawimage,
	*Enoperm,
	*Eshortread;

/* Macros */
#define incref(r)	((*r) = ++(*r))
#define decref(r)	((*r) = --(*r))
#define curwindow	((Window*)server->curc->u)
#define curdraw	(((Window*)server->curc->u)->draw)
#define iswindow(t) ((t) == FsRoot || (t) > FsDDrawn && (t) < FsFCtl)

void fsinit(Ninepserver*);

Ninepops ops;
Ninepserver *server;

/*
 * TODO: These functions should be standard device
 * file functions taken directly from Inferno devices.
 */

/* Devdraw */
char* drawopen(Qid *qid, int mode);
char* drawread(Qid qid, char *buf, ulong *n, vlong offset);
char* drawwrite(Qid qid, char *buf, ulong *n, vlong offset);
DClient* drawnewclient(Draw*);
void drawclose(DClient*, int);
