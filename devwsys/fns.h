#define fatal(...) sysfatal("devwsys: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "devwsys: " __VA_ARGS__)

/* Devdraw */
int drawattach(Window*, char*);
void drawdettach(Window*);
void drawreplacescreenimage(Window*);

/* Keyboard */
void writekbd(Window*, int);
void readkbd(Window*, void*);

/* Mouse */
void readmouse(Window*, void*);
void writemouse(Window*, Mouse, int);
int cursorwrite(Window*, char*, int);

/* Reqbuf */
void addreq(Reqbuf*, void*);
void* nextreq(Reqbuf *reqs);

/* Window */
Window* newwin(void);
Window* lookupwin(int);
void deletewin(Window*);
void setlabel(Window*, char*);

/* Wctl */
int wctlmesg(Window*, char*, int, char*);

/* Fsys */
void fsloop(char*, int);
void killrespond(Window*);
void readreply(void*, char*);

/* X connection */
int xinit(void);
void xnextevent(void);
void xclose(void);

/* X window */
int xattach(Window*, char*);
void xdeletewin(Window*);
void xreplacescreenimage(Window*);
void xflushmemscreen(Window*, Rectangle);
int xupdatelabel(Window*);
void xtopwindow(Window*);
void xresizewindow(Window*, Rectangle);
void xsetcursor(Window*);
void xsetmouse(Window*, Point);
char* xgetsnarf();
void xputsnarf(char*);
