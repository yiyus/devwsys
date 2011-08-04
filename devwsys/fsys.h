/* Constants */
enum {
	/* Dirs */
	FsRoot,
	FsDDraw,
	FsDDrawn,
	FsDWsys,
	FsDWsysn,

	/* Files */
	/*	Keyboard */
	FsFCons,
	FsFConsctl,
	/*	Mouse */
	FsFCursor,
	FsFMouse,
	FsFSnarf,
	/*	Window */
	FsFKill,
	FsFLabel,
	FsFWctl,
	FsFWinid,
	FsFWinname,
	/*	draw/ */
	FsFNew,
	/*	draw/n/ */
	FsFCtl,
	FsFData,
	FsFColormap,
	FsFRefresh,
};

enum {
	/* Dirs */
	Qroot,
	Qdraw,
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
	*Ebadarg,
	*Edeleted,
	*Einterrupted,
	*Einuse,
	*Enodev,
	*Enofile,
	*Enodrawimage,
	*Enomem,
	*Enoperm,
	*Eshortread;

/* Macros */
#define incref(r)	((*r) = ++(*r))
#define decref(r)	((*r) = --(*r))
#define iswindow(t) ((t) == FsRoot || (t) > FsDDrawn && (t) < FsFCtl)

/* styxfsys */
//void fsysinit(Styxserver*);

//Styxops ops;
//Styxserver *server;

/*
 * TODO: These functions should be standard device
 * file functions taken directly from Inferno devices.
 */

/* Devdraw */
const char* drawopen(DClient*, uint);
DClient* drawnewclient(Draw*);
Window* drawwindow(DClient*);
int drawclientid(DClient*);
IOResponse drawread(DClient*, int, char*, int);
IOResponse drawwrite(DClient*, int, char*, int);
void drawclose(DClient*, int);
