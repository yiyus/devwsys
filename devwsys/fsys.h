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

/*
 * TODO: These functions should be standard device
 * file functions taken directly from Inferno devices.
 */

/* Devdraw */
const char* drawopen(Client*, uint);
Client* drawnewclient(Draw*);
Window* drawwindow(Client*);
int drawclientid(Client*);
IOResponse drawread(Client*, int, char*, int);
IOResponse drawwrite(Client*, int, char*, int);
void drawclose(Client*, int);
