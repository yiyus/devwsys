#define fatal(...) sysfatal("fatal: " __VA_ARGS__)

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
void cursorwrite(Window*, char*, int);

/* Reqbuf */
void addreq(Reqbuf*, void*);
void* nextreq(Reqbuf *reqs);

/* Window */
Window* newwin(void);
Window* winlookup(int);
void deletewin(Window*);
void setlabel(Window*, char*);

/* Wctl */
char* wctlmesg(Window*, char*, int);

/* Fsys */
char* fsloop(char*, int);
int killreply(Window*);
void readreply(void*, char*);
void fsdelete(Window*);

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
void xbottomwindow(Window*);
void xtopwindow(Window*);
void xcurrentwindow(Window*);
void xresizewindow(Window*, Rectangle);
void xsetcursor(Window*);
void xsetmouse(Window*, Point);
char* xgetsnarf(void);
void xputsnarf(char*);
